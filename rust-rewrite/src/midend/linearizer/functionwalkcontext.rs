use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{
        ir,
        linearizer::*,
        symtab::{self, DefContext},
        types,
    },
    trace,
};
use std::collections::HashMap;

#[derive(Debug, PartialEq, Eq)]
pub enum BranchError {
    NotBranched, // not branched but expected a branch
    Convergence(ConvergenceError),
    NotDone(usize),
    ExistingFalseBlock(usize, usize), // branch already exists (from_block, false_block)
    MissingFalseBlock(usize),         // missing false block on branch (from_label)
    ScopeHandling,
}
impl From<ConvergenceError> for BranchError {
    fn from(e: ConvergenceError) -> Self {
        Self::Convergence(e)
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum LoopError {
    NotLooping,
    Convergence(ConvergenceError),
    LoopBottomNotDone(usize), // if the loop bottom convergence returns as "not done", the label
    // which the BlockConvergences says we expect to converge to
    LoopInsideNotDone(usize), // same as LoopBottomNotDone but for the loop body itself
}
impl From<ConvergenceError> for LoopError {
    fn from(e: ConvergenceError) -> Self {
        Self::Convergence(e)
    }
}

pub struct FunctionWalkContext<'a> {
    symtab: &'a mut symtab::SymbolTable,
    def_path: symtab::DefPath,
    self_type: Option<types::Type>,
    prototype: symtab::FunctionPrototype,
    block_manager: BlockManager,
    relative_local_def_path: symtab::DefPath,
    open_loop_beginnings: Vec<usize>,
    // conditional branches from source block to the branch_false block.
    // creating a branch replaces current_block with the branch_tru
    // e block,
    // but we also need to track the branch_false block if it exists
    open_branch_false_blocks: HashMap<usize, ir::BasicBlock>,
    // convergences which currently exist for blocks
    open_convergences: BlockConvergences,
    // branch path of basic block labels targeted by the branches which got us to current_block
    open_branch_path: Vec<usize>,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
    current_block: ir::BasicBlock,
}

impl<'a> FunctionWalkContext<'a> {
    pub fn new(
        parent_context: &'a mut symtab::BasicDefContext,
        prototype: symtab::FunctionPrototype,
        self_type: Option<types::Type>,
    ) -> Self {
        trace::trace!("Create function walk context for {}", prototype);
        let (control_flow, start_block, end_block) = BlockManager::new();
        let mut base_convergences = BlockConvergences::new();
        base_convergences
            .add(&[start_block.label], end_block)
            .unwrap();

        let def_path = parent_context.definition_path();
        Self {
            symtab: parent_context.symtab_mut(),
            def_path,
            prototype,
            self_type,
            block_manager: control_flow,
            open_branch_false_blocks: HashMap::new(),
            open_convergences: base_convergences,
            open_branch_path: Vec::new(),
            relative_local_def_path: symtab::DefPath::new(),
            open_loop_beginnings: Vec::new(),
            temp_num: 0,
            current_block: start_block,
        }
    }

    fn new_subscope(&mut self) {
        self.relative_local_def_path
            .push(symtab::DefPathComponent::Scope(
                self.symtab.next_subscope(self.absolute_local_def_path()),
            ));
    }

    fn pop_current_scope_to_subscope_of_next(&mut self) -> Result<(), BranchError> {
        trace::trace!("pop current scope to subscope of next");
        match self.scope_stack.pop() {
            Some(next_scope) => {
                let old_scope = std::mem::replace(&mut self.current_scope, next_scope);
                self.current_scope.insert_scope(old_scope);
                Ok(())
            }
            None => Err(BranchError::ScopeHandling),
        }
    }

    fn replace_current_block(&mut self, new_current: ir::BasicBlock) -> ir::BasicBlock {
        trace::trace!(
            "replace current block ({}) with block {}",
            self.current_block.label,
            new_current.label
        );
        std::mem::replace(&mut self.current_block, new_current)
    }

    pub fn finish_true_branch_switch_to_false(&mut self) -> Result<(), BranchError> {
        trace::debug!("finish true branch, switch to false");

        let branched_from = match self.open_branch_path.last() {
            Some(branched_from) => branched_from,
            None => return Err(BranchError::NotBranched),
        };

        match self.open_convergences.converge(self.current_block.label)? {
            ConvergenceResult::NotDone(converge_to_label) => {
                let false_block = match self.open_branch_false_blocks.remove(branched_from) {
                    Some(false_block) => false_block,
                    None => return Err(BranchError::MissingFalseBlock(*branched_from)),
                };

                let convergence_jump = ir::IrLine::new_jump(
                    SourceLoc::none(),
                    converge_to_label,
                    ir::JumpCondition::Unconditional,
                );

                let mut converged_from = self.replace_current_block(false_block);
                converged_from.statements.push(convergence_jump);
                self.current_scope.insert_basic_block(converged_from);

                Ok(())
            }
            ConvergenceResult::Done(_block) => Err(BranchError::MissingFalseBlock(*branched_from)),
        }?;

        self.pop_current_scope_to_subscope_of_next()?;
        self.new_subscope();
        Ok(())
    }

    pub fn finish_branch(&mut self) -> Result<(), BranchError> {
        let branched_from = self.open_branch_path.last().unwrap();
        trace::debug!("finish branch from block {}", branched_from);

        match self.open_convergences.converge(self.current_block.label)? {
            ConvergenceResult::Done(converge_to_block) => {
                let convergence_jump = ir::IrLine::new_jump(
                    SourceLoc::none(),
                    converge_to_block.label,
                    ir::JumpCondition::Unconditional,
                );

                let mut converged_from = self.replace_current_block(converge_to_block);
                converged_from.statements.push(convergence_jump);
                self.current_scope.insert_basic_block(converged_from);

                match self.pop_current_scope_to_subscope_of_next() {
                    Ok(_) => Ok(()),
                    Err(_) => Err(BranchError::ScopeHandling),
                }
            }
            ConvergenceResult::NotDone(label) => Err(BranchError::NotDone(label)),
        }?;

        self.open_branch_path.pop().unwrap();
        Ok(())
    }

    // create an unconditional branch from the current block, transparently setting the current
    // block to the target. Inserts the current block (before call) into the current scope
    pub fn unconditional_branch_from_current(&mut self, loc: SourceLoc) -> Result<(), BranchError> {
        trace::debug!("create unconditional branch from current block");

        let branch_from = &mut self.current_block;
        let (branch_target, after_branch) = self
            .block_manager
            .create_unconditional_branch(branch_from, loc);

        self.open_convergences
            .rename_source(branch_from.label, after_branch.label);
        self.open_convergences
            .add(&[branch_target.label], after_branch)?;

        self.open_branch_path.push(self.current_block.label);
        let old_block = self.replace_current_block(branch_target);
        self.current_scope.insert_basic_block(old_block);

        self.new_subscope();
        Ok(())
    }

    // create a conditional branch from the current block, transparently setting the current block
    // to the true branch. Inserts the current block (before call) into the current scope, and
    // creates a new subscope for the true branch
    pub fn conditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
        condition: ir::JumpCondition,
    ) -> Result<(), BranchError> {
        trace::debug!("create conditional branch from current block");

        let branch_from = &mut self.current_block;
        self.open_branch_path.push(branch_from.label);

        let (branch_true, branch_false, branch_convergence) = self
            .block_manager
            .create_conditional_branch(branch_from, loc, condition);

        self.open_convergences
            .rename_source(branch_from.label, branch_convergence.label);
        self.open_convergences
            .add(&[branch_true.label, branch_false.label], branch_convergence)?;

        match self
            .open_branch_false_blocks
            .insert(branch_from.label, branch_false)
        {
            Some(existing_false_block) => {
                return Err(BranchError::ExistingFalseBlock(
                    branch_from.label,
                    existing_false_block.label,
                ))
            }
            None => (),
        };

        let old_block = self.replace_current_block(branch_true);
        self.current_scope.insert_basic_block(old_block);

        self.new_subscope();
        Ok(())
    }

    pub fn create_loop(&mut self, loc: SourceLoc) -> Result<usize, LoopError> {
        trace::debug!("create loop");

        let (loop_top, loop_bottom, after_loop) =
            self.block_manager.create_loop(&mut self.current_block, loc);

        // track the top of the loop we are opening
        self.open_loop_beginnings.push(loop_top.label);

        // transfer control flow unconditionally to the top of the loop
        let loop_entry_jump =
            ir::IrLine::new_jump(loc, loop_top.label, ir::JumpCondition::Unconditional);
        self.current_block.statements.push(loop_entry_jump);

        // now the current block should be the loop's top
        let old_block = self.replace_current_block(loop_top);
        self.open_convergences
            .rename_source(old_block.label, after_loop.label);
        self.current_scope.insert_basic_block(old_block);

        let after_loop_label = after_loop.label;

        // NB the convergence here is handled a bit differently for loops, since we might want a
        // condition check either at the top or at the bottom of the loop. In any case, the
        // finish_loop() handling of the convergence mirrors this scheme so the handling is valid.
        // the bottom of the loop should converge to after the loop
        self.open_convergences
            .add(&[loop_bottom.label], after_loop)?;

        // the top of the loop should converge to the bottom of the loop
        self.open_convergences
            .add(&[self.current_block.label], loop_bottom)?;

        Ok(after_loop_label)
    }

    pub fn finish_loop(
        &mut self,
        loc: SourceLoc,
        loop_bottom_actions: Vec<ir::IrLine>,
    ) -> Result<(), LoopError> {
        // wherever the current block ends up, it should have convergence as Done to loop_bottom
        // per create_loop() as each loop convergence is singly-associated
        match self.open_convergences.converge(self.current_block.label)? {
            ConvergenceResult::Done(loop_bottom) => {
                // transfer control flow from the current block to loop_bottom
                let loop_bottom_jump =
                    ir::IrLine::new_jump(loc, loop_bottom.label, ir::JumpCondition::Unconditional);
                self.current_block.statements.push(loop_bottom_jump);

                // make our current block loop_bottom
                let old_block = self.replace_current_block(loop_bottom);
                self.current_scope.insert_basic_block(old_block);

                // insert any IRs that need to be at the bottom of the loop but before the looping jump itself
                for loop_bottom_ir in loop_bottom_actions {
                    self.current_block.statements.push(loop_bottom_ir);
                }

                // figure out where the top of our loop is to jump back to
                let loop_top = match self.open_loop_beginnings.pop() {
                    Some(top) => top,
                    None => return Err(LoopError::NotLooping),
                };

                let loop_jump =
                    ir::IrLine::new_jump(loc, loop_top, ir::JumpCondition::Unconditional);
                self.current_block.statements.push(loop_jump);

                // now that we are in loop_bottom, create_loop() should have a convergence for us
                // which will give us the after_loop block
                match self.open_convergences.converge(self.current_block.label)? {
                    ConvergenceResult::Done(after_loop) => {
                        // transition to after_loop, assuming that the correct IR was inserted to
                        // break out of the loop at some point within the loop or by
                        // loop_bottom_actions
                        let old_block = self.replace_current_block(after_loop);
                        self.current_scope.insert_basic_block(old_block);

                        Ok(())
                    }
                    ConvergenceResult::NotDone(label) => Err(LoopError::LoopBottomNotDone(label)),
                }
            }
            ConvergenceResult::NotDone(label) => Err(LoopError::LoopInsideNotDone(label)),
        }
    }

    pub fn next_temp(&mut self, type_: types::Type) -> ir::Operand {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        let definition_path = self.definition_path();
        self.symtab
            .create_variable(
                definition_path,
                symtab::Variable::new(temp_name.clone(), Some(type_)),
            )
            .unwrap();
        ir::Operand::new_as_temporary(temp_name)
    }

    pub fn append_jump_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operations::Jump(_) => {
                self.current_block.statements.push(statement);
                Ok(())
            }
            _ => Err(()),
        }
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operations::Jump(_) => Err(()),
            _ => {
                self.current_block.statements.push(statement);
                Ok(())
            }
        }
    }
}

impl<'a> symtab::DefContext for FunctionWalkContext<'a> {
    fn symtab(&self) -> &symtab::SymbolTable {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut symtab::SymbolTable {
        &mut self.symtab
    }

    fn definition_path(&self) -> symtab::DefPath {
        self.def_path
            .clone_with_join(self.relative_local_def_path.clone())
    }
}

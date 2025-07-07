use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{
        ir,
        linearizer::*,
        symtab::{self, DefContext, DefPathComponent},
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

pub struct FunctionWalkContext {
    symtab: Box<symtab::SymbolTable>,
    // definition path from the root of the symbol table to this function
    global_def_path: symtab::DefPath,
    self_type: Option<types::Type>,
    block_manager: std::cell::Cell<BlockManager>,
    // definition path starting after this function
    relative_local_def_path: symtab::DefPath,
    values: ir::ValueInterner,
    open_loop_beginnings: Vec<usize>,
    // conditional branches from source block to the branch_false block.
    // creating a branch replaces current_block with the branch_tru
    // e block,
    // but we also need to track the branch_false block if it exists
    open_branch_false_blocks: HashMap<usize, ir::BasicBlock>,
    // convergences which currently exist for blocks
    open_convergences: std::cell::Cell<BlockConvergences>,
    // branch path of basic block labels targeted by the branches which got us to current_block
    open_branch_path: Vec<usize>,
    // key for DefPathComponent::BasicBlock from self.def_path
    current_block: usize,
}

impl FunctionWalkContext {
    pub fn new(
        parent_context: symtab::BasicDefContext,
        prototype: symtab::FunctionPrototype,
        self_type: Option<types::Type>,
    ) -> Result<Self, symtab::SymbolError> {
        trace::trace!("Create function walk context for {}", prototype);

        let (symtab, parent_def_path) = parent_context.take().unwrap();

        let (block_manager, start_block, end_block) = BlockManager::new();
        let mut base_convergences = BlockConvergences::new();
        base_convergences
            .add(&[start_block.label], end_block)
            .unwrap();

        let start_block_label = start_block.label;
        let my_def_path = {
            symtab.insert::<symtab::Function>(
                parent_def_path,
                symtab::Function::new(prototype, None),
            )?
        };
        let my_def_path_component = my_def_path.last().clone();

        symtab
            .insert::<ir::BasicBlock>(my_def_path.clone(), start_block)
            .unwrap();
        let values = ir::ValueInterner::new(
            symtab
                .id_for_type(&my_def_path, &types::Type::Unit)
                .unwrap(),
        );

        Ok(Self {
            symtab,
            global_def_path: my_def_path,
            self_type,
            block_manager: std::cell::Cell::new(block_manager),
            open_branch_false_blocks: HashMap::new(),
            open_convergences: base_convergences,
            open_branch_path: Vec::new(),
            relative_local_def_path: symtab::DefPath::new(my_def_path_component),
            values,
            o pen_loop_beginnings: Vec::new(),
            current_block: start_block_label,
        })
    }

    fn new_subscope(&mut self) {
        self.relative_local_def_path.push(
            self.symtab
                .insert::<symtab::Scope>(self.def_path(), symtab::Scope::new(0))
                .unwrap()
                .pop()
                .unwrap(),
        );
    }

    fn pop_current_scope(&mut self) -> Result<(), BranchError> {
        trace::trace!("pop current scope");

        match self.relative_local_def_path.pop() {
            Some(symtab::DefPathComponent::Scope(_)) => Ok(()),
            _ => Err(BranchError::NotBranched),
        }
    }

    fn replace_current_block(&mut self, new_current: ir::BasicBlock) -> &mut ir::BasicBlock {
        let old_current_label = self.current_block;
        let new_current_label = new_current.label;
        self.current_block = new_current_label;
        let global_def_path = self.global_def_path.clone();
        self.insert_at::<ir::BasicBlock>(global_def_path.clone(), new_current)
            .unwrap();

        let old_current = self
            .lookup_at_mut::<ir::BasicBlock>(&global_def_path, &old_current_label)
            .unwrap();
        trace::trace!(
            "replace current block ({}) with block {}",
            old_current.label,
            new_current_label,
        );

        old_current
    }

    fn current_block_mut(&mut self) -> &mut ir::BasicBlock {
        let current_block = self.current_block;
        self.lookup_mut::<ir::BasicBlock>(&current_block).unwrap()
    }

    fn current_block(&self) -> &ir::BasicBlock {
        let current_block = self.current_block;
        self.lookup::<ir::BasicBlock>(&current_block).unwrap()
    }

    pub fn unit_value_id(&self) -> ir::ValueId {
        ir::ValueInterner::unit_value_id()
    }

    pub fn value_for_variable(&mut self, variable_def_path: &symtab::DefPath) -> &ir::ValueId {
        self.values.id_for_variable(variable_def_path).unwrap()
    }

    pub fn value_for_id(&mut self, id: &ir::ValueId) -> Option<&ir::Value> {
        self.values.get(id)
    }

    pub fn value_id_for_constant(&mut self, constant: usize) -> &ir::ValueId {
        self.values.id_for_constant(constant)
    }

    pub fn type_definition_for_value_id(&mut self, id: &ir::ValueId) -> &symtab::TypeDefinition {
        let value = self.value_for_id(id).unwrap().clone();
        self.type_for_id(&value.type_.unwrap()).unwrap()
    }

    pub fn type_for_value_id(&mut self, id: &ir::ValueId) -> &types::Type {
        let type_id = self.value_for_id(id).unwrap().type_.unwrap();
        self.type_for_id(&type_id).unwrap().type_()
    }

    pub fn type_id_for_value_id(&mut self, id: &ir::ValueId) -> symtab::TypeId {
        let value = self.value_for_id(id).unwrap();
        value.type_.unwrap()
    }

    pub fn finish_true_branch_switch_to_false(&mut self) -> Result<(), BranchError> {
        trace::debug!("finish true branch, switch to false");

        let branched_from = match self.open_branch_path.last() {
            Some(branched_from) => branched_from,
            None => return Err(BranchError::NotBranched),
        };

        match self.open_convergences.converge(self.current_block)? {
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

                let converged_from = self.replace_current_block(false_block);
                converged_from.statements.push(convergence_jump);

                Ok(())
            }
            ConvergenceResult::Done(_block) => Err(BranchError::MissingFalseBlock(*branched_from)),
        }?;

        self.pop_current_scope()?;
        self.new_subscope();
        Ok(())
    }

    pub fn finish_branch(&mut self) -> Result<(), BranchError> {
        let branched_from = self.open_branch_path.last().unwrap();
        trace::debug!("finish branch from block {}", branched_from);

        match self.open_convergences.converge(self.current_block)? {
            ConvergenceResult::Done(converge_to_block) => {
                let convergence_jump = ir::IrLine::new_jump(
                    SourceLoc::none(),
                    converge_to_block.label,
                    ir::JumpCondition::Unconditional,
                );

                let converged_from = self.replace_current_block(converge_to_block);
                converged_from.statements.push(convergence_jump);

                match self.pop_current_scope() {
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

        let branch_from = self.current_block_mut();
        let (branch_target, after_branch) = self
            .block_manager
            .get_mut()
            .create_unconditional_branch(branch_from, loc);

        self.open_convergences
            .rename_source(branch_from.label, after_branch.label);
        self.open_convergences
            .add(&[branch_target.label], after_branch)?;

        self.open_branch_path.push(self.current_block);
        let old_block = self.replace_current_block(branch_target);

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

        let branch_from = self.current_block_mut();
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

        self.new_subscope();
        Ok(())
    }

    pub fn create_loop(&mut self, loc: SourceLoc) -> Result<usize, LoopError> {
        trace::debug!("create loop");

        let current_block_label = self.current_block;
        let current_block_mut = self
            .lookup_mut::<ir::BasicBlock>(&current_block_label)
            .unwrap();
        let (loop_top, loop_bottom, after_loop) =
            self.block_manager.get_mut().create_loop(current_block_mut, loc);

        // track the top of the loop we are opening
        self.open_loop_beginnings.push(loop_top.label);

        // transfer control flow unconditionally to the top of the loop
        let loop_entry_jump =
            ir::IrLine::new_jump(loc, loop_top.label, ir::JumpCondition::Unconditional);
        self.current_block_mut().statements.push(loop_entry_jump);

        // now the current block should be the loop's top
        let old_block_label = self.replace_current_block(loop_top).label;
        self.open_convergences.get_mut()
            .rename_source(old_block_label, after_loop.label);

        let after_loop_label = after_loop.label;

        // NB the convergence here is handled a bit differently for loops, since we might want a
        // condition check either at the top or at the bottom of the loop. In any case, the
        // finish_loop() handling of the convergence mirrors this scheme so the handling is valid.
        // the bottom of the loop should converge to after the loop
        self.open_convergences.get_mut()
            .add(&[loop_bottom.label], after_loop)?;

        // the top of the loop should converge to the bottom of the loop
        self.open_convergences.get_mut()
            .add(&[self.current_block], loop_bottom)?;

        Ok(after_loop_label)
    }

    pub fn finish_loop(
        &mut self,
        loc: SourceLoc,
        loop_bottom_actions: Vec<ir::IrLine>,
    ) -> Result<(), LoopError> {
        // wherever the current block ends up, it should have convergence as Done to loop_bottom
        // per create_loop() as each loop convergence is singly-associated
        match self.open_convergences.get_mut().converge(self.current_block)? {
            ConvergenceResult::Done(loop_bottom) => {
                // transfer control flow from the current block to loop_bottom
                let loop_bottom_jump =
                    ir::IrLine::new_jump(loc, loop_bottom.label, ir::JumpCondition::Unconditional);
                self.current_block_mut().statements.push(loop_bottom_jump);

                // make our current block loop_bottom
                let old_block = self.replace_current_block(loop_bottom);

                // insert any IRs that need to be at the bottom of the loop but before the looping jump itself
                for loop_bottom_ir in loop_bottom_actions {
                    self.current_block_mut().statements.push(loop_bottom_ir);
                }

                // figure out where the top of our loop is to jump back to
                let loop_top = match self.open_loop_beginnings.pop() {
                    Some(top) => top,
                    None => return Err(LoopError::NotLooping),
                };

                let loop_jump =
                    ir::IrLine::new_jump(loc, loop_top, ir::JumpCondition::Unconditional);
                self.current_block_mut().statements.push(loop_jump);

                // now that we are in loop_bottom, create_loop() should have a convergence for us
                // which will give us the after_loop block
                match self.open_convergences.get_mut().converge(self.current_block)? {
                    ConvergenceResult::Done(after_loop) => {
                        // transition to after_loop, assuming that the correct IR was inserted to
                        // break out of the loop at some point within the loop or by
                        // loop_bottom_actions
                        self.replace_current_block(after_loop);
                        Ok(())
                    }
                    ConvergenceResult::NotDone(label) => Err(LoopError::LoopBottomNotDone(label)),
                }
            }
            ConvergenceResult::NotDone(label) => Err(LoopError::LoopInsideNotDone(label)),
        }
    }

    pub fn next_temp(&mut self, type_: Option<types::Type>) -> ir::ValueId {
        match type_ {
            Some(known_type) => {
                let known_type_id = self
                    .symtab()
                    .id_for_type(&self.def_path(), &known_type)
                    .unwrap();
                self.values.next_temp(Some(known_type_id))
            }
            None => self.values.next_temp(None),
        }
    }

    pub fn append_jump_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operations::Jump(_) => {
                self.current_block_mut().statements.push(statement);
                Ok(())
            }
            _ => Err(()),
        }
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operations::Jump(_) => Err(()),
            _ => {
                self.current_block_mut().statements.push(statement);
                Ok(())
            }
        }
    }
}

impl std::fmt::Debug for FunctionWalkContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Function walk context @ {}", self.def_path())
    }
}

impl symtab::DefContext for FunctionWalkContext {
    fn symtab(&self) -> &symtab::SymbolTable {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut symtab::SymbolTable {
        &mut self.symtab
    }

    fn def_path(&self) -> symtab::DefPath {
        self.global_def_path
            .clone()
            .join(self.relative_local_def_path.clone())
            .unwrap()
    }

    fn def_path_mut(&mut self) -> &mut symtab::DefPath {
        &mut self.relative_local_def_path
    }

    fn take(self) -> Result<(Box<symtab::SymbolTable>, symtab::DefPath), ()> {
        // TODO: manage control flow, etc... 
        Ok((self.symtab, self.global_def_path))
    }
}

impl Into<symtab::BasicDefContext> for FunctionWalkContext {
    fn into(self) -> symtab::BasicDefContext {
        let (symtab, path) = self.take().unwrap()
        symtab::BasicDefContext::with_path(symtab, path)
    }
}


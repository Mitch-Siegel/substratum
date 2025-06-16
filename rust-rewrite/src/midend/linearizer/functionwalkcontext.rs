use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{
        ir,
        linearizer::*,
        symtab::{
            self, BasicBlockOwner, MutBasicBlockOwner, MutScopeOwner, MutTypeOwner,
            MutVariableOwner, TypeOwner, VariableOwner,
        },
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
    module_context: &'a mut ModuleWalkContext,
    self_type: Option<types::Type>,
    prototype: symtab::FunctionPrototype,
    block_manager: BlockManager,
    scope_stack: Vec<symtab::Scope>,
    current_scope: symtab::Scope,
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
        module_context: &'a mut ModuleWalkContext,
        prototype: symtab::FunctionPrototype,
        self_type: Option<types::Type>,
    ) -> Self {
        trace::trace!("Create function walk context for {}", prototype);
        let (control_flow, start_block, end_block) = BlockManager::new();
        let mut base_convergences = BlockConvergences::new();
        base_convergences
            .add(&[start_block.label], end_block)
            .unwrap();

        let argument_scope = prototype.create_argument_scope().unwrap();

        Self {
            module_context,
            prototype,
            self_type,
            block_manager: control_flow,
            open_branch_false_blocks: HashMap::new(),
            open_convergences: base_convergences,
            open_branch_path: Vec::new(),
            scope_stack: Vec::new(),
            current_scope: argument_scope,
            open_loop_beginnings: Vec::new(),
            temp_num: 0,
            current_block: start_block,
        }
    }

    fn all_scopes(&self) -> impl Iterator<Item = &symtab::Scope> {
        std::iter::once(&self.current_scope).chain(self.scope_stack.iter().rev())
    }

    fn all_scopes_mut(&mut self) -> impl Iterator<Item = &mut symtab::Scope> {
        std::iter::once(&mut self.current_scope).chain(self.scope_stack.iter_mut().rev())
    }

    fn new_subscope(&mut self) {
        let parent = std::mem::replace(&mut self.current_scope, symtab::Scope::new());
        self.scope_stack.push(parent);
    }

    fn pop_current_scope_to_subscope_of_next(&mut self) -> Result<(), BranchError> {
        match self.scope_stack.pop() {
            Some(next_scope) => {
                let old_scope = std::mem::replace(&mut self.current_scope, next_scope);
                self.current_scope.insert_scope(old_scope);
                Ok(())
            }
            None => Err(BranchError::ScopeHandling),
        }
    }

    pub fn finish_true_branch_switch_to_false(&mut self) -> Result<(), BranchError> {
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

                let mut converged_from = std::mem::replace(&mut self.current_block, false_block);

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

        match self.open_convergences.converge(self.current_block.label)? {
            ConvergenceResult::Done(converge_to_block) => {
                let convergence_jump = ir::IrLine::new_jump(
                    SourceLoc::none(),
                    converge_to_block.label,
                    ir::JumpCondition::Unconditional,
                );

                let mut converged_from =
                    std::mem::replace(&mut self.current_block, converge_to_block);

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
        let branch_from = &mut self.current_block;
        let (branch_target, after_branch) = self
            .block_manager
            .create_unconditional_branch(branch_from, loc);

        self.open_convergences
            .rename_source(branch_from.label, after_branch.label);
        self.open_convergences
            .add(&[branch_target.label], after_branch)?;

        self.open_branch_path.push(self.current_block.label);
        let old_block = std::mem::replace(&mut self.current_block, branch_target);
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

        let old_block = std::mem::replace(&mut self.current_block, branch_true);
        self.current_scope.insert_basic_block(old_block);

        self.new_subscope();
        Ok(())
    }

    pub fn create_loop(&mut self, loc: SourceLoc) -> Result<usize, LoopError> {
        let (loop_top, loop_bottom, after_loop) =
            self.block_manager.create_loop(&mut self.current_block, loc);

        // track the top of the loop we are opening
        self.open_loop_beginnings.push(loop_top.label);

        // transfer control flow unconditionally to the top of the loop
        let loop_entry_jump =
            ir::IrLine::new_jump(loc, loop_top.label, ir::JumpCondition::Unconditional);
        self.current_block.statements.push(loop_entry_jump);

        // now the current block should be the loop's top
        let old_block = std::mem::replace(&mut self.current_block, loop_top);
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
                let old_block = std::mem::replace(&mut self.current_block, loop_bottom);
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
                        let old_block = std::mem::replace(&mut self.current_block, after_loop);
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
        self.insert_variable(symtab::Variable::new(temp_name.clone(), Some(type_)))
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

impl<'a> symtab::BasicBlockOwner for FunctionWalkContext<'a> {
    fn basic_blocks(&self) -> impl Iterator<Item = &ir::BasicBlock> {
        self.all_scopes()
            .into_iter()
            .flat_map(|scope| scope.basic_blocks())
    }

    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock> {
        let _ = trace::span_auto!(
            trace::Level::TRACE,
            "Lookup basic block in function walk context: ",
            "{}",
            label
        );
        for lookup_scope in self.scope_stack.iter().rev() {
            match lookup_scope.lookup_basic_block(label) {
                Some(block) => return Some(block),
                None => (),
            }
        }

        None
    }
}

impl<'a> symtab::VariableOwner for FunctionWalkContext<'a> {
    fn variables(&self) -> impl Iterator<Item = &symtab::Variable> {
        self.all_scopes()
            .into_iter()
            .flat_map(|scope| scope.variables())
    }

    fn lookup_variable_by_name(
        &self,
        name: &str,
    ) -> Result<&symtab::Variable, symtab::UndefinedSymbol> {
        for lookup_scope in self.all_scopes() {
            match lookup_scope.lookup_variable_by_name(name) {
                Ok(variable) => return Ok(variable),
                _ => (),
            }
        }

        Err(symtab::UndefinedSymbol::variable(name.into()))
    }
}
impl<'a> symtab::MutVariableOwner for FunctionWalkContext<'a> {
    fn insert_variable(&mut self, variable: symtab::Variable) -> Result<(), symtab::DefinedSymbol> {
        self.current_scope.insert_variable(variable)
    }
}

impl<'a> symtab::TypeOwner for FunctionWalkContext<'a> {
    fn types(&self) -> impl Iterator<Item = &symtab::TypeDefinition> {
        self.all_scopes()
            .into_iter()
            .flat_map(|scope| scope.types())
            .chain(self.module_context.types())
    }

    fn lookup_type(
        &self,
        type_: &types::Type,
    ) -> Result<&symtab::TypeDefinition, symtab::UndefinedSymbol> {
        for lookup_scope in self.all_scopes() {
            match lookup_scope.lookup_type(type_) {
                Ok(type_) => return Ok(type_),
                _ => (),
            }
        }

        self.module_context.lookup_type(type_)
    }

    fn lookup_struct(&self, name: &str) -> Result<&symtab::StructRepr, symtab::UndefinedSymbol> {
        for lookup_scope in self.all_scopes() {
            match lookup_scope.lookup_struct(name) {
                Ok(struct_) => return Ok(struct_),
                _ => (),
            }
        }

        self.module_context.lookup_struct(name)
    }
}
impl<'ctx> FunctionWalkContext<'ctx> {
    fn lookup_type_mut_local(
        scopes: impl Iterator<Item = &'ctx mut symtab::Scope>,
        type_: &types::Type,
    ) -> Option<&'ctx mut symtab::TypeDefinition> {
        for scope in scopes {
            if let Ok(type_definition) = scope.lookup_type_mut(type_) {
                return Some(type_definition);
            }
        }
        None
    }
}
impl<'ctx> symtab::MutTypeOwner for FunctionWalkContext<'ctx> {
    fn insert_type(&mut self, type_: symtab::TypeDefinition) -> Result<(), symtab::DefinedSymbol> {
        self.current_scope.insert_type(type_)
    }
    fn lookup_type_mut(
        &mut self,
        type_: &types::Type,
    ) -> Result<&mut symtab::TypeDefinition, symtab::UndefinedSymbol> {
        // have to manually handle the iteration here as all_scopes_mut stretches borrow lifetime
        // to full function, which we can't have
        for scope in
            std::iter::once(&mut self.current_scope).chain(self.scope_stack.iter_mut().rev())
        {
            match scope.lookup_type_mut(type_) {
                Ok(type_definition) => return Ok(type_definition),
                Err(_) => (),
            }
        }

        self.module_context.lookup_type_mut(type_)
    }
}

impl<'a> symtab::SelfTypeOwner for FunctionWalkContext<'a> {
    fn self_type(&self) -> &types::Type {
        self.self_type.as_ref().unwrap()
    }
}

impl<'a> symtab::ScopeOwnerships for FunctionWalkContext<'a> {}
impl<'a> symtab::ModuleOwnerships for FunctionWalkContext<'a> {}
impl<'a> types::TypeSizingContext for FunctionWalkContext<'a> {}

impl<'a> Into<symtab::Function> for FunctionWalkContext<'a> {
    fn into(mut self) -> symtab::Function {
        assert!(self.scope_stack.is_empty());
        match self
            .open_convergences
            .converge(self.current_block.label)
            .unwrap()
        {
            ConvergenceResult::Done(exit_block) => {
                let jump_to_exit_block = ir::IrLine::new_jump(
                    SourceLoc::none(),
                    exit_block.label,
                    ir::JumpCondition::Unconditional,
                );
                self.current_block.statements.push(jump_to_exit_block);
                let old_block = std::mem::replace(&mut self.current_block, exit_block);
                self.current_scope.insert_basic_block(old_block);
            }
            ConvergenceResult::NotDone(converge_to) => {
                panic!("FunctionWalkContext.into(symtab::Function can't converge - expecting convergence to exit block but saw unfinished convergence to {}", converge_to);
            }
        }
        assert!(self.open_convergences.is_empty());
        assert!(self.open_branch_path.is_empty());

        while !self.scope_stack.is_empty() {
            self.pop_current_scope_to_subscope_of_next().unwrap()
        }
        assert!(self.scope_stack.is_empty());

        self.current_scope.insert_basic_block(self.current_block);

        self.current_scope.collapse();

        symtab::Function::new(self.prototype, self.current_scope)
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        frontend::sourceloc::SourceLoc,
        midend::{
            ir,
            linearizer::{functionwalkcontext::*, *},
            symtab::{self, VariableOwner},
            types,
        },
    };

    #[test]
    fn branch_error_from_convergence_error() {
        let convergence_error = ConvergenceError::FromBlockExists(1);

        assert_eq!(
            BranchError::from(convergence_error),
            BranchError::Convergence(ConvergenceError::FromBlockExists(1))
        );
    }

    fn test_prototype() -> symtab::FunctionPrototype {
        symtab::FunctionPrototype::new(
            "test_function".into(),
            vec![
                symtab::Variable::new("a".into(), Some(types::Type::U16)),
                symtab::Variable::new("b".into(), Some(types::Type::I32)),
            ],
            types::Type::I32,
        )
    }

    fn test_context<'a>(context: &'a mut ModuleWalkContext) -> FunctionWalkContext<'a> {
        FunctionWalkContext::new(context, test_prototype(), None)
    }

    #[test]
    fn subscope_handling() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        context.new_subscope();
        context.new_subscope();
        assert_eq!(context.pop_current_scope_to_subscope_of_next(), Ok(()));
        context.new_subscope();
        assert_eq!(context.pop_current_scope_to_subscope_of_next(), Ok(()));
        assert_eq!(context.pop_current_scope_to_subscope_of_next(), Ok(()));
        context.new_subscope();
        assert_eq!(context.pop_current_scope_to_subscope_of_next(), Ok(()));
        assert_eq!(
            context.pop_current_scope_to_subscope_of_next(),
            Err(BranchError::ScopeHandling)
        );
    }

    #[test]
    fn unconditional_branch() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        let pre_branch_label = context.current_block.label;
        assert_eq!(
            context.unconditional_branch_from_current(SourceLoc::none()),
            Ok(())
        );
        let in_branch_label = context.current_block.label;
        assert_ne!(in_branch_label, pre_branch_label);

        assert_eq!(context.finish_branch(), Ok(()));
        let post_branch_label = context.current_block.label;
        assert_ne!(post_branch_label, in_branch_label);
    }

    #[test]
    fn conditional_branch() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        let pre_branch_label = context.current_block.label;
        assert_eq!(
            context.conditional_branch_from_current(
                SourceLoc::none(),
                ir::JumpCondition::NE(ir::DualSourceOperands::new(
                    ir::Operand::new_as_variable("a".into()),
                    ir::Operand::new_as_variable("b".into()),
                )),
            ),
            Ok(())
        );
        let true_branch_label = context.current_block.label;
        assert_ne!(true_branch_label, pre_branch_label);

        assert_eq!(context.finish_true_branch_switch_to_false(), Ok(()));
        let false_branch_label = context.current_block.label;
        assert_ne!(false_branch_label, pre_branch_label);

        assert_eq!(context.finish_branch(), Ok(()));
        let post_branch_label = context.current_block.label;
        assert_ne!(post_branch_label, true_branch_label);
    }

    #[test]
    fn conditional_branch_errors() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(
            context.finish_true_branch_switch_to_false(),
            Err(BranchError::NotBranched)
        );

        assert_eq!(
            context.unconditional_branch_from_current(SourceLoc::none()),
            Ok(())
        );

        assert_eq!(
            context.finish_true_branch_switch_to_false(),
            Err(BranchError::MissingFalseBlock(0))
        );

        assert_eq!(
            context.unconditional_branch_from_current(SourceLoc::none()),
            Ok(())
        );
    }

    #[test]
    fn create_and_finish_loop() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(context.create_loop(SourceLoc::none()), Ok(4));

        assert_eq!(context.finish_loop(SourceLoc::none(), Vec::new()), Ok(()));
    }

    #[test]
    fn finish_loop_error() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(
            context.finish_loop(SourceLoc::none(), Vec::new()),
            Err(LoopError::NotLooping)
        );
    }

    #[test]
    fn branch_with_nested_loop() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(
            context.conditional_branch_from_current(
                SourceLoc::none(),
                ir::JumpCondition::NE(ir::DualSourceOperands::new(
                    ir::Operand::new_as_variable("a".into()),
                    ir::Operand::new_as_variable("b".into()),
                )),
            ),
            Ok(())
        );

        assert_eq!(context.create_loop(SourceLoc::none()), Ok(7));

        assert_eq!(context.finish_loop(SourceLoc::none(), Vec::new()), Ok(()));

        assert_eq!(context.finish_true_branch_switch_to_false(), Ok(()));

        assert_eq!(context.finish_branch(), Ok(()));
    }

    #[test]
    fn branch_with_nested_loop_error() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(
            context.conditional_branch_from_current(
                SourceLoc::none(),
                ir::JumpCondition::NE(ir::DualSourceOperands::new(
                    ir::Operand::new_as_variable("a".into()),
                    ir::Operand::new_as_variable("b".into()),
                )),
            ),
            Ok(())
        );

        assert_eq!(context.create_loop(SourceLoc::none()), Ok(7));

        assert_eq!(
            context.finish_true_branch_switch_to_false(),
            Err(BranchError::MissingFalseBlock(0))
        );
    }

    #[test]
    fn loop_with_nested_branch() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(context.create_loop(SourceLoc::none()), Ok(4));

        assert_eq!(
            context.conditional_branch_from_current(
                SourceLoc::none(),
                ir::JumpCondition::NE(ir::DualSourceOperands::new(
                    ir::Operand::new_as_variable("a".into()),
                    ir::Operand::new_as_variable("b".into()),
                )),
            ),
            Ok(())
        );

        assert_eq!(context.finish_true_branch_switch_to_false(), Ok(()));

        assert_eq!(context.finish_branch(), Ok(()));

        assert_eq!(context.finish_loop(SourceLoc::none(), Vec::new()), Ok(()));
    }

    #[test]
    fn loop_with_nested_branch_error() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(context.create_loop(SourceLoc::none()), Ok(4));

        assert_eq!(
            context.conditional_branch_from_current(
                SourceLoc::none(),
                ir::JumpCondition::NE(ir::DualSourceOperands::new(
                    ir::Operand::new_as_variable("a".into()),
                    ir::Operand::new_as_variable("b".into()),
                )),
            ),
            Ok(())
        );

        assert_eq!(
            context.finish_loop(SourceLoc::none(), Vec::new()),
            Err(LoopError::LoopInsideNotDone(7))
        );
    }

    #[test]
    fn next_temp() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(
            context.next_temp(types::Type::I8),
            ir::Operand::new_as_temporary(String::from(".T0"))
        );

        assert_eq!(
            context.lookup_variable_by_name(".T0"),
            Ok(&symtab::Variable::new(".T0".into(), Some(types::Type::I8)))
        );
    }

    #[test]
    fn append_statement_to_current_block() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        assert_eq!(
            context.append_statement_to_current_block(ir::IrLine::new_binary_op(
                SourceLoc::none(),
                ir::BinaryOperations::new_add(
                    ir::Operand::new_as_variable("c".into()),
                    ir::Operand::new_as_variable("a".into()),
                    ir::Operand::new_as_variable("b".into())
                )
            )),
            Ok(())
        );

        assert_eq!(
            context.append_statement_to_current_block(ir::IrLine::new_jump(
                SourceLoc::none(),
                123,
                ir::JumpCondition::Unconditional,
            )),
            Err(())
        );
    }

    #[test]
    fn variable_owner() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        symtab::tests::test_variable_owner(&mut context);
    }

    #[test]
    fn type_owner() {
        let mut module_context = ModuleWalkContext::new();
        let mut context = test_context(&mut module_context);

        symtab::tests::test_type_owner(&mut context);
    }
}

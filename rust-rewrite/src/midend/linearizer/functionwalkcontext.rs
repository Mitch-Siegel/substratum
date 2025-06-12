use crate::{
    trace,
    frontend::sourceloc::SourceLoc,
    midend::{
        ir,
        linearizer::*,
        symtab::{self, BasicBlockOwner, ScopeOwner, VariableOwner},
        types,
    },
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

pub struct FunctionWalkContext<'a> {
    module_context: &'a mut ModuleWalkContext,
    self_type: Option<types::Type>,
    prototype: symtab::FunctionPrototype,
    control_flow: ir::ControlFlow,
    scope_stack: Vec<symtab::Scope>,
    current_scope: symtab::Scope,
    current_scope_address: symtab::ScopePath, // relative to the topmost scope of the current
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
        let (control_flow, start_block, end_block) = ir::ControlFlow::new();
        let mut base_convergences = BlockConvergences::new();
        base_convergences
            .add(&[start_block.label], end_block)
            .unwrap();

        let argument_scope = prototype.create_argument_scope().unwrap();

        Self {
            module_context,
            prototype,
            self_type,
            control_flow,
            open_branch_false_blocks: HashMap::new(),
            open_convergences: base_convergences,
            open_branch_path: Vec::new(),
            scope_stack: Vec::new(),
            current_scope: argument_scope,
            // TODO: remove me?
            current_scope_address: symtab::ScopePath::new(),
            temp_num: 0,
            current_block: start_block,
        }
    }

    fn all_scopes(&self) -> Vec<&symtab::Scope> {
        std::iter::once(&self.current_scope)
            .chain(self.scope_stack.iter().rev())
            .collect()
    }

    fn all_scopes_mut(&mut self) -> Vec<&mut symtab::Scope> {
        std::iter::once(&mut self.current_scope)
            .chain(self.scope_stack.iter_mut().rev())
            .collect()
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
            .control_flow
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
            .control_flow
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

    pub fn create_loop(&mut self) -> usize {
        0
    }

    pub fn next_temp(&mut self, type_: types::Type) -> ir::Operand {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        self.insert_variable(symtab::Variable::new(temp_name.clone(), Some(type_)))
            .unwrap();
        ir::Operand::new_as_temporary(temp_name)
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
    fn insert_basic_block(&mut self, _block: ir::BasicBlock) {
        unimplemented!("insert_basic_block not to be used by FunctionWalkContext");
    }

    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock> {
        let _ = trace::span_auto!(trace::Level::TRACE, "Lookup basic block in function walk context: ", "{}", label);
        for lookup_scope in self.scope_stack.iter().rev() {
            match lookup_scope.lookup_basic_block(label) {
                Some(block) => return Some(block),
                None => (),
            }
        }

        None
    }

    fn lookup_basic_block_mut(&mut self, label: usize) -> Option<&mut ir::BasicBlock> {
        let _ = trace::span_auto!(trace::Level::TRACE, "Lookup basic block (mut) in function walk context: ", "{}", label);
        for lookup_scope in self.scope_stack.iter_mut().rev() {
            match lookup_scope.lookup_basic_block_mut(label) {
                Some(block) => return Some(block),
                None => (),
            }
        }

        None
    }
}

impl<'a> symtab::VariableOwner for FunctionWalkContext<'a> {
    fn insert_variable(&mut self, variable: symtab::Variable) -> Result<(), symtab::DefinedSymbol> {
        self.current_scope.insert_variable(variable)
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

impl<'a> symtab::TypeOwner for FunctionWalkContext<'a> {
    fn insert_type(&mut self, type_: symtab::TypeDefinition) -> Result<(), symtab::DefinedSymbol> {
        self.current_scope.insert_type(type_)
    }

    fn lookup_type(
        &self,
        type_: &types::Type,
    ) -> Result<&symtab::TypeDefinition, symtab::UndefinedSymbol> {
        let _ = trace::span_auto!(trace::Level::TRACE, "Lookup type in function walk context: ", "{}", type_);
        for lookup_scope in self.all_scopes() {
            match lookup_scope.lookup_type(type_) {
                Ok(type_) => return Ok(type_),
                _ => (),
            }
        }

        Err(symtab::UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_type_mut(
        &mut self,
        type_: &types::Type,
    ) -> Result<&mut symtab::TypeDefinition, symtab::UndefinedSymbol> {
        for lookup_scope in self.all_scopes_mut() {
            match lookup_scope.lookup_type_mut(type_) {
                Ok(type_) => return Ok(type_),
                _ => (),
            }
        }

        Err(symtab::UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_struct(&self, name: &str) -> Result<&symtab::StructRepr, symtab::UndefinedSymbol> {
        for lookup_scope in self.all_scopes() {
            match lookup_scope.lookup_struct(name) {
                Ok(struct_) => return Ok(struct_),
                _ => (),
            }
        }

        Err(symtab::UndefinedSymbol::struct_(name.into()))
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
    fn into(self) -> symtab::Function {
        assert!(self.scope_stack.is_empty());
        assert!(self.open_convergences.is_empty());
        assert!(self.open_branch_path.is_empty());

        symtab::Function::new(self.prototype, self.current_scope, self.control_flow)
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        frontend::sourceloc::SourceLoc,
        midend::{
            ir,
            linearizer::{functionwalkcontext::BranchError, *},
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
        context.unconditional_branch_from_current(SourceLoc::none());
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

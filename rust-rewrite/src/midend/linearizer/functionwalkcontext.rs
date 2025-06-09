use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{
        ir,
        linearizer::*,
        symtab::{
            self, AssociatedOwner, BasicBlockOwner, FunctionOwner, MethodOwner, ScopeOwner,
            TypeOwner, VariableOwner,
        },
        types::Type,
    },
};
use std::collections::HashMap;

enum ConvergenceResult {
    NotDone(usize),       // the label of the block to converge to
    Done(ir::BasicBlock), // the block converged to
}

pub struct BlockConvergences {
    open_convergences: HashMap<usize, usize>,
    convergence_blocks: HashMap<usize, ir::BasicBlock>,
}

impl BlockConvergences {
    pub fn new() -> Self {
        Self {
            open_convergences: HashMap::new(),
            convergence_blocks: HashMap::new(),
        }
    }

    pub fn add(&mut self, froms: &[usize], to: ir::BasicBlock) {
        for from in froms {
            match self.open_convergences.insert(*from, to.label) {
                Some(label) => panic!("convergence point from block {} already exists", label),
                None => (),
            }
        }
    }

    pub fn converge(&mut self, from: usize) -> ConvergenceResult {
        let converge_to = self.open_convergences.remove(&from).unwrap();

        let remaining_with_same_target = self
            .open_convergences
            .iter()
            .map(|(_, to)| if *to == converge_to { 1 } else { 0 })
            .sum::<usize>();

        if remaining_with_same_target > 0 {
            ConvergenceResult::NotDone(converge_to)
        } else {
            // manually unwrap and rewrap to ensure we actually removed something
            ConvergenceResult::Done(self.convergence_blocks.remove(&converge_to).unwrap())
        }
    }

    pub fn rename_source(&mut self, old: usize, new: usize) {
        self.open_convergences = self
            .open_convergences
            .iter()
            .map(|(from, to)| {
                if *from == old {
                    (new, *to)
                } else {
                    (*from, *to)
                }
            })
            .collect::<HashMap<usize, usize>>();
    }

    pub fn is_empty(&self) -> bool {
        self.open_convergences.is_empty() && self.convergence_blocks.is_empty()
    }
}

pub struct FunctionWalkContext<'a> {
    module_context: &'a mut ModuleWalkContext,
    self_type: Option<Type>,
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
        self_type: Option<Type>,
    ) -> Self {
        let (control_flow, start_block, end_block) = ir::ControlFlow::new();
        let mut base_convergences = BlockConvergences::new();
        base_convergences.add(&[start_block.label], end_block);

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
            current_scope_address: symtab::ScopePath::new(),
            temp_num: 0,
            current_block: start_block,
        }
    }

    fn pop_current_scope_to_subscope_of_next(&mut self) -> Result<(), ()> {
        match self.scope_stack.pop() {
            Some(next_scope) => {
                let old_scope = std::mem::replace(&mut self.current_scope, next_scope);
                self.current_scope.insert_scope(old_scope);
                Ok(())
            }
            None => Err(()),
        }
    }

    pub fn finish_true_branch_switch_to_false(&mut self) -> Result<(), ()> {
        let branched_from = self.open_branch_path.last().unwrap();

        match self.open_convergences.converge(self.current_block.label) {
            ConvergenceResult::NotDone(converge_to_label) => {
                let false_block = match self.open_branch_false_blocks.remove(branched_from) {
                    Some(false_block) => false_block,
                    None => return Err(()),
                };

                let convergence_jump = ir::IrLine::new_jump(
                    SourceLoc::none(),
                    converge_to_label,
                    ir::JumpCondition::Unconditional,
                );

                let mut converged_from = std::mem::replace(&mut self.current_block, false_block);

                converged_from.statements.push(convergence_jump);
                self.current_scope.insert_basic_block(converged_from);

                self.pop_current_scope_to_subscope_of_next()
            }
            ConvergenceResult::Done(_) => Err(()),
        }
    }

    pub fn finish_branch(&mut self) -> Result<(), ()> {
        let branched_from = self.open_branch_path.last().unwrap();

        match self.open_convergences.converge(self.current_block.label) {
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

                self.pop_current_scope_to_subscope_of_next()
            }
            ConvergenceResult::NotDone(_) => Err(()),
        }
    }

    // create an unconditional branch from the current block, transparently setting the current
    // block to the target. Inserts the current block (before call) into the current scope
    pub fn unconditional_branch_from_current(&mut self, loc: SourceLoc) {
        let branch_from = &mut self.current_block;
        let (branch_target, after_branch) = self
            .control_flow
            .create_unconditional_branch(branch_from, loc);

        self.open_convergences
            .rename_source(branch_from.label, branch_target.label);
        self.open_convergences
            .add(&[branch_target.label], after_branch);

        self.open_branch_path.push(self.current_block.label);
        let old_block = std::mem::replace(&mut self.current_block, branch_target);
        self.current_scope.insert_basic_block(old_block);

        let parent_scope = std::mem::replace(&mut self.current_scope, symtab::Scope::new());
        self.scope_stack.push(parent_scope);
    }

    // create a conditional branch from the current block, transparently setting the current block
    // to the true branch. Inserts the current block (before call) into the current scope, and
    // creates a new subscope for the true branch
    pub fn conditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
        condition: ir::JumpCondition,
    ) {
        let branch_from = &mut self.current_block;
        self.open_branch_path.push(branch_from.label);

        let (branch_true, branch_false, branch_convergence) = self
            .control_flow
            .create_conditional_branch(branch_from, loc, condition);

        self.open_convergences
            .rename_source(branch_from.label, branch_convergence.label);
        self.open_convergences
            .add(&[branch_true.label, branch_false.label], branch_convergence);

        let old_block = std::mem::replace(&mut self.current_block, branch_true);
        self.current_scope.insert_basic_block(old_block);

        let parent_scope = std::mem::replace(&mut self.current_scope, symtab::Scope::new());
        self.scope_stack.push(parent_scope);
    }

    pub fn create_loop(&mut self) -> usize {
        0
    }

    pub fn next_temp(&mut self, type_: Type) -> ir::Operand {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        self.current_scope
            .insert_variable(symtab::Variable::new(temp_name.clone(), Some(type_)));
        ir::Operand::new_as_temporary(temp_name)
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) {
        match &statement.operation {
            ir::Operations::Jump(_) => {
                panic!("FunctionWalkContext::append_statement_to_current_block does NOT support jumps!")
            }
            _ => {}
        }
        self.current_block.statements.push(statement);
    }
}

impl<'a> symtab::BasicBlockOwner for FunctionWalkContext<'a> {
    fn insert_basic_block(&mut self, _block: ir::BasicBlock) {
        unimplemented!("insert_basic_block not to be used by FunctionWalkContext");
    }

    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock> {
        for lookup_scope in self.scope_stack.iter().rev() {
            match lookup_scope.lookup_basic_block(label) {
                Some(block) => return Some(block),
                None => (),
            }
        }

        None
    }

    fn lookup_basic_block_mut(&mut self, label: usize) -> Option<&mut ir::BasicBlock> {
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
        for lookup_scope in self.scope_stack.iter().rev() {
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
        type_: &Type,
    ) -> Result<&symtab::TypeDefinition, symtab::UndefinedSymbol> {
        for lookup_scope in self.scope_stack.iter().rev() {
            match lookup_scope.lookup_type(type_) {
                Ok(type_) => return Ok(type_),
                _ => (),
            }
        }

        Err(symtab::UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_type_mut(
        &mut self,
        type_: &Type,
    ) -> Result<&mut symtab::TypeDefinition, symtab::UndefinedSymbol> {
        for lookup_scope in self.scope_stack.iter_mut().rev() {
            match lookup_scope.lookup_type_mut(type_) {
                Ok(type_) => return Ok(type_),
                _ => (),
            }
        }

        Err(symtab::UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_struct(&self, name: &str) -> Result<&symtab::StructRepr, symtab::UndefinedSymbol> {
        for lookup_scope in self.scope_stack.iter().rev() {
            match lookup_scope.lookup_struct(name) {
                Ok(struct_) => return Ok(struct_),
                _ => (),
            }
        }

        Err(symtab::UndefinedSymbol::struct_(name.into()))
    }
}

impl<'a> symtab::SelfTypeOwner for FunctionWalkContext<'a> {
    fn self_type(&self) -> &Type {
        self.self_type.as_ref().unwrap()
    }
}

impl<'a> symtab::ScopeOwnerships for FunctionWalkContext<'a> {}
impl<'a> symtab::ModuleOwnerships for FunctionWalkContext<'a> {}

impl<'a> Into<symtab::Function> for FunctionWalkContext<'a> {
    fn into(self) -> symtab::Function {
        assert!(self.scope_stack.is_empty());
        assert!(self.open_convergences.is_empty());
        assert!(self.open_branch_path.is_empty());

        symtab::Function::new(self.prototype, self.current_scope, self.control_flow)
    }
}

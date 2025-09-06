use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{
        ir,
        linearizer::{DefContext, *},
        symtab::{self, DefPathComponent},
        types,
    },
    trace,
};
pub struct FunctionWalkContext {
    symtab: Box<symtab::SymbolTable>,
    // definition path from the root of the symbol table to this function
    global_def_path: symtab::DefPath,
    // definition path from the root of the symbol table to wherever we are in the function
    full_def_path: symtab::DefPath,
    generics: GenericParamsContext,
    self_type: Option<types::Syntactic>,
    block_manager: BlockManager,
    values: ir::ValueInterner,
    // key for DefPathComponent::BasicBlock from self.def_path
    current_block: usize,
}

impl FunctionWalkContext {
    #[tracing::instrument(level = "debug")]
    pub fn new(
        parent_context: BasicDefContext,
        prototype: symtab::FunctionPrototype,
    ) -> Result<Self, symtab::SymbolError> {
        let self_type = parent_context.self_type();

        trace::trace!(
            "Self type for function {} is {:?}",
            prototype.name,
            self_type
        );

        let (mut symtab, parent_def_path, generics) = parent_context.take().unwrap();

        let (block_manager, start_block_label) = BlockManager::new();
        let my_def_path = {
            symtab.insert::<symtab::Function>(
                parent_def_path.clone(),
                symtab::Function::new(prototype.clone(), None),
            )?
        };

        for argument in prototype.arguments {
            symtab
                .insert::<symtab::Variable>(my_def_path.clone(), argument.clone())
                .unwrap();
        }

        let (_, unit_type_path) = symtab
            .lookup_with_path::<symtab::TypeDefinition>(&parent_def_path, &types::Syntactic::Unit)
            .unwrap();
        let unit_type = symtab.types.get_semantic(&unit_type_path).unwrap();

        let values = ir::ValueInterner::new(unit_type);

        Ok(Self {
            symtab,
            generics,
            global_def_path: my_def_path.clone(),
            full_def_path: my_def_path,
            self_type,
            block_manager: block_manager,
            values,
            current_block: start_block_label,
        })
    }

    pub fn self_variable(&mut self) -> Option<ir::ValueId> {
        let self_variable_path = self
            .global_def_path
            .clone()
            .with_component(DefPathComponent::Variable("self".into()))
            .unwrap();

        let self_variable_definition = match self.lookup_at::<symtab::Variable>(&self_variable_path)
        {
            Ok(def) => def,
            Err(_) => return None,
        };

        Some(self.values.id_for_variable_or_insert(self_variable_path))
    }

    fn new_subscope(&mut self) -> Result<(), symtab::SymbolError> {
        let next_subscope_index = self
            .symtab()
            .children(&self.def_path())
            .into_iter()
            .filter(|path| match path.last() {
                DefPathComponent::Scope(_) => true,
                _ => false,
            })
            .count();

        self.full_def_path.push(
            self.symtab
                .insert::<symtab::Scope>(self.def_path(), symtab::Scope::new(next_subscope_index))
                .unwrap()
                .pop()
                .unwrap(),
        )
    }

    fn pop_current_scope(&mut self) -> Result<(), block_manager::BranchError> {
        trace::trace!("pop current scope");

        match self.full_def_path.pop() {
            Some(symtab::DefPathComponent::Scope(_)) => Ok(()),
            _ => Err(block_manager::BranchError::NotBranched),
        }
    }

    // takes the label of the block to be made 'current'
    // returns mutable reference to the block which was previously current
    fn replace_current_block(&mut self, new_current: usize) -> &mut ir::BasicBlock {
        let old_current = self.current_block;
        self.current_block = new_current;
        let global_def_path = self.global_def_path.clone();

        trace::trace!(
            "replace current block ({}) with block {}",
            old_current,
            new_current,
        );

        self.block_manager.get_mut(&old_current).unwrap()
    }

    fn set_current_block(&mut self, label: usize) {
        // sanity check - look up the block to ensure it exists
        let _lookup_result = self.block_manager.get_mut(&label).unwrap();

        trace::trace!("set current block from {} to {}", self.current_block, label);

        self.current_block = label;
    }

    fn current_block_mut(&mut self) -> &mut ir::BasicBlock {
        self.block_manager.get_mut(&self.current_block).unwrap()
    }

    pub fn unit_value_id(&self) -> ir::ValueId {
        ir::ValueInterner::unit_value_id()
    }

    pub fn value_for_variable(&self, variable_def_path: &symtab::DefPath) -> &ir::ValueId {
        self.values
            .id_for_variable(variable_def_path)
            .expect(&format!(
                "Variable at def path {} has no ValueID",
                variable_def_path
            ))
    }

    pub fn value_for_variable_or_insert(
        &mut self,
        variable_def_path: symtab::DefPath,
    ) -> ir::ValueId {
        let variable_def = self
            .lookup_at::<symtab::Variable>(&variable_def_path)
            .expect("Variable must be defined to get its ValueId");

        self.values.id_for_variable_or_insert(variable_def_path)
    }

    pub fn value_for_id(&self, id: &ir::ValueId) -> Option<&ir::Value> {
        self.values.value_for_id(id)
    }

    pub fn value_id_for_constant(&mut self, constant: usize) -> &ir::ValueId {
        self.values.id_for_constant(constant)
    }

    pub fn type_definition_for_value_id(
        &self,
        id: &ir::ValueId,
    ) -> Option<&symtab::TypeDefinition> {
        let value = self.value_for_id(id).unwrap().clone();
        self.definition_for_semantic_type(&value.type_.unwrap())
    }

    pub fn finish_true_branch_switch_to_false(&mut self) -> Result<(), block_manager::BranchError> {
        trace::debug!("finish true branch, switch to false");

        let def_path = self.def_path();
        let FunctionWalkContext {
            block_manager,
            symtab,
            current_block,
            ..
        } = self;

        let false_block = block_manager
            .finish_true_branch_switch_to_false(self.current_block)
            .unwrap();
        self.replace_current_block(false_block);
        self.pop_current_scope()?;
        // scope management should be automatic within this module
        self.new_subscope().unwrap();
        Ok(())
    }

    pub fn finish_branch(&mut self) -> Result<(), block_manager::BranchError> {
        let def_path = self.def_path();
        let FunctionWalkContext {
            block_manager,
            symtab,
            current_block,
            ..
        } = self;

        let after_branch = block_manager.finish_branch(self.current_block)?;
        self.replace_current_block(after_branch);
        match self.pop_current_scope() {
            Ok(_) => Ok(()),
            Err(_) => Err(block_manager::BranchError::ScopeHandling),
        }
    }

    // create an unconditional branch from the current block, transparently setting the current
    // block to the target. Inserts the current block (before call) into the current scope
    pub fn unconditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
    ) -> Result<(), block_manager::BranchError> {
        trace::debug!("create unconditional branch from current block");

        let def_path = self.def_path();
        let FunctionWalkContext {
            block_manager,
            symtab,
            current_block,
            ..
        } = self;

        let branched_to_block = block_manager
            .create_unconditional_branch(self.current_block, loc)
            .unwrap();

        self.replace_current_block(branched_to_block);

        // scope management should be automatic within this module
        self.new_subscope().unwrap();
        Ok(())
    }

    // create a conditional branch from the current block, transparently setting the current block
    // to the true branch. Inserts the current block (before call) into the current scope, and
    // creates a new subscope for the true branch
    pub fn conditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
        condition: ir::lowered::operands::JumpCondition,
    ) -> Result<(), block_manager::BranchError> {
        trace::debug!("create conditional branch from current block");
        let def_path = self.def_path();
        let FunctionWalkContext {
            block_manager,
            symtab,
            current_block,
            ..
        } = self;

        let true_block = block_manager
            .create_conditional_branch(self.current_block, loc, condition)
            .unwrap();

        self.replace_current_block(true_block);
        // scope management should be automatic within this module
        self.new_subscope().unwrap();
        Ok(())
    }

    pub fn create_loop(&mut self, loc: SourceLoc) -> Result<usize, block_manager::LoopError> {
        trace::debug!("create loop");

        let def_path = self.def_path();
        let FunctionWalkContext {
            block_manager,
            symtab,
            current_block,
            ..
        } = self;

        let (loop_top_block, after_loop_label) =
            block_manager.create_loop(self.current_block, loc).unwrap();
        self.replace_current_block(loop_top_block);

        Ok(after_loop_label)
    }

    pub fn finish_loop(
        &mut self,
        loc: SourceLoc,
        loop_bottom_actions: Vec<ir::IrLine>,
    ) -> Result<(), block_manager::LoopError> {
        let def_path = self.def_path();
        {
            let FunctionWalkContext {
                block_manager,
                symtab,
                current_block,
                ..
            } = self;

            let loop_bottom = self
                .block_manager
                .finish_loop_1(self.current_block, loc.clone())
                .unwrap();
            // make our current block loop_bottom
            self.replace_current_block(loop_bottom);
        }

        {
            let FunctionWalkContext {
                block_manager,
                symtab,
                current_block,
                ..
            } = self;

            let after_loop = block_manager
                .finish_loop_2(self.current_block, loc, loop_bottom_actions)
                .unwrap();

            self.replace_current_block(after_loop);
        }
        Ok(())
    }

    pub fn create_switch(&mut self, loc: SourceLoc) -> Result<(), block_manager::BranchError> {
        let switch_block = self.block_manager.create_switch(self.current_block, loc)?;

        self.new_subscope().unwrap();
        let _ = self.replace_current_block(switch_block);

        Ok(())
    }

    // returns the label of the first block in the case
    pub fn create_switch_case(&mut self) -> Result<usize, block_manager::BranchError> {
        let case_label = self.block_manager.create_switch_case(self.current_block)?;

        self.new_subscope().unwrap();
        let _ = self.replace_current_block(case_label);

        Ok(case_label)
    }

    pub fn finish_switch_case(&mut self) -> Result<(), block_manager::BranchError> {
        let switch_label = {
            let def_path = self.def_path();
            let FunctionWalkContext {
                block_manager,
                symtab,
                current_block,
                ..
            } = self;

            block_manager.finish_switch_case(self.current_block)?
        };

        self.set_current_block(switch_label);
        self.pop_current_scope()
    }

    pub fn finish_switch(&mut self) -> Result<(), block_manager::BranchError> {
        let after_switch = self.block_manager.finish_switch(self.current_block)?;

        self.pop_current_scope().unwrap();

        let _ = self.replace_current_block(after_switch);

        Ok(())
    }

    pub fn next_temp(&mut self) -> ir::ValueId {
        self.values.next_temp()
    }

    pub fn append_jump_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operation::Lowered(ir::lowered::Operation::Jump(_)) => {
                self.current_block_mut().statements.push(statement);
                Ok(())
            }
            _ => Err(()),
        }
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operation::Lowered(ir::lowered::Operation::Jump(_)) => Err(()),
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

impl DefContext for FunctionWalkContext {
    fn symtab(&self) -> &symtab::SymbolTable {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut symtab::SymbolTable {
        &mut self.symtab
    }

    fn def_path(&self) -> symtab::DefPath {
        self.full_def_path.clone()
    }

    fn def_path_mut(&mut self) -> &mut symtab::DefPath {
        &mut self.full_def_path
    }

    fn generics(&self) -> &GenericParamsContext {
        &self.generics
    }

    fn generics_mut(&mut self) -> &mut GenericParamsContext {
        &mut self.generics
    }
}

impl FunctionWalkContext {
    fn take(
        mut self,
    ) -> Result<
        (
            Box<symtab::SymbolTable>,
            symtab::DefPath,
            GenericParamsContext,
            ir::ControlFlow,
        ),
        (),
    > {
        assert!(self.full_def_path.len() == self.global_def_path.len());

        let cf: ir::ControlFlow = self.block_manager.try_into()?;

        Ok((self.symtab, self.global_def_path, self.generics, cf))
    }
}

impl Into<BasicDefContext> for FunctionWalkContext {
    fn into(mut self) -> BasicDefContext {
        self.block_manager.finish(self.current_block).unwrap();

        let (mut symtab, mut path, generics, cf) = self.take().unwrap();
        let walked_function = symtab.lookup_at_mut::<symtab::Function>(&path).unwrap();
        if let Some(existing_cf) = walked_function.control_flow.replace(cf) {
            panic!(
                "Control flow already exists for function {}",
                walked_function.name()
            );
        }

        assert!(matches!(path.pop().unwrap(), DefPathComponent::Function(_)));

        BasicDefContext::with_path(symtab, path, generics)
    }
}

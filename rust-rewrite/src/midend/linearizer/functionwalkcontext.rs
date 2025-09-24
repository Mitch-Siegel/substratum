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
    current_local_def_path: symtab::DefPath,
    generics: GenericParamsContext,
    block_manager: ir::BlockManager,
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

        let (_, unit_type_path) = symtab
            .lookup_with_path::<symtab::TypeDefinition>(&parent_def_path, &types::Syntactic::Unit)
            .unwrap();
        let unit_type_id = symtab.types.get_semantic(&unit_type_path).unwrap();

        let my_def_path = {
            symtab.insert::<symtab::Function>(
                parent_def_path.clone(),
                symtab::Function::new(prototype.clone(), None),
            )?
        };
        let (block_manager, start_block_label) =
            ir::BlockManager::new(unit_type_id, my_def_path.clone());

        for argument in prototype.arguments {
            symtab
                .insert::<symtab::Variable>(my_def_path.clone(), argument.clone())
                .unwrap();
        }

        Ok(Self {
            symtab,
            generics,
            global_def_path: my_def_path.clone(),
            current_local_def_path: my_def_path,
            block_manager: block_manager,
            current_block: start_block_label,
        })
    }

    pub fn from_existing(
        symtab: Box<symtab::SymbolTable>,
        generics: GenericParamsContext,
        full_local_def_path: symtab::DefPath,
        block_manager: ir::BlockManager,
        current_block: usize,
    ) -> Self {
        Self {
            symtab,
            generics,
            global_def_path: full_local_def_path.clone().parent_function().unwrap(),
            current_local_def_path: full_local_def_path,
            block_manager,
            current_block,
        }
    }

    pub fn values(&self) -> &ir::ValueInterner {
        self.block_manager.values()
    }

    pub fn values_mut(&mut self) -> &mut ir::ValueInterner {
        self.block_manager.values_mut()
    }

    pub fn self_variable(&mut self) -> Option<ir::ValueId> {
        let self_variable_path = self
            .global_def_path
            .clone()
            .with_component(DefPathComponent::Variable("self".into()))
            .unwrap();

        match self.lookup_at::<symtab::Variable>(&self_variable_path) {
            Ok(def) => def,
            Err(_) => return None,
        };

        Some(self.values_mut().id_for_variable(self_variable_path))
    }

    // reserves a subscope, returning its defpath
    fn reserve_subscope(&mut self) -> symtab::DefPath {
        let next_subscope_index = self
            .symtab()
            .children(&self.def_path())
            .into_iter()
            .filter(|path| match path.last() {
                DefPathComponent::Scope(_) => true,
                _ => false,
            })
            .count();

        self.symtab
            .insert::<symtab::Scope>(self.def_path(), symtab::Scope::new(next_subscope_index))
            .unwrap()
    }

    // takes the label of the block to be made 'current'
    // returns mutable reference to the block which was previously current
    fn replace_current_block(&mut self, new_current: usize) -> &mut ir::BasicBlock {
        let old_current = self.current_block;

        trace::trace!(
            "replace current block ({}) with block {}",
            old_current,
            new_current,
        );

        let old_current_block = self.block_manager.get_mut(&old_current).unwrap();
        let expected_def_path = old_current_block.def_path().clone();
        assert!(
            (self.def_path() == expected_def_path)
                || self.def_path().is_prefix_of(&expected_def_path)
        );
        self.current_local_def_path = self
            .block_manager
            .get(&self.current_block)
            .unwrap()
            .def_path()
            .clone();

        self.current_block = new_current;
        self.block_manager.get_mut(&old_current).unwrap()
    }

    fn set_current_block(&mut self, label: usize) {
        // sanity check - look up the block to ensure it exists
        let lookup_result = self.block_manager.get_mut(&label).unwrap();

        trace::trace!("set current block from {} to {}", self.current_block, label);

        self.current_block = label;
        self.current_local_def_path = lookup_result.def_path().clone();
    }

    fn current_block_mut(&mut self) -> &mut ir::BasicBlock {
        self.block_manager.get_mut(&self.current_block).unwrap()
    }

    pub fn finish_true_branch_switch_to_false(
        &mut self,
        loc: SourceLoc,
    ) -> Result<(), ir::block_manager::BranchError> {
        trace::debug!("finish true branch, switch to false");

        let false_block = self
            .block_manager
            .finish_true_branch_switch_to_false(self.current_block, loc)
            .unwrap();
        self.replace_current_block(false_block);
        Ok(())
    }

    pub fn finish_branch(&mut self, loc: SourceLoc) -> Result<(), ir::block_manager::BranchError> {
        let after_branch = self.block_manager.finish_branch(self.current_block, loc)?;
        self.replace_current_block(after_branch);
        Ok(())
    }

    // create an unconditional branch from the current block, transparently setting the current
    // block to the target. Inserts the current block (before call) into the current scope
    pub fn unconditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
    ) -> Result<(), ir::block_manager::BranchError> {
        trace::debug!("create unconditional branch from current block");

        let true_scope = self.reserve_subscope();

        let branched_to_block = self
            .block_manager
            .create_unconditional_branch(
                self.current_block,
                loc,
                self.def_path().clone(),
                true_scope,
            )
            .unwrap();

        self.replace_current_block(branched_to_block);

        Ok(())
    }

    // create a conditional branch from the current block, transparently setting the current block
    // to the true branch. Inserts the current block (before call) into the current scope, and
    // creates a new subscope for the true branch
    pub fn conditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
        condition: ir::lowered::operands::JumpCondition,
    ) -> Result<(), ir::block_manager::BranchError> {
        trace::debug!("create conditional branch from current block");

        let true_scope = self.reserve_subscope();
        let false_scope = self.reserve_subscope();

        let true_block = self
            .block_manager
            .create_conditional_branch(
                self.current_block,
                loc,
                condition,
                self.def_path(),
                true_scope,
                false_scope,
            )
            .unwrap();

        self.replace_current_block(true_block);
        Ok(())
    }

    pub fn create_loop(&mut self, loc: SourceLoc) -> Result<usize, ir::block_manager::BranchError> {
        trace::debug!("create loop");

        let loop_scope = self.reserve_subscope();

        let (loop_top_block, after_loop_label) = self
            .block_manager
            .create_loop(self.current_block, loc, self.def_path(), loop_scope)
            .unwrap();
        self.replace_current_block(loop_top_block);

        Ok(after_loop_label)
    }

    pub fn finish_loop(
        &mut self,
        loc: SourceLoc,
        loop_bottom_actions: Vec<ir::IrLine>,
    ) -> Result<(), ir::block_manager::BranchError> {
        let loop_bottom = self
            .block_manager
            .finish_loop_1(self.current_block, loc.clone())
            .unwrap();
        // make our current block loop_bottom
        self.replace_current_block(loop_bottom);

        let after_loop = self
            .block_manager
            .finish_loop_2(self.current_block, loc, loop_bottom_actions)
            .unwrap();

        self.replace_current_block(after_loop);
        Ok(())
    }

    pub fn create_switch(&mut self, loc: SourceLoc) -> Result<(), ir::block_manager::BranchError> {
        let switch_scope = self.reserve_subscope();

        let switch_block = self.block_manager.create_switch(
            self.current_block,
            loc,
            self.def_path(),
            switch_scope,
        )?;

        self.replace_current_block(switch_block);

        Ok(())
    }

    // returns the label of the first block in the case
    pub fn create_switch_case(&mut self) -> Result<usize, ir::block_manager::BranchError> {
        let case_scope = self.reserve_subscope();

        let case_label = self
            .block_manager
            .create_switch_case(self.current_block, case_scope)?;

        let _ = self.replace_current_block(case_label);

        Ok(case_label)
    }

    pub fn finish_switch_case(
        &mut self,
        loc: SourceLoc,
    ) -> Result<(), ir::block_manager::BranchError> {
        let switch_label = self
            .block_manager
            .finish_switch_case(self.current_block, loc)?;

        self.set_current_block(switch_label);
        Ok(())
    }

    pub fn finish_switch(&mut self, loc: SourceLoc) -> Result<(), ir::block_manager::BranchError> {
        let after_switch = self.block_manager.finish_switch(self.current_block, loc)?;

        self.replace_current_block(after_switch);

        Ok(())
    }

    pub fn append_jump_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operation::Lowered(ir::lowered::Operation::Jump(_)) => {
                self.current_block_mut().append(statement);
                Ok(())
            }
            _ => Err(()),
        }
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operation::Lowered(ir::lowered::Operation::Jump(_)) => Err(()),
            _ => {
                self.current_block_mut().append(statement);
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
        self.current_local_def_path.clone()
    }

    fn def_path_mut(&mut self) -> &mut symtab::DefPath {
        &mut self.current_local_def_path
    }

    fn generics(&self) -> &GenericParamsContext {
        &self.generics
    }

    fn generics_mut(&mut self) -> &mut GenericParamsContext {
        &mut self.generics
    }
}

impl FunctionWalkContext {
    pub fn take(
        self,
    ) -> Result<
        (
            Box<symtab::SymbolTable>,
            symtab::DefPath,
            GenericParamsContext,
            ir::BlockManager,
        ),
        (),
    > {
        assert!(self.current_local_def_path.len() == self.global_def_path.len());

        Ok((
            self.symtab,
            self.global_def_path,
            self.generics,
            self.block_manager,
        ))
    }
}

impl Into<BasicDefContext> for FunctionWalkContext {
    fn into(mut self) -> BasicDefContext {
        self.block_manager.finish(self.current_block).unwrap();

        let (mut symtab, mut path, generics, manager) = self.take().unwrap();
        let walked_function = symtab.lookup_at_mut::<symtab::Function>(&path).unwrap();
        if let Some(_existing_cf) = walked_function
            .control_flow
            .replace(ir::ControlFlow::from(manager))
        {
            panic!(
                "Control flow already exists for function {}",
                walked_function.name()
            );
        }

        assert!(matches!(path.pop().unwrap(), DefPathComponent::Function(_)));

        BasicDefContext::new(symtab, path, generics)
    }
}

use crate::{
    frontend::sourceloc::SourceLocWithMod,
    midend::{
        ir,
        linearizer::*,
        symtab::{self, DefContext, DefPathComponent},
        types,
    },
    trace,
};
pub struct FunctionWalkContext {
    symtab: Box<symtab::SymbolTable>,
    // definition path from the root of the symbol table to this function
    global_def_path: symtab::DefPath,
    generics: symtab::GenericParamsContext,
    self_type: Option<types::Type>,
    block_manager: BlockManager,
    // definition path starting after this function
    relative_local_def_path: symtab::DefPath,
    values: ir::ValueInterner,
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

        let (mut symtab, parent_def_path, generics) = parent_context.take().unwrap();

        let (block_manager, start_block) = BlockManager::new();
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
            generics,
            global_def_path: my_def_path,
            self_type,
            block_manager: block_manager,
            relative_local_def_path: symtab::DefPath::new(my_def_path_component),
            values,
            current_block: start_block_label,
        })
    }

    fn new_subscope(&mut self) -> Result<(), symtab::SymbolError> {
        self.relative_local_def_path.push(
            self.symtab
                .insert::<symtab::Scope>(self.def_path(), symtab::Scope::new(0))
                .unwrap()
                .pop()
                .unwrap(),
        )
    }

    fn pop_current_scope(&mut self) -> Result<(), block_manager::BranchError> {
        trace::trace!("pop current scope");

        match self.relative_local_def_path.pop() {
            Some(symtab::DefPathComponent::Scope(_)) => Ok(()),
            _ => Err(block_manager::BranchError::NotBranched),
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
            .lookup_at_mut::<ir::BasicBlock>(
                &global_def_path
                    .clone()
                    .with_component(DefPathComponent::BasicBlock(old_current_label))
                    .unwrap(),
            )
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
            .finish_true_branch_switch_to_false(
                symtab
                    .lookup_mut::<ir::BasicBlock>(&def_path, current_block)
                    .unwrap(),
            )
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

        let after_branch = block_manager.finish_branch(
            symtab
                .lookup_mut::<ir::BasicBlock>(&def_path, current_block)
                .unwrap(),
        )?;
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
        loc: SourceLocWithMod,
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
            .create_unconditional_branch(
                symtab
                    .lookup_mut::<ir::BasicBlock>(&def_path, current_block)
                    .unwrap(),
                loc,
            )
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
        loc: SourceLocWithMod,
        condition: ir::JumpCondition,
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
            .create_conditional_branch(
                symtab
                    .lookup_mut::<ir::BasicBlock>(&def_path, current_block)
                    .unwrap(),
                loc,
                condition,
            )
            .unwrap();

        self.replace_current_block(true_block);
        // scope management should be automatic within this module
        self.new_subscope().unwrap();
        Ok(())
    }

    pub fn create_loop(
        &mut self,
        loc: SourceLocWithMod,
    ) -> Result<usize, block_manager::LoopError> {
        trace::debug!("create loop");

        let def_path = self.def_path();
        let FunctionWalkContext {
            block_manager,
            symtab,
            current_block,
            ..
        } = self;

        let current_block_mut = symtab
            .lookup_mut::<ir::BasicBlock>(&def_path, current_block)
            .unwrap();
        let (loop_top_block, after_loop_label) =
            block_manager.create_loop(current_block_mut, loc).unwrap();
        self.replace_current_block(loop_top_block);

        Ok(after_loop_label)
    }

    pub fn finish_loop(
        &mut self,
        loc: SourceLocWithMod,
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

            let loop_bottom = block_manager
                .finish_loop_1(
                    symtab
                        .lookup_mut::<ir::BasicBlock>(&def_path, current_block)
                        .unwrap(),
                    loc.clone(),
                )
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
                .finish_loop_2(
                    symtab
                        .lookup_mut::<ir::BasicBlock>(&def_path, current_block)
                        .unwrap(),
                    loc,
                    loop_bottom_actions,
                )
                .unwrap();

            self.replace_current_block(after_loop);
        }
        Ok(())
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

    fn generics_mut(&mut self) -> &mut symtab::GenericParamsContext {
        &mut self.generics
    }

    fn take(
        self,
    ) -> Result<
        (
            Box<symtab::SymbolTable>,
            symtab::DefPath,
            symtab::GenericParamsContext,
        ),
        (),
    > {
        assert!(self.relative_local_def_path.is_empty());
        // TODO: manage control flow, etc...
        Ok((self.symtab, self.global_def_path, self.generics))
    }
}

impl Into<symtab::BasicDefContext> for FunctionWalkContext {
    fn into(self) -> symtab::BasicDefContext {
        let (symtab, path, generics) = self.take().unwrap();
        symtab::BasicDefContext::with_path(symtab, path, generics)
    }
}

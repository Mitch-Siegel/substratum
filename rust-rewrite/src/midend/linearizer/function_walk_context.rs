use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{
        ir,
        symtab::{self},
        types,
    },
    trace,
};
pub struct FunctionWalkContext {
    // definition path from the root of the symbol table to this function
    block_manager: ir::BlockManager,
    // key for DefPathComponent::BasicBlock from self.def_path
    current_block: usize,
}

impl FunctionWalkContext {
    #[tracing::instrument(level = "debug")]
    pub fn new(
        prototype: symtab::FunctionPrototype,
        def_path: symtab::DefPath,
        unit_type: types::Semantic,
    ) -> Self {
        let (block_manager, start_block_label) = ir::BlockManager::new(unit_type, def_path.clone());

        Self {
            block_manager: block_manager,
            current_block: start_block_label,
        }
    }

    pub fn from_existing(block_manager: ir::BlockManager, current_block: usize) -> Self {
        Self {
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

    // takes the label of the block to be made 'current'
    // returns mutable reference to the block which was previously current
    fn replace_current_block(&mut self, new_current: usize) -> &mut ir::BasicBlock {
        let old_current = self.current_block;

        trace::trace!(
            "replace current block ({}) with block {}",
            old_current,
            new_current,
        );

        self.current_block = new_current;
        self.block_manager.get_mut(&old_current).unwrap()
    }

    fn set_current_block(&mut self, label: usize) -> &symtab::DefPath {
        // sanity check - look up the block to ensure it exists
        self.block_manager.get_mut(&label).unwrap();

        trace::trace!("set current block from {} to {}", self.current_block, label);

        self.current_block = label;
        self.block_manager
            .get(&self.current_block)
            .unwrap()
            .def_path()
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
        parent_scope_def_path: symtab::DefPath,
        true_scope_def_path: symtab::DefPath,
    ) -> Result<(), ir::block_manager::BranchError> {
        trace::debug!("create unconditional branch from current block");

        let branched_to_block = self
            .block_manager
            .create_unconditional_branch(
                self.current_block,
                loc,
                parent_scope_def_path,
                true_scope_def_path,
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
        parent_scope_def_path: symtab::DefPath,
        true_scope_def_path: symtab::DefPath,
        false_scope_def_path: symtab::DefPath,
    ) -> Result<(), ir::block_manager::BranchError> {
        trace::debug!("create conditional branch from current block");

        let true_block = self
            .block_manager
            .create_conditional_branch(
                self.current_block,
                loc,
                condition,
                parent_scope_def_path,
                true_scope_def_path,
                false_scope_def_path,
            )
            .unwrap();

        self.replace_current_block(true_block);
        Ok(())
    }

    pub fn create_loop(
        &mut self,
        loc: SourceLoc,
        parent_scope_def_path: symtab::DefPath,
        loop_scope_def_path: symtab::DefPath,
    ) -> Result<usize, ir::block_manager::BranchError> {
        trace::debug!("create loop");

        let (loop_top_block, after_loop_label) = self
            .block_manager
            .create_loop(
                self.current_block,
                loc,
                parent_scope_def_path,
                loop_scope_def_path,
            )
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

    pub fn create_switch(
        &mut self,
        loc: SourceLoc,
        parent_scope_def_path: symtab::DefPath,
        switch_scope_def_path: symtab::DefPath,
    ) -> Result<(), ir::block_manager::BranchError> {
        let switch_block = self.block_manager.create_switch(
            self.current_block,
            loc,
            parent_scope_def_path,
            switch_scope_def_path,
        )?;

        self.replace_current_block(switch_block);

        Ok(())
    }

    // returns the label of the first block in the case
    pub fn create_switch_case(
        &mut self,
        case_scope_def_path: symtab::DefPath,
    ) -> Result<usize, ir::block_manager::BranchError> {
        let case_label = self
            .block_manager
            .create_switch_case(self.current_block, case_scope_def_path)?;

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
                self.current_block_mut().push(statement);
                Ok(())
            }
            _ => Err(()),
        }
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operation::Lowered(ir::lowered::Operation::Jump(_)) => Err(()),
            _ => {
                self.current_block_mut().push(statement);
                Ok(())
            }
        }
    }

    pub fn finish(&mut self) {
        self.block_manager.finish(self.current_block).unwrap();
    }
}

impl std::fmt::Debug for FunctionWalkContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Function walk context {:?}", self.block_manager)
    }
}

impl FunctionWalkContext {
    // TODO: emplace control flow for function at call site
    pub fn take(self) -> ir::ControlFlow {
        ir::ControlFlow::from(self.block_manager)
    }
}

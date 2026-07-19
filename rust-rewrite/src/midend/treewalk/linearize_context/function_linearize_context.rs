use std::collections::HashSet;

use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{
        symtab::Path,
        treewalk::{
            PathedCtx, PathedCtxTrait, UnpathedCtxTrait, UnpathedLinearizeCtx,
            UnpathedLinearizeCtxTrait,
        },
        *,
    },
    trace,
};

pub(crate) struct UnpathedFunctionLinearizeCtx {
    base: UnpathedLinearizeCtx,
    function_path: symtab::ValuePath,
    function: WipFunction,
}

impl UnpathedFunctionLinearizeCtx {
    #[tracing::instrument(level = "debug")]
    pub(crate) fn new(
        ctx: PathedCtx<UnpathedLinearizeCtx, impl symtab::ValueOwner>,
        // symtab: symtab::SymbolTable,
        // function_path: symtab::ValuePath,
        prototype: symtab::values::function::FunctionPrototype,
        // def_path: symtab::ValuePath,
        unit_type: types::Semantic,
        arg_def_paths: Vec<symtab::ValuePath>,
    ) -> Self {
        let function_path = ctx.path().clone().with_child_value(prototype.name.clone());
        let base = ctx.into_result(()).unwrap().1;

        Self {
            base,
            function_path: function_path.clone(),
            function: WipFunction::new(prototype, function_path, unit_type, arg_def_paths),
        }
    }

    pub(crate) fn finalize<P>(
        self,
        path: P,
        _return_value_id: ir::ValueId,
    ) -> treewalk::LinearizeResult<symtab::values::Function, UnpathedLinearizeCtx>
    where
        P: symtab::Path,
    {
        assert_eq!(
            Into::<symtab::RawPath>::into(self.function_path),
            Into::<symtab::RawPath>::into(path)
        );

        let function = self.function.finish().unwrap();

        Ok((function, self.base))
    }
}

impl UnpathedCtxTrait for UnpathedFunctionLinearizeCtx {
    fn with_path<P: symtab::Path>(self, path: P) -> PathedCtx<Self, P> {
        if !self.function_path.is_prefix_of(&path) {
            panic!("Required for function path to be prefix of function context path");
        }

        PathedCtx {
            unpathed: self,
            path,
        }
    }
}

impl UnpathedLinearizeCtxTrait for UnpathedFunctionLinearizeCtx {}

impl symtab::SymtabBase for UnpathedFunctionLinearizeCtx {
    fn insert(
        &mut self,
        path: symtab::RawPath,
        maybe_symbol: Option<symtab::SymbolDef>,
    ) -> Result<symtab::RawPath, symtab::SymbolError> {
        self.base.insert(path, maybe_symbol)
    }

    fn lookup_at(
        &self,
        path: &symtab::RawPath,
    ) -> Result<Option<&symtab::SymbolDef>, symtab::SymbolError> {
        self.base.lookup_at(path)
    }

    fn lookup_at_mut(
        &mut self,
        path: &symtab::RawPath,
    ) -> Result<Option<&mut symtab::SymbolDef>, symtab::SymbolError> {
        self.base.lookup_at_mut(path)
    }

    fn insert_use_declaration(
        &mut self,
        path: symtab::RawPath,
        use_declaration: symtab::UseDeclaration,
    ) {
        self.base.insert_use_declaration(path, use_declaration);
    }

    fn get_use_declarations_at(
        &self,
        path: &symtab::RawPath,
    ) -> Option<&BTreeSet<symtab::UseDeclaration>> {
        self.base.get_use_declarations_at(path)
    }
}

impl symtab::Symtab for UnpathedFunctionLinearizeCtx {
    fn semantic_type_for_syntactic(
        &self,
        search_def_path: &impl symtab::Path,
        generic_params: crate::midend::types::ParamSubstMap,
        ty_: &crate::midend::types::Syntactic,
    ) -> Result<crate::midend::types::Semantic, symtab::SymbolError> {
        self.base
            .semantic_type_for_syntactic(search_def_path, generic_params, ty_)
    }

    fn create_impl(
        &mut self,
        impl_parent_path: symtab::RawPath,
        impl_for_path: symtab::TypePath,
    ) -> Result<symtab::ImplPath, symtab::SymbolError> {
        self.base.create_impl(impl_parent_path, impl_for_path)
    }

    fn get_impls_for(
        &self,
        path: &symtab::TypePath,
    ) -> Result<&HashSet<symtab::ImplPath>, symtab::SymbolError> {
        self.base.get_impls_for(path)
    }
}

pub(crate) struct WipFunction {
    prototype: symtab::values::FunctionPrototype,
    block_manager: ir::BlockManager,
    current_block: usize,
}

impl WipFunction {
    #[tracing::instrument(level = "debug")]
    pub(crate) fn new(
        prototype: symtab::values::FunctionPrototype,
        def_path: symtab::ValuePath,
        unit_type: types::Semantic,
        arg_def_paths: Vec<symtab::ValuePath>,
    ) -> Self {
        let (mut block_manager, start_block_label) =
            ir::BlockManager::new(unit_type, def_path.clone());

        for arg in arg_def_paths {
            let id = block_manager.values_mut().id_for_path(arg.clone());
            println!(
                "arg {}: id {}: value {:?}",
                arg,
                id,
                block_manager.values().value_for_id(&id).unwrap()
            );
        }

        Self {
            prototype,
            block_manager,
            current_block: start_block_label,
        }
    }

    pub(crate) fn values(&self) -> &ir::ValueInterner {
        self.block_manager.values()
    }

    pub(crate) fn values_mut(&mut self) -> &mut ir::ValueInterner {
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

    fn set_current_block(&mut self, label: usize) -> &symtab::ValuePath {
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

    pub(crate) fn finish_true_branch_switch_to_false(
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

    pub(crate) fn finish_branch(
        &mut self,
        loc: SourceLoc,
    ) -> Result<(), ir::block_manager::BranchError> {
        let after_branch = self.block_manager.finish_branch(self.current_block, loc)?;
        self.replace_current_block(after_branch);
        Ok(())
    }

    // create an unconditional branch from the current block, transparently setting the current
    // block to the target. Inserts the current block (before call) into the current scope
    pub(crate) fn unconditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
        parent_scope_def_path: symtab::ValuePath,
        true_scope_def_path: symtab::ValuePath,
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
    pub(crate) fn conditional_branch_from_current(
        &mut self,
        loc: SourceLoc,
        condition: ir::lowered::operands::JumpCondition,
        parent_scope_def_path: symtab::ValuePath,
        true_scope_def_path: symtab::ValuePath,
        false_scope_def_path: symtab::ValuePath,
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

    pub(crate) fn create_loop(
        &mut self,
        loc: SourceLoc,
        parent_scope_def_path: symtab::ValuePath,
        loop_scope_def_path: symtab::ValuePath,
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

    pub(crate) fn finish_loop(
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

    pub(crate) fn create_switch(
        &mut self,
        loc: SourceLoc,
        parent_scope_def_path: symtab::ValuePath,
        switch_scope_def_path: symtab::ValuePath,
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
    pub(crate) fn create_switch_case(
        &mut self,
        case_scope_def_path: symtab::ValuePath,
    ) -> Result<usize, ir::block_manager::BranchError> {
        let case_label = self
            .block_manager
            .create_switch_case(self.current_block, case_scope_def_path)?;

        let _ = self.replace_current_block(case_label);

        Ok(case_label)
    }

    pub(crate) fn finish_switch_case(
        &mut self,
        loc: SourceLoc,
    ) -> Result<(), ir::block_manager::BranchError> {
        let switch_label = self
            .block_manager
            .finish_switch_case(self.current_block, loc)?;

        self.set_current_block(switch_label);
        Ok(())
    }

    pub(crate) fn finish_switch(
        &mut self,
        loc: SourceLoc,
    ) -> Result<(), ir::block_manager::BranchError> {
        let after_switch = self.block_manager.finish_switch(self.current_block, loc)?;

        self.replace_current_block(after_switch);

        Ok(())
    }

    pub(crate) fn append_jump_to_current_block(&mut self, statement: ir::IrLine) -> Result<(), ()> {
        match &statement.operation {
            ir::Operation::Lowered(ir::lowered::Operation::Jump(_)) => {
                self.current_block_mut().push(statement);
                Ok(())
            }
            _ => Err(()),
        }
    }

    pub(crate) fn append_statement_to_current_block(
        &mut self,
        statement: ir::IrLine,
    ) -> Result<(), ()> {
        match &statement.operation {
            ir::Operation::Lowered(ir::lowered::Operation::Jump(_)) => Err(()),
            _ => {
                self.current_block_mut().push(statement);
                Ok(())
            }
        }
    }

    pub(crate) fn resolve_final_convergence(&mut self) {
        self.block_manager
            .resolve_final_convergence(self.current_block)
            .unwrap();
    }

    pub(crate) fn finish(self) -> Result<symtab::values::Function, ir::block_manager::BranchError> {
        self.block_manager.ensure_finished()?;

        let (blocks, values) = self.block_manager.try_take().unwrap();

        Ok(symtab::values::Function::new(
            self.prototype,
            Some(ir::ControlFlow::new(blocks, values)),
        ))
    }
}

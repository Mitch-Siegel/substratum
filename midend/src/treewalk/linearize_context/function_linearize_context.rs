use frontend::sourceloc;

use crate::{
    ir::{self, block_manager::branch},
    symtab::{self, Path, Symtab, SymtabBase},
    treewalk::{
        PathedCtx, PathedCtxTrait, UnpathedCtxTrait, UnpathedLinearizeCtx,
        UnpathedLinearizeCtxTrait,
    },
    types,
};

pub(crate) struct UnpathedFunctionLinearizeCtx<B: branch::Branch> {
    pub(super) base: UnpathedLinearizeCtx,
    pub(super) function_path: symtab::ValuePath,
    pub(super) prototype: symtab::values::FunctionPrototype,
    pub(super) block_manager: ir::BlockManager<B>,
}

impl<B: branch::Branch> UnpathedFunctionLinearizeCtx<B> {
    #[trace::instrument(level = "debug")]
    pub(crate) fn new(
        ctx: PathedCtx<UnpathedLinearizeCtx, impl symtab::ValueOwner>,
        prototype: symtab::values::function::FunctionPrototype,
        arg_def_paths: &Vec<symtab::ValuePath>,
        start_loc: sourceloc::SourceLoc,
        end_loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<branch::UnfinishedFunction> {
        let function_path = ctx.path().clone().with_child_value(prototype.name.clone());
        let base = ctx.into_result(()).unwrap().1;

        let block_manager: ir::BlockManager<branch::UnfinishedFunction> =
            ir::BlockManager::<branch::UnfinishedFunction>::new(&function_path, start_loc, end_loc);

        UnpathedFunctionLinearizeCtx {
            base,
            function_path,
            prototype,
            block_manager,
        }
    }

    pub(crate) fn into_erased(self) -> UnpathedFunctionLinearizeCtx<Box<dyn branch::Branch>> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.erase(),
        }
    }

    pub(crate) fn reserve_subscope(
        &mut self,
        parent_path: impl symtab::ScopeOwner,
    ) -> symtab::ScopePath {
        let next_subscope_index = self
            .symtab()
            .children_of_path(&parent_path)
            .into_iter()
            .filter(|path| matches!(path.last(), symtab::PathSegment::Scope(_)))
            .count();

        let reserved_scope_id = symtab::ScopeId(next_subscope_index);

        self.symtab_mut()
            .declare_scope(parent_path.with_child_scope(reserved_scope_id))
            .unwrap()
    }

    pub(crate) fn into_pathed_ctx(
        mut self,
        function_path: symtab::ValuePath,
    ) -> PathedCtx<Self, symtab::ScopePath> {
        let main_scope = self.reserve_subscope(function_path);
        self.with_path(main_scope)
    }

    pub(crate) fn values(&self) -> &ir::ValueInterner<Option<types::Syntactic>> {
        self.block_manager.values()
    }

    pub(crate) fn values_mut(&mut self) -> &mut ir::ValueInterner<Option<types::Syntactic>> {
        self.block_manager.values_mut()
    }

    pub(crate) fn append_statement_to_current_block(&mut self, line: ir::IrLine) {
        self.block_manager.current_block_mut().push(line);
    }
}

impl<B: branch::Branch> UnpathedCtxTrait for UnpathedFunctionLinearizeCtx<B> {
    fn with_path<P: symtab::Path>(self, path: P) -> PathedCtx<Self, P> {
        assert!(
            self.function_path.is_prefix_of(&path),
            "Required for function path to be prefix of function context path"
        );

        PathedCtx {
            unpathed: self,
            path,
        }
    }

    fn symtab(&self) -> &impl symtab::Symtab {
        self.base.symtab()
    }

    fn symtab_mut(&mut self) -> &mut impl symtab::Symtab {
        self.base.symtab_mut()
    }

    fn symtab_and_types(&mut self) -> (&mut impl symtab::Symtab, &mut types::Interner) {
        self.base.symtab_and_types()
    }
}

impl<B: branch::Branch> UnpathedLinearizeCtxTrait for UnpathedFunctionLinearizeCtx<B> {}

impl<B: branch::Branch> UnpathedFunctionLinearizeCtx<B> {
    pub(crate) fn create_unconditional_branch(
        self,
        desc: branch::BranchDesc,
        parent_def_path: symtab::ScopePath,
        true_def_path: symtab::ScopePath,
    ) -> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::Unconditional>> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.create_unconditional_branch(
                desc,
                parent_def_path,
                true_def_path,
            ),
        }
    }

    pub(crate) fn create_double_conditional_branch(
        self,
        loc: frontend::sourceloc::SourceLoc,
        after: frontend::sourceloc::SourceLoc,
        jump_condition: ir::lowered::operands::JumpCondition,
        true_data: branch::BranchTarget,
        false_data: branch::BranchTarget,
        parent_def_path: symtab::ScopePath,
    ) -> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::DoubleBranchTrue>> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.create_double_branch(
                branch::BranchDesc { loc, after },
                jump_condition,
                true_data,
                false_data,
                parent_def_path,
            ),
        }
    }

    pub(crate) fn create_single_conditional_branch(
        self,
        loc: frontend::sourceloc::SourceLoc,
        after: frontend::sourceloc::SourceLoc,
        jump_condition: ir::lowered::operands::JumpCondition,
        true_data: branch::BranchTarget,
        parent_def_path: symtab::ScopePath,
    ) -> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::SingleBranchTrue>> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.create_single_branch(
                branch::BranchDesc { loc, after },
                jump_condition,
                true_data,
                parent_def_path,
            ),
        }
    }

    pub(crate) fn create_loop(
        self,
        desc: branch::BranchDesc,
        hdr_loc: sourceloc::SourceLoc,
        body_loc: sourceloc::SourceLoc,
        ftr_loc: sourceloc::SourceLoc,
        parent_def_path: symtab::ScopePath,
        loop_def_path: symtab::ScopePath,
    ) -> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopHeader>> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.create_loop(
                desc,
                hdr_loc,
                body_loc,
                ftr_loc,
                parent_def_path,
                loop_def_path,
            ),
        }
    }
}

impl<B: ir::block_manager::branch::Branch>
    UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::Unconditional>>
{
    pub(crate) fn finish_unconditional_branch(
        self,
        loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<B> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.finish_unconditional_branch(loc),
        }
    }
}

impl<B: branch::Branch>
    UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::DoubleBranchTrue>>
{
    pub(crate) fn finish_true_branch_switch_to_false(
        self,
        loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::DoubleBranchFalse>> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.finish_true_branch_switch_to_false(loc),
        }
    }
}

impl<B: branch::Branch>
    UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::DoubleBranchFalse>>
{
    pub(crate) fn finish_false_branch(
        self,
        loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<B> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.finish_branch(loc),
        }
    }
}

impl<B: branch::Branch>
    UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::SingleBranchTrue>>
{
    pub(crate) fn finish_true_branch(
        self,
        loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<B> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.finish_branch(loc),
        }
    }
}

impl<B: branch::Branch> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopHeader>> {
    pub(crate) fn finish_loop_header(
        self,
        loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopBody>> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.finish_loop_header(loc),
        }
    }

    pub(crate) fn after_loop_label(&self) -> usize {
        self.block_manager.branch_state().branch.after_loop_label()
    }
}

impl<B: branch::Branch> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopBody>> {
    pub(crate) fn finish_loop_body(
        self,
        loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopFooter>> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.finish_loop_body(loc),
        }
    }
}

impl<B: branch::Branch> UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopFooter>> {
    pub(crate) fn finish_loop_footer(
        self,
        loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<B> {
        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.finish_loop_footer(loc),
        }
    }
}

impl UnpathedFunctionLinearizeCtx<branch::UnfinishedFunction> {
    pub(crate) fn finish_to_epilogue(
        self,
        path: &symtab::ScopePath,
        loc: sourceloc::SourceLoc,
    ) -> UnpathedFunctionLinearizeCtx<branch::Epilogue> {
        // fact-check that we are finalizing from subscope 0 and nowhere else
        assert_eq!(
            Into::<symtab::RawPath>::into(self.function_path.clone()),
            Into::<symtab::RawPath>::into(path.clone().split_last().0.unwrap())
        );

        UnpathedFunctionLinearizeCtx {
            base: self.base,
            function_path: self.function_path,
            prototype: self.prototype,
            block_manager: self.block_manager.finish_to_epilogue(loc),
        }
    }
}

impl UnpathedFunctionLinearizeCtx<branch::Epilogue> {
    pub(crate) fn take(
        self,
        path: &symtab::ScopePath,
    ) -> (symtab::values::Function, UnpathedLinearizeCtx) {
        // fact-check that we are finalizing from subscope 0 and nowhere else
        assert_eq!(
            Into::<symtab::RawPath>::into(self.function_path),
            Into::<symtab::RawPath>::into(path.clone().split_last().0.unwrap())
        );

        let cf = self.block_manager.into();

        let f = symtab::values::Function::new(self.prototype, Some(cf));
        (f, self.base)
    }
}

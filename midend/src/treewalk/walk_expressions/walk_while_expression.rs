use frontend::ast::{self, Ast};

use crate::{
    ir::{self, block_manager::branch},
    symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult, PathedCtxTrait,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx, linearize_context::FunctionLinearizeErased,
    },
    types,
};

impl Collect<symtab::ValuePath> for ast::expressions::WhileExpressionTree {
    fn collect_inner(&self, mut ctx: ValueCollectCtx) -> CollectResult {
        ctx = self.condition.collect_symbols(ctx)?;
        self.body.collect_symbols(ctx)?.into_result()
    }
}

impl FunctionLinearizeErased for ast::expressions::WhileExpressionTree {
    type Data = ir::ValueId;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_erased(
        self,
        mut ctx: FunctionLinearizeCtx<Box<dyn branch::Branch>>,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx<Box<dyn branch::Branch>>> {
        let span = self.span();

        let parent_scope_def_path = ctx.path().clone();
        let loop_scope_def_path = ctx.reserve_subscope();
        let mut ctx: FunctionLinearizeCtx<
            branch::NestedBranch<Box<dyn branch::Branch>, branch::LoopHeader>,
        > = ctx.create_loop(
            branch::BranchDesc {
                loc: span.clone().start(),
                after: span.end(),
            },
            self.body.span().start(),
            self.body.span().end(),
            self.span().end(),
            parent_scope_def_path,
            loop_scope_def_path,
        );

        let after_loop_label = ctx.after_loop_label();

        let condition_loc = self.condition.span();
        let condition;
        (condition, ctx) = self.condition.linearize(ctx)?;
        let loop_condition_jump = ir::IrLine::new_jump(
            condition_loc.clone().end(),
            after_loop_label,
            ir::lowered::operands::JumpCondition::Conditional(
                ir::lowered::operands::BinaryComparisonOperands::new(
                    condition,
                    ctx.values_mut().id_for_constant(
                        0,
                        #[cfg(feature = "value_locs")]
                        frontend::here!(),
                    ),
                    ir::lowered::operands::BinaryComparisonKind::EQ,
                ),
            ),
        );

        ctx.append_statement_to_current_block(loop_condition_jump);

        let ctx: FunctionLinearizeCtx<
            branch::NestedBranch<Box<dyn branch::Branch>, branch::LoopBody>,
        > = ctx.finish_loop_header(condition_loc.end());

        let body_end = self.body.span().end();
        let (_, ctx) = self.body.linearize(ctx)?;

        let ctx: FunctionLinearizeCtx<
            branch::NestedBranch<Box<dyn branch::Branch>, branch::LoopFooter>,
        > = ctx.finish_loop_body(body_end.clone());

        ctx.finish_loop_footer(body_end)
            .into_result(ir::ValueInterner::<Option<types::Syntactic>>::unit_value_id())
    }
}

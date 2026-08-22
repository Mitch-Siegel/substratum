use frontend::ast::{self, Ast};

use crate::{
    ir, symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult, PathedCtxTrait,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx,
    },
};

impl Collect<symtab::ValuePath> for ast::expressions::WhileExpressionTree {
    fn collect_inner(&self, mut ctx: ValueCollectCtx) -> CollectResult {
        ctx = self.condition.collect_symbols(ctx)?;
        self.body.collect_symbols(ctx)?.into_result()
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::WhileExpressionTree
{
    type Data = ir::ValueId;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let loc = self.loc();

        let parent_scope_def_path = ctx.path().clone();
        let loop_scope_def_path = ctx.reserve_subscope();
        let loop_done_label = ctx.create_loop(
            loc.clone().start(),
            parent_scope_def_path,
            loop_scope_def_path,
        );

        let condition_loc = self.condition.loc();
        let condition;
        (condition, ctx) = self.condition.linearize(ctx)?;
        let loop_condition_jump = ir::IrLine::new_jump(
            condition_loc.end(),
            loop_done_label,
            ir::lowered::operands::JumpCondition::Conditional(
                ir::lowered::operands::BinaryComparisonOperands::new(
                    condition,
                    *ctx.values_mut().id_for_constant(0),
                    ir::lowered::operands::BinaryComparisonKind::EQ,
                ),
            ),
        );

        ctx.append_jump_to_current_block(loop_condition_jump);

        let parent_def_path = ctx.path().clone();
        ctx.unconditional_branch_from_current(
            loc.clone().end(),
            parent_def_path.clone(),
            parent_def_path,
        );
        let (_, mut ctx) = self.body.linearize(ctx)?;

        ctx.finish_branch(loc.clone().end());

        ctx.finish_loop(loc.end(), Vec::new());

        ctx.into_result(ir::ValueInterner::unit_value_id())
    }
}

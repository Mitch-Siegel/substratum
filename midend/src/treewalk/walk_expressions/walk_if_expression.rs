use frontend::ast::{self, Ast};

use crate::{
    ir, symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult, PathedCtxTrait,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx,
    },
};

impl Collect<symtab::ValuePath> for ast::expressions::IfExpressionTree {
    fn collect_inner(&self, mut ctx: ValueCollectCtx) -> CollectResult {
        ctx = self.true_block.collect_symbols(ctx)?;
        if let Some(else_block) = &self.false_block {
            else_block.collect_symbols(ctx)?.into_result()
        } else {
            ctx.into_result()
        }
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::IfExpressionTree
{
    type Data = ir::ValueId;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.loc();
        let if_loc = self.loc();
        let condition_value;
        (condition_value, ctx) = self.condition.linearize(ctx)?;

        let if_condition = ir::lowered::operands::JumpCondition::Conditional(
            ir::lowered::operands::BinaryComparisonOperands::new(
                condition_value,
                *ctx.values_mut().id_for_constant(0),
                ir::lowered::operands::BinaryComparisonKind::NE,
            ),
        );

        let parent_scope_def_path = ctx.path().clone();
        let true_scope_def_path = ctx.reserve_subscope();
        let false_scope_def_path = ctx.reserve_subscope();

        ctx.conditional_branch_from_current(
            condition_loc.clone().start(),
            if_condition,
            parent_scope_def_path,
            true_scope_def_path,
            false_scope_def_path,
        );

        let true_loc = self.true_block.loc();
        let if_value_id;
        (if_value_id, ctx) = self.true_block.linearize(ctx)?;

        // create a separate, mutable value which contains the true result
        let result_value_id = if_value_id;

        // if a false block exists AND the 'if' value exists
        if self.false_block.is_some() {
            // we need to copy the 'if' result to the common result_value at the end of the 'if' block
            let result_value = ctx.values_mut().next_temp();
            let assign_if_result_line =
                ir::IrLine::new_assignment(true_loc.start(), result_value, if_value_id);
            ctx.append_statement_to_current_block(assign_if_result_line);
        }

        ctx.finish_true_branch_switch_to_false(condition_loc.start());

        // handle branch linearization and assignment to the result value
        if let Some(else_block) = self.false_block {
            let else_loc = else_block.loc();
            let else_value_id;
            (else_value_id, ctx) = else_block.linearize(ctx)?;

            // if the 'else' value exists (have already passed check to assert types are the same)
            // copy the 'else' result to the common result_value at the end of the 'else' block
            let assign_else_result_line =
                ir::IrLine::new_assignment(else_loc.end(), result_value_id, else_value_id);
            ctx.append_statement_to_current_block(assign_else_result_line);
        }

        ctx.finish_branch(if_loc.end());

        ctx.into_result(result_value_id)
    }
}

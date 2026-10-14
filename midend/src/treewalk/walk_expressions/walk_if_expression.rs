use frontend::ast::{self, Ast};

use crate::{
    ir::{self, block_manager::branch},
    symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult, PathedCtxTrait,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx, linearize_context::FunctionLinearizeErased,
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

impl FunctionLinearizeErased for ast::expressions::IfExpressionTree {
    type Data = ir::ValueId;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_erased(
        self,
        mut ctx: FunctionLinearizeCtx<Box<dyn branch::Branch>>,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx<Box<dyn branch::Branch>>> {
        // FUTURE: optimize condition walk to use different jumps
        let condition_loc = self.condition.span();
        let if_loc = self.span();
        let after_loc = self.span().into_end();

        let condition_value;
        (condition_value, ctx) = self.condition.linearize(ctx)?;

        let if_condition = ir::lowered::operands::JumpCondition::Conditional(
            ir::lowered::operands::BinaryComparisonOperands::new(
                condition_value,
                ctx.values_mut().id_for_constant(
                    0,
                    #[cfg(feature = "value_locs")]
                    frontend::here!(),
                ),
                ir::lowered::operands::BinaryComparisonKind::NE,
            ),
        );

        let parent_scope_def_path = ctx.path().clone();

        let true_data = branch::BranchTarget::new(if_loc.start().clone(), ctx.reserve_subscope());

        let branch_loc = condition_loc.start().clone();

        // only generate false data if a false block actually exists
        if let Some(f) = self.false_block.as_ref() {
            let false_data =
                branch::BranchTarget::new(f.span().into_start(), ctx.reserve_subscope());

            // TODO: create double/single branch based on presence of else
            let mut ctx = ctx.create_double_conditional_branch(
                branch_loc,
                after_loc, // TODO: this will artificially capture the close brace as included in the range of "after". How to cleanly mitigate?
                true_data,
                false_data,
                if_condition,
                parent_scope_def_path,
            );

            let true_loc = self.true_block.span();
            let if_value_id;
            (if_value_id, ctx) = self.true_block.linearize(ctx)?;

            // create a separate, mutable value which contains the true result
            let mut result_value = if_value_id;

            // handle branch linearization and assignment to the result value
            let ctx = if let Some(else_block) = self.false_block {
                // we need to copy the 'if' result to the common result_value at the end of the 'if' block
                result_value = ctx.values_mut().next_temp(
                    #[cfg(feature = "value_locs")]
                    frontend::here!(),
                );
                let assign_if_result_line =
                    ir::IrLine::new_assignment(true_loc.into_start(), result_value, if_value_id);
                ctx.append_statement_to_current_block(assign_if_result_line);
                // TODO: types here
                let mut ctx = ctx.finish_true_branch_switch_to_false(condition_loc.into_start());
                let else_loc = else_block.span();
                let else_value_id;
                (else_value_id, ctx) = else_block.linearize(ctx)?;

                // if the 'else' value exists (have already passed check to assert types are the same)
                // copy the 'else' result to the common result_value at the end of the 'else' block
                let assign_else_result_line =
                    ir::IrLine::new_assignment(else_loc.into_end(), result_value, else_value_id);
                ctx.append_statement_to_current_block(assign_else_result_line);
                ctx
            } else {
                ctx.finish_true_branch_switch_to_false(true_loc.into_end())
            };

            let ctx = ctx.finish_false_branch(if_loc.into_end());

            ctx.into_result(result_value)
        } else {
            let ctx: FunctionLinearizeCtx<
                branch::NestedBranch<Box<dyn branch::Branch>, branch::SingleBranchTrue>,
            > = ctx.create_single_conditional_branch(
                branch_loc,
                after_loc,
                true_data,
                if_condition,
                parent_scope_def_path,
            );

            let _ctx = ctx.finish_true_branch(if_loc.into_end());
            unimplemented!();

            // ctx.into_result(ir::ValueInterner::unit_value_id())
        }
    }
}

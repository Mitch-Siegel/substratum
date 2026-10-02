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
        let after_loc = self.loc().end();

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

        let true_data =
            ir::block_manager::BranchTarget::new(if_loc.start(), ctx.reserve_subscope());

        // only generate false data if a false block actually exists
        let false_data = if let Some(f) = self.false_block.as_ref() {
            ir::block_manager::BranchTarget::new(f.loc().start(), ctx.reserve_subscope())
        } else {
            // TODO: remove me when we can pass options to conditional branch functions for branches with no false
            ir::block_manager::BranchTarget::new(
                frontend::sourceloc::SourceLoc::new(
                    String::new(),
                    frontend::sourceloc::SourcePoint::new(0, 0),
                ),
                ctx.reserve_subscope(),
            )
        };

        ctx.conditional_branch_from_current(
            condition_loc.start(),
            true_data,
            false_data,
            after_loc, // TODO: this will artificially capture the close brace as included in the range of "after". How to cleanly mitigate?
            if_condition,
            parent_scope_def_path,
        );

        let true_loc = self.true_block.loc();
        let if_value_id;
        (if_value_id, ctx) = self.true_block.linearize(ctx)?;

        // create a separate, mutable value which contains the true result
        let mut result_value = if_value_id;

        // handle branch linearization and assignment to the result value
        if let Some(else_block) = self.false_block {
            // we need to copy the 'if' result to the common result_value at the end of the 'if' block
            result_value = ctx.values_mut().next_temp(
                #[cfg(feature = "value_locs")]
                frontend::here!(),
            );
            let assign_if_result_line =
                ir::IrLine::new_assignment(true_loc.start(), result_value, if_value_id);
            ctx.append_statement_to_current_block(assign_if_result_line);
            ctx.finish_true_branch_switch_to_false(condition_loc.start());
            let else_loc = else_block.loc();
            let else_value_id;
            (else_value_id, ctx) = else_block.linearize(ctx)?;

            // if the 'else' value exists (have already passed check to assert types are the same)
            // copy the 'else' result to the common result_value at the end of the 'else' block
            let assign_else_result_line =
                ir::IrLine::new_assignment(else_loc.end(), result_value, else_value_id);
            ctx.append_statement_to_current_block(assign_else_result_line);
        }

        ctx.finish_branch(if_loc.end());

        ctx.into_result(result_value)
    }
}

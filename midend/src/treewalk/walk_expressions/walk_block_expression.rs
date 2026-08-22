use frontend::ast;

use crate::{
    symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult, PathedCtxTrait,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx, ir,
    },
};

impl Collect<symtab::ValuePath> for ast::expressions::BlockExpressionTree {
    fn collect_inner(&self, mut ctx: ValueCollectCtx) -> CollectResult {
        for stmt in &self.statements {
            ctx = stmt.collect_symbols(ctx)?;
        }

        ctx.into_result()
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::BlockExpressionTree
{
    type Data = ir::ValueId;
    fn linearize_inner(
        mut self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let parent_def_path = ctx.path().clone();
        let true_scope_def_path = ctx.reserve_subscope();
        ctx.unconditional_branch_from_current(
            self.open_brace_loc.start(),
            parent_def_path,
            true_scope_def_path,
        );

        let last_statement = self.statements.pop();
        for statement in self.statements {
            (_, ctx) = statement.linearize(ctx)?;
        }

        let last_statement_value = match last_statement {
            Some(statement_tree) => {
                let maybe_value;
                (maybe_value, ctx) = statement_tree.linearize(ctx)?;
                maybe_value.unwrap_or(ir::ValueInterner::unit_value_id())
            }
            None => ir::ValueInterner::unit_value_id(),
        };

        ctx.finish_branch(self.close_brace_loc.end());

        ctx.into_result(last_statement_value)
    }
}

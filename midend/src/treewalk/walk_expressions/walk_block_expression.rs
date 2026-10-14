use frontend::ast;

use crate::{
    ir::{self, block_manager::branch},
    symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult, PathedCtxTrait,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx,
    },
    types,
};

impl Collect<symtab::ValuePath> for ast::expressions::BlockExpressionTree {
    fn collect_inner(&self, mut ctx: ValueCollectCtx) -> CollectResult {
        for stmt in &self.statements {
            ctx = stmt.collect_symbols(ctx)?;
        }

        ctx.into_result()
    }
}

impl<B: branch::Branch>
    Linearize<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath, FunctionLinearizeCtx<B>>
    for ast::expressions::BlockExpressionTree
{
    type Data = ir::ValueId;
    fn linearize_inner(
        mut self,
        mut ctx: FunctionLinearizeCtx<B>,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx<B>> {
        let parent_def_path = ctx.path().clone();
        let true_scope_def_path = ctx.reserve_subscope();
        let mut ctx: FunctionLinearizeCtx<branch::NestedBranch<B, branch::Unconditional>> = ctx
            .create_unconditional_branch(
                self.open_brace.into_start(),
                // TODO: this will artifically capture the close brace as included in the range of "after". How to cleanly mitigate?
                self.close_brace.end().clone(),
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
                maybe_value
                    .unwrap_or(ir::ValueInterner::<Option<types::Syntactic>>::unit_value_id())
            }
            None => ir::ValueInterner::<Option<types::Syntactic>>::unit_value_id(),
        };

        let ctx = ctx.finish_unconditional_branch(self.close_brace.into_end());

        ctx.into_result(last_statement_value)
    }
}

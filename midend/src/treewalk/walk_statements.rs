use frontend::ast::{self, Ast};

use crate::{
    ir, symtab,
    treewalk::{
        Collect, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx,
    },
};

mod walk_let_statement;

impl Collect<symtab::ValuePath> for ast::StatementTree {
    fn collect_inner(&self, ctx: ValueCollectCtx) -> CollectResult {
        match self {
            Self::Let(let_stmt) => let_stmt.collect_symbols(ctx),
            // FUTURE: support items in statements
            Self::Item(_) => unimplemented!("items in statements not yet supported"),
            // StatementTree::Item(item) => item.collect_symbols(ctx),
            Self::Expression(expr) => expr.collect_symbols(ctx),
        }?
        .into_result()
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::StatementTree
{
    type Data = Option<ir::ValueId>;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let (maybe_value, ctx) = match self {
            Self::Item(_) => unimplemented!(),
            Self::Let(let_tree) => {
                let ((), ctx) = let_tree.linearize(ctx)?;
                (None, ctx)
            }
            Self::Expression(expression_tree) => {
                let (value, ctx) = expression_tree.linearize(ctx)?;
                (Some(value), ctx)
            }
        };

        ctx.into_result(maybe_value)
    }
}

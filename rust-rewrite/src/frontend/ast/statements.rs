use crate::{
    frontend::ast::*,
    midend::{
        symtab,
        treewalk::{self, PathedLinearizeCtxTrait, ValueFunctionLinearizeCtx},
    },
};

pub mod let_statement;

pub use let_statement::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum StatementTree {
    Item(Box<ItemTree>),
    Let(Box<LetTree>),
    Expression(Expression),
}

impl Ast for StatementTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Item(item) => item.loc(),
            Self::Let(let_tree) => let_tree.loc(),
            Self::Expression(expr_tree) => expr_tree.loc(),
        }
    }
}

impl midend::treewalk::Collect<midend::symtab::ValuePath> for StatementTree {
    fn collect_inner(
        &self,
        ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        match self {
            StatementTree::Let(let_stmt) => let_stmt.collect_symbols(ctx),
            // FUTURE: support items in statements
            StatementTree::Item(_) => unimplemented!("items in statements not yet supported"),
            // StatementTree::Item(item) => item.collect_symbols(ctx),
            StatementTree::Expression(expr) => expr.collect_symbols(ctx),
        }?
        .into_result()
    }
}

impl
    treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        symtab::ValuePath,
        treewalk::ValueFunctionLinearizeCtx,
    > for StatementTree
{
    type Data = Option<midend::ir::ValueId>;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        ctx: treewalk::ValueFunctionLinearizeCtx,
    ) -> treewalk::LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let (maybe_value, ctx) = match self {
            Self::Item(_) => unimplemented!(),
            Self::Let(let_tree) => {
                let (_, ctx) = let_tree.linearize(ctx)?;
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

impl Display for StatementTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Item(item) => write!(f, "{}", item),
            Self::Let(let_) => write!(f, "{}", let_),
            Self::Expression(expression) => write!(f, "{}", expression),
        }
    }
}

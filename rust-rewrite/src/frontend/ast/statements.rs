use crate::frontend::ast::*;

pub mod let_statement;

pub use let_statement::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum StatementTree {
    Item(ItemTree),
    Let(LetTree),
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

impl midend::treewalk::Collect for StatementTree {
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        match self {
            StatementTree::Let(let_stmt) => let_stmt.collect_symbols(ctx),
            StatementTree::Item(item) => item.collect_symbols(ctx),
            StatementTree::Expression(expr) => expr.collect_symbols(ctx),
        }
    }
}

impl midend::treewalk::Linearize<Option<midend::ir::ValueId>> for StatementTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> Option<midend::ir::ValueId> {
        match self {
            Self::Item(_) => unimplemented!(),
            Self::Let(let_tree) => {
                let_tree.linearize(ctx);
                None
            }
            Self::Expression(expression_tree) => Some(expression_tree.linearize(ctx)),
        }
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

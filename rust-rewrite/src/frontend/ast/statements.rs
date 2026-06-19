use crate::frontend::ast::*;

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
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        match self {
            StatementTree::Let(let_stmt) => let_stmt.collect_symbols(ctx),
            // FUTURE: support items in statements
            StatementTree::Item(_) => unimplemented!("items in statements not yet supported"),
            // StatementTree::Item(item) => item.collect_symbols(ctx),
            StatementTree::Expression(expr) => expr.collect_symbols(ctx),
        }
    }
}

impl midend::treewalk::Linearize<midend::symtab::ValuePath> for StatementTree {
    type Data = Option<midend::ir::ValueId>;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        ctx: midend::treewalk::ValueLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
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

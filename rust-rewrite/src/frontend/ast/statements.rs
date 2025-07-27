use crate::frontend::ast::*;

pub mod let_statement;

pub use let_statement::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Statement {
    Item(ItemTree),
    Let(LetTree),
    Expression(ExpressionTree),
}
impl Display for Statement {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Item(item) => write!(f, "{}", item),
            Self::Let(let_) => write!(f, "{}", let_),
            Self::Expression(expression) => write!(f, "{}", expression),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StatementTree {
    pub loc: SourceLocWithMod,
    pub statement: Statement,
}

impl StatementTree {
    pub fn new(loc: SourceLocWithMod, statement: Statement) -> Self {
        Self { loc, statement }
    }
}

impl Display for StatementTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.statement)
    }
}

impl ValueWalk for StatementTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut midend::linearizer::FunctionWalkContext) -> midend::ir::ValueId {
        match self.statement {
            Statement::Item(_) => unimplemented!(),
            Statement::Let(let_tree) => let_tree.walk(context),
            Statement::Expression(expression_tree) => expression_tree.walk(context),
        }
    }
}

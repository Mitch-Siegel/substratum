use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression, ItemTree},
    loc,
};

pub mod let_statement;

pub use let_statement::LetTree;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum StatementTree {
    Item(Box<ItemTree>),
    Let(Box<LetTree>),
    Expression(Expression),
}

impl Ast for StatementTree {
    fn span(&self) -> loc::Span {
        match self {
            Self::Item(item) => item.span(),
            Self::Let(let_tree) => let_tree.span(),
            Self::Expression(expr_tree) => expr_tree.span(),
        }
    }
}

impl fmt::Display for StatementTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Item(item) => write!(f, "{item}"),
            Self::Let(let_) => write!(f, "{let_}"),
            Self::Expression(expression) => write!(f, "{expression}"),
        }
    }
}

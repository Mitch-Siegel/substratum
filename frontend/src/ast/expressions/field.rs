use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression, IdentifierTree},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FieldExpressionTree {
    pub receiver: Expression,
    pub field: IdentifierTree,
}

impl Ast for FieldExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.receiver.loc().merge(&self.field.loc()).unwrap()
    }
}

impl fmt::Display for FieldExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.receiver, self.field)
    }
}

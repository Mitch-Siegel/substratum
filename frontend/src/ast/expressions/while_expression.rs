use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression, expressions},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct WhileExpressionTree {
    pub(crate) while_keyword: loc::Span,
    pub condition: Expression,
    pub body: expressions::BlockExpressionTree,
}

impl Ast for WhileExpressionTree {
    fn span(&self) -> loc::Span {
        self.while_keyword.clone().merge(&self.body.span()).unwrap()
    }
}

impl fmt::Display for WhileExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "while ({}) {}", self.condition, self.body)
    }
}

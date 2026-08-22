use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression, expressions},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct WhileExpressionTree {
    pub(crate) while_keyword_loc: sourceloc::SourceSpan,
    pub condition: Expression,
    pub body: expressions::BlockExpressionTree,
}

impl Ast for WhileExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.while_keyword_loc
            .clone()
            .merge(&self.body.loc())
            .unwrap()
    }
}

impl fmt::Display for WhileExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "while ({}) {}", self.condition, self.body)
    }
}

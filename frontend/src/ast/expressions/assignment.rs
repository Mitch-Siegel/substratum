use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct AssignmentTree {
    pub assignee: Box<Expression>,
    pub value: Box<Expression>,
}

impl Ast for AssignmentTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.assignee.loc().merge(&self.value.loc()).unwrap()
    }
}

impl fmt::Display for AssignmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.assignee, self.value)
    }
}

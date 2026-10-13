use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct AssignmentTree {
    pub assignee: Box<Expression>,
    pub value: Box<Expression>,
}

impl Ast for AssignmentTree {
    fn span(&self) -> loc::Span {
        self.assignee.span().merge(&self.value.span()).unwrap()
    }
}

impl fmt::Display for AssignmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.assignee, self.value)
    }
}

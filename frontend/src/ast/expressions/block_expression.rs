use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, StatementTree},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct BlockExpressionTree {
    pub open_brace: loc::Span,
    pub statements: Vec<StatementTree>,
    pub close_brace: loc::Span,
}

impl Ast for BlockExpressionTree {
    fn span(&self) -> loc::Span {
        self.open_brace
            .clone()
            .merge(&self.close_brace.clone())
            .unwrap()
    }
}

impl fmt::Display for BlockExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut statement_string = String::new();
        for statement in &self.statements {
            statement_string.push_str(format!("{statement}\n").as_str());
        }
        write!(f, "Block Expression: {statement_string}")
    }
}

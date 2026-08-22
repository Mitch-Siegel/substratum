use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, StatementTree},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct BlockExpressionTree {
    pub open_brace_loc: sourceloc::SourceSpan,
    pub statements: Vec<StatementTree>,
    pub close_brace_loc: sourceloc::SourceSpan,
}

impl Ast for BlockExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_brace_loc
            .clone()
            .merge(&self.close_brace_loc.clone())
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

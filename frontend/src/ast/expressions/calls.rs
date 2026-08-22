use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct CallParamsTree {
    pub(crate) open_paren_loc: sourceloc::SourceSpan,
    pub params: Vec<Expression>,
    pub(crate) close_paren_loc: sourceloc::SourceSpan,
}

impl Ast for CallParamsTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_paren_loc
            .clone()
            .merge(&self.close_paren_loc)
            .unwrap()
    }
}

impl fmt::Display for CallParamsTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            self.params
                .iter()
                .map(|param| format!("{param}"))
                .collect::<Vec<String>>()
                .join(", ")
        )
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct CallExpressionTree {
    pub function_operand: Expression,
    pub params: CallParamsTree,
}

impl Ast for CallExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.function_operand
            .loc()
            .merge(&self.params.loc())
            .unwrap()
    }
}

impl fmt::Display for CallExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}({})", self.function_operand, self.params)
    }
}

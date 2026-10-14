use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct CallParamsTree {
    pub(crate) open_paren: loc::Span,
    pub params: Vec<Expression>,
    pub(crate) close_paren: loc::Span,
}

impl Ast for CallParamsTree {
    fn span(&self) -> loc::Span {
        self.open_paren.clone().merge(&self.close_paren).unwrap()
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
    fn span(&self) -> loc::Span {
        self.function_operand
            .span()
            .merge(&self.params.span())
            .unwrap()
    }
}

impl fmt::Display for CallExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}({})", self.function_operand, self.params)
    }
}

use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ArithmeticDualOperands {
    pub e1: Box<Expression>,
    pub e2: Box<Expression>,
}

impl ArithmeticDualOperands {
    pub(crate) fn new(e1: Expression, e2: Expression) -> Self {
        Self {
            e1: Box::new(e1),
            e2: Box::new(e2),
        }
    }
}

impl Ast for ArithmeticDualOperands {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.e1.loc().merge(&self.e2.loc()).unwrap()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ComparisonExpressionTree {
    LThan(ArithmeticDualOperands),
    GThan(ArithmeticDualOperands),
    LThanE(ArithmeticDualOperands),
    GThanE(ArithmeticDualOperands),
    Equals(ArithmeticDualOperands),
    NotEquals(ArithmeticDualOperands),
}

impl Ast for ComparisonExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::LThan(operands)
            | Self::GThan(operands)
            | Self::LThanE(operands)
            | Self::GThanE(operands)
            | Self::Equals(operands)
            | Self::NotEquals(operands) => operands.loc(),
        }
    }
}

impl fmt::Display for ComparisonExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::LThan(operands) => write!(f, "({} < {})", operands.e1, operands.e2),
            Self::GThan(operands) => write!(f, "({} > {})", operands.e1, operands.e2),
            Self::LThanE(operands) => write!(f, "({} <= {})", operands.e1, operands.e2),
            Self::GThanE(operands) => write!(f, "({} >= {})", operands.e1, operands.e2),
            Self::Equals(operands) => write!(f, "({} == {})", operands.e1, operands.e2),
            Self::NotEquals(operands) => write!(f, "({} != {})", operands.e1, operands.e2),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ArithmeticExpressionTree {
    Add(ArithmeticDualOperands),
    Subtract(ArithmeticDualOperands),
    Multiply(ArithmeticDualOperands),
    Divide(ArithmeticDualOperands),
}

impl Ast for ArithmeticExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Add(o) | Self::Subtract(o) | Self::Multiply(o) | Self::Divide(o) => o.loc(),
        }
    }
}

impl fmt::Display for ArithmeticExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Add(operands) => write!(f, "({} + {})", operands.e1, operands.e2),
            Self::Subtract(operands) => write!(f, "({} - {})", operands.e1, operands.e2),
            Self::Multiply(operands) => write!(f, "({} * {})", operands.e1, operands.e2),
            Self::Divide(operands) => write!(f, "({} / {})", operands.e1, operands.e2),
        }
    }
}

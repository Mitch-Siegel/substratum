use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{ast::Ast, loc};

pub mod arithmetic;
pub mod assignment;
pub mod block_expression;
pub mod calls;
pub mod field;
pub mod if_expression;
pub mod match_expression;
pub mod path_in_expression;
pub mod while_expression;

pub use arithmetic::{ArithmeticExpressionTree, ComparisonExpressionTree};
pub use assignment::AssignmentTree;
pub use block_expression::BlockExpressionTree;
pub use calls::CallExpressionTree;
pub use field::FieldExpressionTree;
pub use if_expression::IfExpressionTree;
pub use match_expression::MatchExpressionTree;
pub use path_in_expression::*;
pub use while_expression::WhileExpressionTree;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum Expression {
    PathIn(PathInExpressionTree),
    UnsignedDecimalConstant(loc::Span, usize),
    Arithmetic(ArithmeticExpressionTree),
    Comparison(ComparisonExpressionTree),
    Assignment(AssignmentTree),
    If(Box<IfExpressionTree>),
    Match(Box<MatchExpressionTree>),
    While(Box<WhileExpressionTree>),
    Field(Box<FieldExpressionTree>),
    Call(Box<CallExpressionTree>),
}

impl Ast for Expression {
    fn span(&self) -> loc::Span {
        match self {
            Self::PathIn(e) => e.span(),
            Self::UnsignedDecimalConstant(l, _) => l.clone(),
            Self::Arithmetic(e) => e.span(),
            Self::Comparison(e) => e.span(),
            Self::Assignment(e) => e.span(),
            Self::If(e) => e.span(),
            Self::Match(e) => e.span(),
            Self::While(e) => e.span(),
            Self::Field(e) => e.span(),
            Self::Call(e) => e.span(),
        }
    }
}

impl fmt::Display for Expression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::PathIn(path) => write!(f, "{path}"),
            Self::UnsignedDecimalConstant(_, constant) => write!(f, "{constant}"),
            Self::Arithmetic(arithmetic_expression) => write!(f, "{arithmetic_expression}"),
            Self::Comparison(comparison_expression) => write!(f, "{comparison_expression}"),
            Self::Assignment(assignment_expression) => write!(f, "{assignment_expression}"),
            Self::If(if_expression) => write!(f, "{if_expression}"),
            Self::Match(match_expression) => write!(f, "{match_expression}"),
            Self::While(while_expression) => write!(f, "{while_expression}"),
            Self::Field(field_expression) => write!(f, "{field_expression}"),
            Self::Call(function_call) => write!(f, "{function_call}"),
        }
    }
}

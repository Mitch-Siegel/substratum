use crate::frontend::ast::{
    expressions::{arithmetic::ArithmeticDualOperands, ArithmeticExpressionTree},
    Expression, ExpressionTree,
};
use crate::frontend::sourceloc::SourceLoc;

pub fn test_loc(line: usize, col: usize) -> SourceLoc {
    SourceLoc::new(std::path::Path::new("test_module"), line, col)
}

pub fn expr(line: usize, col: usize, e: Expression) -> ExpressionTree {
    ExpressionTree::new(test_loc(line, col), e)
}

pub fn id(line: usize, col: usize, name: &str) -> ExpressionTree {
    expr(line, col, Expression::Identifier(name.into()))
}

pub fn unsigned_decimal_constant(line: usize, col: usize, value: usize) -> ExpressionTree {
    expr(line, col, Expression::UnsignedDecimalConstant(value))
}

pub fn add(line: usize, col: usize, lhs: ExpressionTree, rhs: ExpressionTree) -> ExpressionTree {
    expr(
        line,
        col,
        Expression::Arithmetic(ArithmeticExpressionTree::Add(ArithmeticDualOperands::new(
            lhs, rhs,
        ))),
    )
}

pub fn sub(line: usize, col: usize, lhs: ExpressionTree, rhs: ExpressionTree) -> ExpressionTree {
    expr(
        line,
        col,
        Expression::Arithmetic(ArithmeticExpressionTree::Subtract(
            ArithmeticDualOperands::new(lhs, rhs),
        )),
    )
}

pub fn mul(line: usize, col: usize, lhs: ExpressionTree, rhs: ExpressionTree) -> ExpressionTree {
    expr(
        line,
        col,
        Expression::Arithmetic(ArithmeticExpressionTree::Multiply(
            ArithmeticDualOperands::new(lhs, rhs),
        )),
    )
}

pub fn div(line: usize, col: usize, lhs: ExpressionTree, rhs: ExpressionTree) -> ExpressionTree {
    expr(
        line,
        col,
        Expression::Arithmetic(ArithmeticExpressionTree::Divide(
            ArithmeticDualOperands::new(lhs, rhs),
        )),
    )
}

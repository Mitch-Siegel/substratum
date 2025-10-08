use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ArithmeticDualOperands {
    pub e1: Box<ExpressionTree>,
    pub e2: Box<ExpressionTree>,
}

impl ArithmeticDualOperands {
    pub fn new(e1: ExpressionTree, e2: ExpressionTree) -> Self {
        Self {
            e1: Box::new(e1),
            e2: Box::new(e2),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum ComparisonExpressionTree {
    LThan(ArithmeticDualOperands),
    GThan(ArithmeticDualOperands),
    LThanE(ArithmeticDualOperands),
    GThanE(ArithmeticDualOperands),
    Equals(ArithmeticDualOperands),
    NotEquals(ArithmeticDualOperands),
}
impl Display for ComparisonExpressionTree {
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

impl treewalk::Linearize<midend::ir::lowered::operands::BinaryComparisonOperands>
    for ComparisonExpressionTree
{
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        ctx: &mut treewalk::LinearizeCtx,
    ) -> midend::ir::lowered::operands::BinaryComparisonOperands {
        match self {
            ComparisonExpressionTree::LThan(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryComparisonKind::LT,
                )
            }
            ComparisonExpressionTree::GThan(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryComparisonKind::GT,
                )
            }
            ComparisonExpressionTree::LThanE(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryComparisonKind::LE,
                )
            }
            ComparisonExpressionTree::GThanE(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryComparisonKind::GE,
                )
            }
            ComparisonExpressionTree::Equals(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryComparisonKind::EQ,
                )
            }
            ComparisonExpressionTree::NotEquals(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryComparisonKind::NE,
                )
            }
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum ArithmeticExpressionTree {
    Add(ArithmeticDualOperands),
    Subtract(ArithmeticDualOperands),
    Multiply(ArithmeticDualOperands),
    Divide(ArithmeticDualOperands),
}
impl Display for ArithmeticExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Add(operands) => write!(f, "({} + {})", operands.e1, operands.e2),
            Self::Subtract(operands) => write!(f, "({} - {})", operands.e1, operands.e2),
            Self::Multiply(operands) => write!(f, "({} * {})", operands.e1, operands.e2),
            Self::Divide(operands) => write!(f, "({} / {})", operands.e1, operands.e2),
        }
    }
}

impl treewalk::Linearize<midend::ir::lowered::operands::BinaryArithmeticOperands>
    for ArithmeticExpressionTree
{
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        ctx: &mut treewalk::LinearizeCtx,
    ) -> midend::ir::lowered::operands::BinaryArithmeticOperands {
        match self {
            ArithmeticExpressionTree::Add(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryArithmeticOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryArithmeticKind::Add,
                )
            }
            ArithmeticExpressionTree::Subtract(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryArithmeticOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryArithmeticKind::Sub,
                )
            }
            ArithmeticExpressionTree::Multiply(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryArithmeticOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryArithmeticKind::Mul,
                )
            }
            ArithmeticExpressionTree::Divide(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.linearize(ctx).into();
                let rhs: midend::ir::ValueId = operands.e2.linearize(ctx).into();
                midend::ir::lowered::operands::BinaryArithmeticOperands::new(
                    lhs,
                    rhs,
                    midend::ir::lowered::operands::BinaryArithmeticKind::Div,
                )
            }
        }
    }
}

use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
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

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
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

impl ValueWalk for ComparisonExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> midend::ir::ValueId {
        let (temp_dest, op) = match self {
            ComparisonExpressionTree::LThan(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_lt(dest, lhs, rhs))
            }
            ComparisonExpressionTree::GThan(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_gt(dest, lhs, rhs))
            }
            ComparisonExpressionTree::LThanE(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_le(dest, lhs, rhs))
            }
            ComparisonExpressionTree::GThanE(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_ge(dest, lhs, rhs))
            }
            ComparisonExpressionTree::Equals(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_eq(dest, lhs, rhs))
            }
            ComparisonExpressionTree::NotEquals(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_ne(dest, lhs, rhs))
            }
        };

        // TODO: association location with comparison expression tree
        let operation = midend::ir::IrLine::new(SourceLoc::none(), op);
        context
            .append_statement_to_current_block(operation)
            .unwrap();
        temp_dest
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
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

impl ValueWalk for ArithmeticExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut FunctionWalkContext) -> midend::ir::ValueId {
        let (temp_dest, op) = match self {
            ArithmeticExpressionTree::Add(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_add(dest, lhs, rhs))
            }
            ArithmeticExpressionTree::Subtract(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest: midend::ir::ValueId = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_sub(dest, lhs, rhs))
            }
            ArithmeticExpressionTree::Multiply(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_mul(dest, lhs, rhs))
            }
            ArithmeticExpressionTree::Divide(operands) => {
                let lhs: midend::ir::ValueId = operands.e1.walk(context).into();
                let rhs: midend::ir::ValueId = operands.e2.walk(context).into();
                let dest = context.next_temp();
                (dest.clone(), midend::ir::lowered::new_div(dest, lhs, rhs))
            }
        };

        // TODO: associate location with arithmetic expression trees
        let operation = midend::ir::IrLine::new_binary_op(SourceLoc::none(), op);
        context
            .append_statement_to_current_block(operation)
            .unwrap();
        temp_dest
    }
}

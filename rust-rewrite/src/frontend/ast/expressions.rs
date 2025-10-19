use crate::{frontend::ast::*, midend};

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

#[derive(ReflectName, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum Expression {
    SelfLower,
    PathInExpression(PathInExpressionTree),
    UnsignedDecimalConstant(usize),
    Arithmetic(ArithmeticExpressionTree),
    Comparison(ComparisonExpressionTree),
    Assignment(AssignmentTree),
    If(Box<IfExpressionTree>),
    Match(Box<MatchExpressionTree>),
    While(Box<WhileExpressionTree>),
    FieldExpression(Box<FieldExpressionTree>),
    Call(Box<CallExpressionTree>),
}

impl Display for Expression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::SelfLower => write!(f, "self"),
            Self::PathInExpression(path) => write!(f, "{}", path),
            Self::UnsignedDecimalConstant(constant) => write!(f, "{}", constant),
            Self::Arithmetic(arithmetic_expression) => write!(f, "{}", arithmetic_expression),
            Self::Comparison(comparison_expression) => write!(f, "{}", comparison_expression),
            Self::Assignment(assignment_expression) => write!(f, "{}", assignment_expression),
            Self::If(if_expression) => write!(f, "{}", if_expression),
            Self::Match(match_expression) => write!(f, "{}", match_expression),
            Self::While(while_expression) => write!(f, "{}", while_expression),
            Self::FieldExpression(field_expression) => write!(f, "{}", field_expression),
            Self::Call(function_call) => write!(f, "{}", function_call),
        }
    }
}

impl std::fmt::Debug for Expression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ExpressionTree {
    pub loc: SourceLoc,
    pub expression: Expression,
}

impl ExpressionTree {
    pub fn new(loc: SourceLoc, expression: Expression) -> Self {
        Self { loc, expression }
    }
}

impl Display for ExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.expression)
    }
}

impl treewalk::CollectSymbols for ExpressionTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        match &self.expression {
            Expression::If(if_expr) => {
                if_expr.condition.collect_symbols(ctx);
                if_expr.true_block.collect_symbols(ctx);
                if let Some(false_block) = &if_expr.false_block {
                    false_block.collect_symbols(ctx);
                }
            }
            Expression::While(while_expr) => {
                while_expr.condition.collect_symbols(ctx);
                while_expr.body.collect_symbols(ctx);
            }
            Expression::Match(match_expr) => {
                match_expr.scrutinee_expression.collect_symbols(ctx);
                for arm in &match_expr.arms {
                    arm.collect_symbols(ctx);
                }
            }
            Expression::SelfLower
            | Expression::PathInExpression(_)
            | Expression::UnsignedDecimalConstant(_)
            | Expression::Arithmetic(_)
            | Expression::Comparison(_)
            | Expression::Assignment(_)
            | Expression::FieldExpression(_)
            | Expression::Call(_) => (),
        }
    }
}

impl treewalk::Linearize<midend::ir::ValueId> for ExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
        match self.expression {
            Expression::SelfLower => {
                let self_variable_path = ctx.self_variable().unwrap();
                ctx.function_mut()
                    .values_mut()
                    .id_for_variable(self_variable_path)
            }
            Expression::PathInExpression(path) => path.linearize(ctx),
            Expression::UnsignedDecimalConstant(constant) => {
                *ctx.function_mut().values_mut().id_for_constant(constant)
            }
            Expression::Arithmetic(arithmetic_operation) => {
                let operands = arithmetic_operation.linearize(ctx);
                let destination = ctx.function_mut().values_mut().next_temp();
                let expression_statement = midend::ir::IrLine::new_binary_arithmetic_expression(
                    SourceLoc::none(), // FIXME: loc tracking for arithmetic expressions
                    destination,
                    operands,
                );
                ctx.function_mut()
                    .append_statement_to_current_block(expression_statement)
                    .unwrap();
                destination
            }
            Expression::Comparison(comparison_operation) => {
                let operands = comparison_operation.linearize(ctx);
                let destination = ctx.function_mut().values_mut().next_temp();
                let comparison_statement = midend::ir::IrLine::new_binary_comparison_expression(
                    SourceLoc::none(), // FIXME: loc tracking for arithmetic expressions
                    destination,
                    operands,
                );
                ctx.function_mut()
                    .append_statement_to_current_block(comparison_statement)
                    .unwrap();
                destination
            }
            Expression::Assignment(assignment_expression) => assignment_expression.linearize(ctx),
            Expression::If(if_expression) => if_expression.linearize(ctx),
            Expression::Match(match_expression) => match_expression.linearize(ctx),

            Expression::While(while_expression) => while_expression.linearize(ctx),
            Expression::FieldExpression(field_expression) => {
                let (receiver, field) = field_expression.linearize(ctx);
                let field_pointer_temp = ctx.function_mut().values_mut().next_temp();
                let field_read_line = midend::ir::IrLine::new_get_field_pointer(
                    self.loc,
                    receiver.into(),
                    field,
                    field_pointer_temp.clone(),
                );
                ctx.function_mut()
                    .append_statement_to_current_block(field_read_line)
                    .unwrap();
                field_pointer_temp
            }
            Expression::Call(call) => call.linearize(ctx),
        }
    }
}

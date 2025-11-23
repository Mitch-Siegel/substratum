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
    PathInExpression(PathInExpressionTree),
    UnsignedDecimalConstant(SourceLoc, usize),
    Arithmetic(ArithmeticExpressionTree),
    Comparison(ComparisonExpressionTree),
    Assignment(AssignmentTree),
    If(Box<IfExpressionTree>),
    Match(Box<MatchExpressionTree>),
    While(Box<WhileExpressionTree>),
    FieldExpression(Box<FieldExpressionTree>),
    Call(Box<CallExpressionTree>),
}

impl Expression {
    pub fn loc(&self) -> &SourceLoc {
        match self {
            Self::PathInExpression(e) => &e.loc,
            Self::UnsignedDecimalConstant(l, _) => l,
            Self::Arithmetic(e) => e.loc(),
            Self::Comparison(e) => e.loc(),
            Self::Assignment(e) => &e.loc,
            Self::If(e) => &e.loc,
            Self::Match(e) => &e.loc,
            Self::While(e) => &e.loc,
            Self::FieldExpression(e) => &e.loc,
            Self::Call(e) => &e.loc,
        }
    }
}

impl Display for Expression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::PathInExpression(path) => write!(f, "{}", path),
            Self::UnsignedDecimalConstant(_, constant) => write!(f, "{}", constant),
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

impl treewalk::CollectSymbols for Expression {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        match &self {
            Self::If(if_expr) => {
                if_expr.condition.collect_symbols(ctx);
                if_expr.true_block.collect_symbols(ctx);
                if let Some(false_block) = &if_expr.false_block {
                    false_block.collect_symbols(ctx);
                }
            }
            Self::While(while_expr) => {
                while_expr.condition.collect_symbols(ctx);
                while_expr.body.collect_symbols(ctx);
            }
            Self::Match(match_expr) => {
                match_expr.scrutinee_expression.collect_symbols(ctx);
                for arm in &match_expr.arms {
                    arm.collect_symbols(ctx);
                }
            }
            Self::PathInExpression(_)
            | Self::UnsignedDecimalConstant(_, _)
            | Self::Arithmetic(_)
            | Self::Comparison(_)
            | Self::Assignment(_)
            | Self::FieldExpression(_)
            | Self::Call(_) => (),
        }
    }
}

impl treewalk::Linearize<midend::ir::ValueId> for Expression {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
        match self {
            Self::PathInExpression(path) => path.linearize(ctx),
            Self::UnsignedDecimalConstant(_, constant) => {
                *ctx.function_mut().values_mut().id_for_constant(constant)
            }
            Self::Arithmetic(arithmetic_operation) => {
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
            Self::Comparison(comparison_operation) => {
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
            Self::Assignment(assignment_expression) => assignment_expression.linearize(ctx),
            Self::If(if_expression) => if_expression.linearize(ctx),
            Self::Match(match_expression) => match_expression.linearize(ctx),

            Self::While(while_expression) => while_expression.linearize(ctx),
            Self::FieldExpression(field_expression) => {
                let field_loc = field_expression.loc.clone();
                let (receiver, field) = field_expression.linearize(ctx);
                let field_pointer_temp = ctx.function_mut().values_mut().next_temp();
                let field_read_line = midend::ir::IrLine::new_get_field_pointer(
                    field_loc,
                    receiver.into(),
                    field,
                    field_pointer_temp.clone(),
                );
                ctx.function_mut()
                    .append_statement_to_current_block(field_read_line)
                    .unwrap();
                field_pointer_temp
            }
            Self::Call(call) => call.linearize(ctx),
        }
    }
}

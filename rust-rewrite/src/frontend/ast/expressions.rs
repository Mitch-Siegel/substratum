use crate::{
    frontend::ast::*, midend::{self, treewalk::{CollectCtx, TypeCollectCtx, ValueCollectCtx, ValueFunctionLinearizeCtx}},
};

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

#[derive(Debug, ReflectName, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum Expression {
    PathIn(PathInExpressionTree),
    UnsignedDecimalConstant(sourceloc::SourceSpan, usize),
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
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::PathIn(e) => e.loc(),
            Self::UnsignedDecimalConstant(l, _) => l.clone(),
            Self::Arithmetic(e) => e.loc(),
            Self::Comparison(e) => e.loc(),
            Self::Assignment(e) => e.loc(),
            Self::If(e) => e.loc(),
            Self::Match(e) => e.loc(),
            Self::While(e) => e.loc(),
            Self::Field(e) => e.loc(),
            Self::Call(e) => e.loc(),
        }
    }
}

impl midend::treewalk::Collect<midend::symtab::ValuePath> for Expression {
    fn collect_inner(
        &self,
        ctx: CollectCtx<midend::symtab::ValuePath>,
    ) -> midend::treewalk::CollectResult {
        match self {
            Self::If(if_expr) => if_expr.collect_symbols(ctx),
            Self::While(while_expr) => while_expr.collect_symbols(ctx),
            Self::Match(match_expr) => match_expr.collect_symbols(ctx),
            Self::PathIn(p) => p.collect_symbols(ctx),
            Self::Arithmetic(a) => a.collect_symbols(ctx),
            Self::Comparison(c) => c.collect_symbols(ctx),
            Self::Assignment(a) => a.collect_symbols(ctx),
            Self::Field(_) | Self::UnsignedDecimalConstant(_, _) | Self::Call(_) => Ok(ctx),
        }?.into_result()
    }
}

impl midend::treewalk::Linearize<ValueFunctionLinearizeCtx> for Expression
{
    type Data = midend::ir::ValueId;
    // #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: midend::treewalk::ValueFunctionLinearizeCtx,
    ) -> <midend::treewalk::ValueFunctionLinearizeCtx as midend::treewalk::PathedLinearizeCtxTrait>::Result<Self::Data>{
        let (value, ctx) = match self {
            Self::PathIn(path) => path.linearize(ctx)?,
            Self::UnsignedDecimalConstant(_, constant) => (
                ctx.function_mut()
                    .values_mut()
                    .id_for_constant(constant)
                    .to_owned(),
                ctx,
            ),
            Self::Arithmetic(arith) => {
                let loc = arith.loc();
                let (operands, mut ctx) = arith.linearize(ctx)?;
                let destination = ctx.function_mut().values_mut().next_temp();
                let expression_statement = midend::ir::IrLine::new_binary_arithmetic_expression(
                    loc.start(),
                    destination,
                    operands,
                );
                ctx.function_mut()
                    .append_statement_to_current_block(expression_statement)
                    .unwrap();
                (destination, ctx)
            }
            Self::Comparison(cmp) => {
                let loc = cmp.loc();
                let (operands, mut ctx) = cmp.linearize(ctx)?;
                let destination = ctx.function_mut().values_mut().next_temp();
                let comparison_statement = midend::ir::IrLine::new_binary_comparison_expression(
                    loc.start(),
                    destination,
                    operands,
                );
                ctx.function_mut()
                    .append_statement_to_current_block(comparison_statement)
                    .unwrap();
                (destination, ctx)
            }
            Self::Assignment(assignment_expression) => assignment_expression.linearize(ctx)?,
            Self::If(if_expression) => if_expression.linearize(ctx)?,
            Self::Match(match_expression) => match_expression.linearize(ctx)?,

            Self::While(while_expression) => while_expression.linearize(ctx)?,
            Self::Field(field_expression) => {
                let field_loc = field_expression.loc();
                let ((receiver, field), mut ctx) = field_expression.linearize(ctx)?;
                let field_pointer_temp = ctx.function_mut().values_mut().next_temp();
                let field_read_line = midend::ir::IrLine::new_get_field_pointer(
                    field_loc.start(),
                    receiver,
                    field,
                    field_pointer_temp,
                );
                ctx.function_mut()
                    .append_statement_to_current_block(field_read_line)
                    .unwrap();
                (field_pointer_temp, ctx)
            }
            Self::Call(call) => call.linearize(ctx)?,
        };

        ctx.into_result(value)
    }
}

impl Display for Expression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::PathIn(path) => write!(f, "{}", path),
            Self::UnsignedDecimalConstant(_, constant) => write!(f, "{}", constant),
            Self::Arithmetic(arithmetic_expression) => write!(f, "{}", arithmetic_expression),
            Self::Comparison(comparison_expression) => write!(f, "{}", comparison_expression),
            Self::Assignment(assignment_expression) => write!(f, "{}", assignment_expression),
            Self::If(if_expression) => write!(f, "{}", if_expression),
            Self::Match(match_expression) => write!(f, "{}", match_expression),
            Self::While(while_expression) => write!(f, "{}", while_expression),
            Self::Field(field_expression) => write!(f, "{}", field_expression),
            Self::Call(function_call) => write!(f, "{}", function_call),
        }
    }
}

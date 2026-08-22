use frontend::ast::{self, Ast};

use crate::{
    ir, symtab,
    treewalk::{
        Collect, CollectCtx, CollectResult, FunctionLinearizeCtx, Linearize, LinearizeResult,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx,
    },
};

mod walk_arithmetic;
mod walk_assignment;
mod walk_block_expression;
mod walk_calls;
mod walk_field;
mod walk_if_expression;
mod walk_match_expression;
mod walk_path_in_expression;
mod walk_while_expression;

impl Collect<symtab::ValuePath> for ast::Expression {
    fn collect_inner(&self, ctx: CollectCtx<symtab::ValuePath>) -> CollectResult {
        match self {
            Self::If(if_expr) => if_expr.collect_symbols(ctx),
            Self::While(while_expr) => while_expr.collect_symbols(ctx),
            Self::Match(match_expr) => match_expr.collect_symbols(ctx),
            Self::PathIn(p) => p.collect_symbols(ctx),
            Self::Arithmetic(a) => a.collect_symbols(ctx),
            Self::Comparison(c) => c.collect_symbols(ctx),
            Self::Assignment(a) => a.collect_symbols(ctx),
            Self::Field(_) | Self::UnsignedDecimalConstant(_, _) | Self::Call(_) => Ok(ctx),
        }?
        .into_result()
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::Expression
{
    type Data = ir::ValueId;
    // #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let (value, ctx) = match self {
            Self::PathIn(path) => path.linearize(ctx)?,
            Self::UnsignedDecimalConstant(_, constant) => {
                (ctx.values_mut().id_for_constant(constant).to_owned(), ctx)
            }
            Self::Arithmetic(arith) => {
                let loc = arith.loc();
                let (operands, mut ctx) = arith.linearize(ctx)?;
                let destination = ctx.values_mut().next_temp();
                let expression_statement = ir::IrLine::new_binary_arithmetic_expression(
                    loc.start(),
                    destination,
                    operands,
                );
                ctx.append_statement_to_current_block(expression_statement);
                (destination, ctx)
            }
            Self::Comparison(cmp) => {
                let loc = cmp.loc();
                let (operands, mut ctx) = cmp.linearize(ctx)?;
                let destination = ctx.values_mut().next_temp();
                let comparison_statement = ir::IrLine::new_binary_comparison_expression(
                    loc.start(),
                    destination,
                    operands,
                );
                ctx.append_statement_to_current_block(comparison_statement);
                (destination, ctx)
            }
            Self::Assignment(assignment_expression) => assignment_expression.linearize(ctx)?,
            Self::If(if_expression) => if_expression.linearize(ctx)?,
            Self::Match(match_expression) => match_expression.linearize(ctx)?,

            Self::While(while_expression) => while_expression.linearize(ctx)?,
            Self::Field(field_expression) => {
                let field_loc = field_expression.loc();
                let ((receiver, field), mut ctx) = field_expression.linearize(ctx)?;
                let field_pointer_temp = ctx.values_mut().next_temp();
                let field_read_line = ir::IrLine::new_get_field_pointer(
                    field_loc.start(),
                    receiver,
                    field,
                    field_pointer_temp,
                );
                ctx.append_statement_to_current_block(field_read_line);
                (field_pointer_temp, ctx)
            }
            Self::Call(call) => call.linearize(ctx)?,
        };

        ctx.into_result(value)
    }
}

impl Collect<symtab::ValuePath> for ast::expressions::ComparisonExpressionTree {
    fn collect_inner(&self, ctx: ValueCollectCtx) -> CollectResult {
        match self {
            Self::LThan(operands)
            | Self::GThan(operands)
            | Self::LThanE(operands)
            | Self::GThanE(operands)
            | Self::Equals(operands)
            | Self::NotEquals(operands) => operands.collect_inner(ctx),
        }
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::ComparisonExpressionTree
{
    type Data = ir::lowered::operands::BinaryComparisonOperands;
    #[trace::instrument(skip(self, ctx), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        match self {
            Self::LThan(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, ctx) = operands.e2.linearize(ctx)?;
                ctx.into_result(ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryComparisonKind::LT,
                ))
            }
            Self::GThan(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryComparisonKind::GT,
                ))
            }
            Self::LThanE(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryComparisonKind::LE,
                ))
            }
            Self::GThanE(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryComparisonKind::GE,
                ))
            }
            Self::Equals(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryComparisonKind::EQ,
                ))
            }
            Self::NotEquals(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryComparisonOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryComparisonKind::NE,
                ))
            }
        }
    }
}

impl Collect<symtab::ValuePath> for ast::expressions::ArithmeticExpressionTree {
    fn collect_inner(&self, ctx: ValueCollectCtx) -> CollectResult {
        match self {
            Self::Add(operands)
            | Self::Subtract(operands)
            | Self::Multiply(operands)
            | Self::Divide(operands) => operands.collect_inner(ctx),
        }
    }
}
impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::ArithmeticExpressionTree
{
    type Data = ir::lowered::operands::BinaryArithmeticOperands;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        match self {
            Self::Add(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryArithmeticOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryArithmeticKind::Add,
                ))
            }
            Self::Subtract(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryArithmeticOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryArithmeticKind::Sub,
                ))
            }
            Self::Multiply(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryArithmeticOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryArithmeticKind::Mul,
                ))
            }
            Self::Divide(operands) => {
                let (lhs, ctx) = operands.e1.linearize(ctx)?;
                let (rhs, unpathed) = operands.e2.linearize(ctx)?;
                unpathed.into_result(ir::lowered::operands::BinaryArithmeticOperands::new(
                    lhs,
                    rhs,
                    ir::lowered::operands::BinaryArithmeticKind::Div,
                ))
            }
        }
    }
}

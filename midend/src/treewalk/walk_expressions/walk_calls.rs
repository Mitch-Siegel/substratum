use frontend::ast::{self, Ast};

use crate::{
    symtab,
    treewalk::{
        FunctionLinearizeCtx, Linearize, LinearizeResult, UnpathedFunctionLinearizeCtx, ir,
    },
};

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::calls::CallParamsTree
{
    type Data = Vec<ir::ValueId>;
    fn linearize_inner(
        self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let mut param_values = Vec::new();

        for param in self.params {
            let param_id;
            (param_id, ctx) = param.linearize(ctx)?;
            param_values.push(param_id);
        }

        ctx.into_result(param_values)
    }
}

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::calls::CallExpressionTree
{
    type Data = ir::ValueId;
    fn linearize_inner(
        self,
        mut ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let call_start = self.loc().start();

        let function_operand;
        (function_operand, ctx) = self.function_operand.linearize(ctx)?;

        let return_value_to = ctx.values_mut().next_temp();

        // //TODO: error handling and checking
        // assert!(called_method.arguments.len() == params.len());

        let (params, mut ctx) = self.params.linearize(ctx)?;

        let method_call_line =
            ir::IrLine::new_call(call_start, function_operand, params, return_value_to);

        ctx.append_statement_to_current_block(method_call_line);

        ctx.into_result(return_value_to)
    }
}

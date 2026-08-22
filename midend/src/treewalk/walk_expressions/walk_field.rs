use frontend::ast;

use crate::{
    symtab,
    treewalk::{
        FunctionLinearizeCtx, Linearize, LinearizeResult, UnpathedFunctionLinearizeCtx, ir,
    },
};

impl Linearize<UnpathedFunctionLinearizeCtx, symtab::ScopePath, FunctionLinearizeCtx>
    for ast::expressions::FieldExpressionTree
{
    type Data = (ir::ValueId, String);
    // returns (receiver, field_info)
    // receiver is the value id for the receiver of the field access
    // field_info is a value id for the field being accessed
    fn linearize_inner(
        self,
        ctx: FunctionLinearizeCtx,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let (receiver, ctx) = self.receiver.linearize(ctx)?;

        let (field_name, ctx) = self.field.linearize(ctx)?;
        ctx.into_result((receiver, field_name))
    }
}

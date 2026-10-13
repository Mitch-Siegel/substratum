use frontend::ast;

use crate::{
    ir::block_manager::branch,
    symtab,
    treewalk::{
        FunctionLinearizeCtx, Linearize, LinearizeResult, UnpathedFunctionLinearizeCtx, ir,
    },
};

impl<B: branch::Branch>
    Linearize<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath, FunctionLinearizeCtx<B>>
    for ast::expressions::FieldExpressionTree
{
    type Data = (ir::ValueId, String);
    // returns (receiver, field_info)
    // receiver is the value id for the receiver of the field access
    // field_info is a value id for the field being accessed
    fn linearize_inner(
        self,
        ctx: FunctionLinearizeCtx<B>,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx<B>> {
        let (receiver, ctx) = self.receiver.linearize(ctx)?;

        let (field_name, ctx) = self.field.linearize(ctx)?;
        ctx.into_result((receiver, field_name))
    }
}

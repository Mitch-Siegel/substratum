use frontend::ast;

use crate::{
    ir, symtab,
    treewalk::{
        Collect, CollectResult, Linearize, LinearizeResult, PathedCtx,
        UnpathedFunctionLinearizeCtx, ValueCollectCtx, walk_path,
    },
};

impl Collect<symtab::ValuePath> for ast::expressions::PathInExpressionTree {
    fn collect_inner(&self, ctx: ValueCollectCtx) -> CollectResult {
        ctx.into_result()
    }
}

impl
    Linearize<
        UnpathedFunctionLinearizeCtx,
        symtab::ScopePath,
        PathedCtx<UnpathedFunctionLinearizeCtx, symtab::ScopePath>,
    > for ast::expressions::PathInExpressionTree
{
    type Data = ir::ValueId;
    fn linearize_inner(
        self,
        ctx: PathedCtx<UnpathedFunctionLinearizeCtx, symtab::ScopePath>,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let (finished_walk, mut ctx) = self.underlying_path.linearize(ctx)?;
        // TODO: record monomorphization
        // let path = linearized_path
        //     .map_data(|path, maybe_data| record_monomorphization(ctx, path, maybe_data))
        //     .unwrap();

        let walk_path::PathWithSegmentData { path, data: _data } =
            finished_walk.into_value().unwrap();

        let value = ctx.values_mut().id_for_path(&path);
        ctx.into_result(value)
    }
}

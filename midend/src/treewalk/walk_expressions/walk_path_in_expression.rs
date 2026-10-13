use frontend::ast;

use crate::{
    ir::{self, block_manager::branch},
    symtab,
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

impl<B: branch::Branch>
    Linearize<
        UnpathedFunctionLinearizeCtx<B>,
        symtab::ScopePath,
        PathedCtx<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath>,
    > for ast::expressions::PathInExpressionTree
{
    type Data = ir::ValueId;
    fn linearize_inner(
        self,
        ctx: PathedCtx<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath>,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx<B>> {
        let (finished_walk, mut ctx) = self.underlying_path.linearize(ctx)?;
        // TODO: record monomorphization
        // let path = linearized_path
        //     .map_data(|path, maybe_data| record_monomorphization(ctx, path, maybe_data))
        //     .unwrap();

        let walk_path::PathWithSegmentData { path, data: _data } =
            finished_walk.into_value().unwrap();

        let value = ctx.values_mut().id_for_path(
            &path,
            #[cfg(feature = "value_locs")]
            frontend::here!(),
        );
        ctx.into_result(value)
    }
}

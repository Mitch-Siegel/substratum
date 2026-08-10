use serde::{Deserialize, Serialize};

use crate::{
    frontend::{
        ast::{self, Ast},
        sourceloc,
    },
    midend::{
        self,
        symtab::ValuePath,
        treewalk::{self, UnpathedFunctionLinearizeCtx},
    },
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub(crate) struct PathInExpressionTree {
    pub underlying_path: ast::path::PathTree<ast::generics::GenericArgsListTree>,
}

impl Ast for PathInExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.underlying_path.loc()
    }
}

impl midend::treewalk::Collect<ValuePath> for PathInExpressionTree {
    fn collect_inner(
        &self,
        ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.into_result()
    }
}

impl
    treewalk::Linearize<
        UnpathedFunctionLinearizeCtx,
        midend::symtab::ScopePath,
        treewalk::PathedCtx<UnpathedFunctionLinearizeCtx, midend::symtab::ScopePath>,
    > for PathInExpressionTree
{
    type Data = midend::ir::ValueId;
    fn linearize_inner(
        self,
        ctx: treewalk::PathedCtx<UnpathedFunctionLinearizeCtx, midend::symtab::ScopePath>,
    ) -> midend::treewalk::LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let (finished_walk, mut ctx) = self.underlying_path.linearize(ctx)?;
        // TODO: record monomorphization
        // let path = linearized_path
        //     .map_data(|path, maybe_data| record_monomorphization(ctx, path, maybe_data))
        //     .unwrap();

        let ast::path::PathWithSegmentData { path, data: _data } =
            finished_walk.into_value().unwrap();

        let value = ctx.function_mut().values_mut().id_for_path(&path);
        ctx.into_result(value)
    }
}

impl std::fmt::Display for PathInExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.underlying_path)
    }
}

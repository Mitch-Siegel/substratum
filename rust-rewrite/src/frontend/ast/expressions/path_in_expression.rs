use serde::{Deserialize, Serialize};

use crate::{
    frontend::{
        ast::{self, Ast},
        sourceloc,
    },
    midend::{
        self,
        symtab::ValuePath,
        treewalk::{self},
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

impl<U, P, C> treewalk::Linearize<U, P, C> for PathInExpressionTree
where
    U: treewalk::UnpathedLinearizeCtxTrait,
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = midend::ir::ValueId;
    fn linearize_inner(self, mut _ctx: C) -> midend::treewalk::LinearizeResult<Self::Data, U> {
        unimplemented!();
        /*
        let _span = trace::span_auto_debug!(
            "treewalk::linearize for PathInexpressionTree @",
            "{:?}",
            self.loc()
        );

        let linearized_path = self.underlying_path.linearize(ctx);
        let path = linearized_path
            .map_data(|path, maybe_data| record_monomorphization(ctx, path, maybe_data))
            .unwrap();

        ctx.function_mut().values_mut().id_for_path(path)
        */
    }
}

impl std::fmt::Display for PathInExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.underlying_path)
    }
}

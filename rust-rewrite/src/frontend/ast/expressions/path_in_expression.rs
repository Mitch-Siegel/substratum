use serde::{Deserialize, Serialize};

use crate::{
    frontend::{
        ast::{self, Ast},
        sourceloc,
    },
    midend::{self},
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathInExpressionTree {
    pub underlying_path: ast::path::PathTree<ast::generics::GenericArgsListTree>,
}

impl Ast for PathInExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.underlying_path.loc()
    }
}

impl midend::treewalk::Collect<midend::symtab::ValuePath> for PathInExpressionTree {
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        Ok(ctx.take())
    }
}

impl midend::treewalk::Linearize<midend::symtab::ValuePath> for PathInExpressionTree {
    type Data = midend::ir::ValueId;
    fn linearize(
        self,
        mut _ctx: midend::treewalk::ValueLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
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

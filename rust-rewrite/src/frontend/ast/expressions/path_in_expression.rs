use serde::{Deserialize, Serialize};

use crate::{
    frontend::{
        ast::{self, Ast},
        sourceloc,
    },
    midend::{self, treewalk::Treewalk},
    trace,
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

impl midend::treewalk::Treewalk<midend::ir::ValueId> for PathInExpressionTree {
    fn collect_symbols(&self, _ctx: &mut midend::treewalk::CollectCtx) {
        ()
    }

    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> midend::ir::ValueId {
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

pub fn record_monomorphization(
    ctx: &mut midend::treewalk::LinearizeCtx,
    path: &midend::symtab::DefPath,
    maybe_generics: Option<ast::generics::GenericArgsListTree>,
) {
    unimplemented!();

    // TODO: checking for correct number of params
    /*let generics = match maybe_generics {
        Some(g) => g.linearize(ctx),
        None => return,
    };

    use midend::symtab::DefPathComponent;

    let generic_params = match path.last() {
        DefPathComponent::Type(_) => ctx
            .lookup_at::<midend::symtab::TypeDefinition>(path)
            .unwrap()
            .generic_params()
            .clone(),
        DefPathComponent::Function(_) => ctx
            .lookup_at::<midend::symtab::Function>(path)
            .unwrap()
            .prototype
            .generic_params
            .clone(),
        _ => panic!(),
    };

    // bare-minimum assertion that param counts are correct
    if generics.len() != generic_params.len() {
        panic!(
            "expected {} params for {} ({:?}), only found {}",
            generic_params.len(),
            path,
            generic_params,
            generics.len()
        );
    }

    let substs =
        midend::types::ParamSubstMap::new(generic_params.into_iter().zip(generics.into_iter()));
    ctx.symtab_mut()
        .types
        .record_monomorphization(path.clone(), substs)
        .unwrap();*/
}

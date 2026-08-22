use frontend::ast::{self, Ast};

use crate::{
    symtab,
    treewalk::{
        Collect, CollectResult, Linearize, LinearizeResult, PathedLinearizeCtxTrait,
        TypeCollectCtx, UnpathedLinearizeCtxTrait,
    },
};

impl Collect<symtab::TypePath> for ast::items::ImplementationTree {
    fn collect_inner(&self, ctx: TypeCollectCtx) -> CollectResult {
        ctx.into_result()
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::items::ImplementationTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = ();
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        unimplemented!();
        /*
        let for_name = self.for_.linearize(ctx);

        let implemented_for_type = ctx.disambiguate_named_type(&for_name).unwrap();
        match implemented_for_type {
            types::Syntactic::Named(_) => (),
            _ => panic!(
                "unexpected disambiguation of name {} to type {}",
                for_name, implemented_for_type
            ),
        }

        let generic_params: types::GenericParamsList = match self.generic_params {
            Some(params) => params.linearize(ctx),
            None => Vec::new(),
        };

        let implemented_for_generic_params: types::GenericParamsList =
            match self.implemented_for_generic_params {
                Some(params) => params.linearize(ctx),
                None => Vec::new(),
            };

        let impl_def_path_component = symtab::DefPathComponent::Implementation(
            symtab::ImplementationName::new(
                generic_params.clone(),
                implemented_for_type,
                implemented_for_generic_params,
            ),
        );
        ctx.push_def_path(impl_def_path_component.clone(), &generic_params);

        for item in self.items {
            item.linearize(ctx);
        }

        ctx.pop_def_path(impl_def_path_component).unwrap();
        */
    }
}

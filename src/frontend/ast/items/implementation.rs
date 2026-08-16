use crate::{
    frontend::ast::{
        generics, items, midend, sourceloc, Ast, Display, IdentifierTree, NameReflectable,
        ReflectName,
    },
    midend::{
        symtab,
        treewalk::{self},
    },
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct ImplementationTree {
    pub impl_keyword_loc: sourceloc::SourceSpan,
    pub generic_params: generics::OptionalGenericParamsListTree,
    pub for_: IdentifierTree,
    pub implemented_for_generic_params: generics::OptionalGenericParamsListTree,
    pub items: Vec<items::FunctionDefinitionTree>,
    pub close_brace_loc: sourceloc::SourceSpan,
}

impl Ast for ImplementationTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.impl_keyword_loc
            .clone()
            .merge(&self.close_brace_loc)
            .unwrap()
    }
}

impl midend::treewalk::Collect<midend::symtab::TypePath> for ImplementationTree {
    fn collect_inner(
        &self,
        ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.into_result()
    }
}

impl<U, P, C> treewalk::Linearize<U, P, C> for ImplementationTree
where
    U: treewalk::UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = ();
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(self, ctx: C) -> treewalk::LinearizeResult<Self::Data, U> {
        unimplemented!();
        /*
        let for_name = self.for_.linearize(ctx);

        let implemented_for_type = ctx.disambiguate_named_type(&for_name).unwrap();
        match implemented_for_type {
            midend::types::Syntactic::Named(_) => (),
            _ => panic!(
                "unexpected disambiguation of name {} to type {}",
                for_name, implemented_for_type
            ),
        }

        let generic_params: midend::types::GenericParamsList = match self.generic_params {
            Some(params) => params.linearize(ctx),
            None => Vec::new(),
        };

        let implemented_for_generic_params: midend::types::GenericParamsList =
            match self.implemented_for_generic_params {
                Some(params) => params.linearize(ctx),
                None => Vec::new(),
            };

        let impl_def_path_component = midend::symtab::DefPathComponent::Implementation(
            midend::symtab::ImplementationName::new(
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

impl Display for ImplementationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut params: String = format!("{}", self.generic_params);
        if !params.is_empty() {
            params = format!("<{params}>");
        }

        let mut for_params = format!("{}", self.implemented_for_generic_params);
        if !for_params.is_empty() {
            for_params = format!("<{for_params}>");
        }

        write!(f, "Impl{} {}{}", params, self.for_, for_params).and_then(|()| {
            for item in &self.items {
                write!(f, "{item}")?;
            }
            Ok(())
        })
    }
}

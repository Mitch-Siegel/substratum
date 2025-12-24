use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ImplementationTree {
    pub impl_keyword_loc: sourceloc::SourceSpan,
    pub generic_params: Option<generics::GenericParamsListTree>,
    pub for_: IdentifierTree,
    pub implemented_for_generic_params: Option<generics::GenericParamsListTree>,
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

impl midend::treewalk::Treewalk<()> for ImplementationTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        unimplemented!();
        /*
         let generic_params_as_vec = match &self.generic_params {
            Some(params) => params
                .clone()
                .linearize_ctxless()
                .into_iter()
                .map(|(_, p)| p)
                .collect(),
            None => midend::types::GenericParamsList::new(),
        };

        let implemented_for_generic_params_as_vec: midend::types::GenericParamsList =
            match &self.implemented_for_generic_params {
                Some(params) => params
                    .clone()
                    .linearize_ctxless()
                    .into_iter()
                    .map(|(_, p)| p)
                    .collect(),
                None => Vec::new(),
            };

        let impl_def_path_component = midend::symtab::DefPathComponent::Implementation(
            midend::symtab::ImplementationName::new(
                generic_params_as_vec,
                midend::types::Syntactic::Named(self.for_.value.clone()),
                implemented_for_generic_params_as_vec,
            ),
        );

        // declare the impl, then push it to the defpath
        ctx.declare(impl_def_path_component.clone()).unwrap();
        ctx.push_def_path(impl_def_path_component.clone()).unwrap();

        for item in &self.items {
            item.collect_symbols(ctx);
        }

        ctx.pop_def_path(impl_def_path_component).unwrap();
        */
    }

    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> () {
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
        let generic_params_string = match &self.generic_params {
            Some(params) => String::from(format!("<{}>", params)),
            None => String::new(),
        };

        let for_generic_params_string = match &self.implemented_for_generic_params {
            Some(params) => String::from(format!("<{}>", params)),
            None => String::new(),
        };

        write!(
            f,
            "Impl{} {}{}",
            generic_params_string, self.for_, for_generic_params_string
        )
        .and_then(|_| {
            for item in &self.items {
                write!(f, "{}", item)?
            }
            Ok(())
        })
    }
}

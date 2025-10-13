use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ImplementationTree {
    pub loc: SourceLoc,
    pub generic_params: Option<generics::GenericParamsListTree>,
    pub for_: String,
    pub implemented_for_generic_params: Option<generics::GenericParamsListTree>,
    pub items: Vec<items::FunctionDefinitionTree>,
}

impl ImplementationTree {
    pub fn new(
        loc: SourceLoc,
        generic_params: Option<generics::GenericParamsListTree>,
        for_: String,
        implemented_for_generic_params: Option<generics::GenericParamsListTree>,
        items: Vec<items::FunctionDefinitionTree>,
    ) -> Self {
        Self {
            loc,
            generic_params,
            for_,
            implemented_for_generic_params,
            items,
        }
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

impl treewalk::CollectSymbols for ImplementationTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        let generic_params_as_vec = match &self.generic_params {
            Some(params) => params.clone().as_vec(),
            None => Vec::new(),
        };

        let implemented_for_generic_params_as_vec = match &self.implemented_for_generic_params {
            Some(params) => params.clone().as_vec(),
            None => Vec::new(),
        };

        let impl_def_path_component = midend::symtab::DefPathComponent::Implementation(
            midend::symtab::ImplementationName::new(
                generic_params_as_vec,
                midend::types::Syntactic::Named(self.for_.clone()),
                implemented_for_generic_params_as_vec,
            ),
        );

        ctx.push_def_path(impl_def_path_component.clone()).unwrap();
        for item in &self.items {
            item.collect_symbols(ctx);
        }

        ctx.pop_def_path(impl_def_path_component).unwrap();
    }
}

impl treewalk::Linearize<()> for ImplementationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> () {
        let implemented_for_type = ctx.resolve_type_name(&self.for_).unwrap();

        let generic_params: Vec<String> = match self.generic_params {
            Some(params) => params.linearize(ctx),
            None => Vec::new(),
        };

        let implemented_for_generic_params = match self.implemented_for_generic_params {
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
    }
}

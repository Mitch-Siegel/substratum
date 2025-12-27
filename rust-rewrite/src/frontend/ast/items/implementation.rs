use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ImplementationTree {
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

impl midend::treewalk::Collect for ImplementationTree {
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        return Ok(ctx.take());
    }
}

impl midend::treewalk::Linearize for ImplementationTree {
    type Data = ();
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
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
        let mut generic_params_string = String::from(format!("{}", &self.generic_params));
        if generic_params_string.len() > 0 {
            generic_params_string = String::from(format!("<{}>", generic_params_string))
        };

        let mut for_generic_params_string =
            String::from(format!("{}", &self.implemented_for_generic_params));
        if for_generic_params_string.len() > 0 {
            for_generic_params_string = String::from(format!("<{}>", for_generic_params_string))
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

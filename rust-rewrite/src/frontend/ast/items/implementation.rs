use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ImplementationTree {
    pub loc: SourceLoc,
    pub generic_params: Option<generics::GenericParamsListTree>,
    pub for_: generics::IdentifierWithGenericsTree,
    pub items: Vec<items::FunctionDefinitionTree>,
}

impl ImplementationTree {
    pub fn new(
        loc: SourceLoc,
        generic_params: Option<generics::GenericParamsListTree>,
        for_: generics::IdentifierWithGenericsTree,
        items: Vec<items::FunctionDefinitionTree>,
    ) -> Self {
        Self {
            loc,
            generic_params,
            for_,
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

        write!(f, "Impl{} {}", generic_params_string, self.for_).and_then(|_| {
            for item in &self.items {
                write!(f, "{}", item)?
            }
            Ok(())
        })
    }
}

impl CustomReturnWalk<midend::linearizer::BasicDefContext, midend::linearizer::BasicDefContext>
    for ImplementationTree
{
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(
        self,
        mut context: midend::linearizer::BasicDefContext,
    ) -> midend::linearizer::BasicDefContext {
        let (implemented_for_string_name, _implemented_for_generic_params) = self.for_.walk(());
        let implemented_for_type = context
            .resolve_type_name(&implemented_for_string_name)
            .unwrap();
        let implemented_for_type_id = context.id_for_type(&implemented_for_type).unwrap();

        let generic_params: Vec<String> = match self.generic_params {
            Some(params) => params.walk(()),
            None => Vec::new(),
        };

        let impl_def_path_component =
            midend::symtab::DefPathComponent::Implementation(midend::symtab::ImplementationName {
                implemented_for: implemented_for_type_id,
                generic_params: generic_params.clone(),
            });
        context.push_def_path(impl_def_path_component.clone(), &generic_params);

        for item in self.items {
            context = item.walk(context);
        }

        context.pop_def_path(impl_def_path_component).unwrap();

        context
    }
}

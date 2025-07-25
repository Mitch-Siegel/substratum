use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ImplementationTree {
    pub loc: SourceLocWithMod,
    pub generic_params: Option<generics::GenericParamsListTree>,
    pub for_: generics::IdentifierWithGenericsTree,
    pub items: Vec<items::FunctionDefinitionTree>,
}
impl ImplementationTree {
    pub fn new(
        loc: SourceLocWithMod,
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

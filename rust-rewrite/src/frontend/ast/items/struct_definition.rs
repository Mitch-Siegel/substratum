use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StructFieldTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub type_: TypeTree,
}
impl StructFieldTree {
    pub fn new(loc: SourceLocWithMod, name: String, type_: TypeTree) -> Self {
        Self { loc, name, type_ }
    }
}
impl Display for StructFieldTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_)
    }
}

impl ReturnWalk<(String, midend::types::Type)> for StructFieldTree {
    fn walk(
        self,
        context: &mut impl midend::linearizer::DefContext,
    ) -> (String, midend::types::Type) {
        let field_type = self.type_.walk(context);
        (self.name, field_type)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StructDefinitionTree {
    pub loc: SourceLocWithMod,
    pub name: generics::IdentifierWithGenericsTree,
    pub fields: Vec<StructFieldTree>,
}

impl StructDefinitionTree {
    pub fn new(
        loc: SourceLocWithMod,
        name: generics::IdentifierWithGenericsTree,
        fields: Vec<StructFieldTree>,
    ) -> Self {
        Self { loc, name, fields }
    }
}

impl Display for StructDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut fields = String::new();
        for field in &self.fields {
            fields += &field.to_string();
            fields += " ";
        }

        write!(f, "Struct Definition: {}: {}", self.name, fields)
    }
}

impl ReturnWalk<midend::symtab::StructRepr> for StructDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl midend::linearizer::DefContext) -> midend::symtab::StructRepr {
        let (string_name, generic_params) = self.name.walk(());
        let type_def_path_component =
            midend::symtab::DefPathComponent::Type(midend::types::Type::Named(string_name.clone()));
        context.push_def_path(type_def_path_component.clone(), &generic_params);

        let fields = self
            .fields
            .into_iter()
            .map(|field| field.walk(context))
            .collect::<Vec<_>>();

        context.pop_def_path(type_def_path_component).unwrap();
        midend::symtab::StructRepr::new(string_name, generic_params, fields).unwrap()
    }
}

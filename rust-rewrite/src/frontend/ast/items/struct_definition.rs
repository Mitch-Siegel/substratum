use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StructFieldTree {
    pub loc: SourceLoc,
    pub name: String,
    pub type_: TypeTree,
}
impl StructFieldTree {
    pub fn new(loc: SourceLoc, name: String, type_: TypeTree) -> Self {
        Self { loc, name, type_ }
    }
}
impl Display for StructFieldTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_)
    }
}

impl treewalk::Linearize<(String, midend::types::Syntactic)> for StructFieldTree {
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> (String, midend::types::Syntactic) {
        let field_type = self.type_.linearize(ctx);
        (self.name, field_type)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StructDefinitionTree {
    pub loc: SourceLoc,
    pub name: generics::IdentifierWithGenericsTree,
    pub fields: Vec<StructFieldTree>,
}

impl StructDefinitionTree {
    pub fn new(
        loc: SourceLoc,
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

impl treewalk::CollectSymbols for StructDefinitionTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        ctx.declare(midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(self.name.name.clone()),
        ))
        .unwrap();
    }
}

impl treewalk::Linearize<midend::symtab::StructRepr> for StructDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::symtab::StructRepr {
        let (string_name, generic_params) = self.name.linearize(ctx);
        let type_def_path_component = midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(string_name.clone()),
        );
        ctx.push_def_path(type_def_path_component.clone(), &generic_params);

        let fields = self
            .fields
            .into_iter()
            .map(|field| field.linearize(ctx))
            .collect::<Vec<_>>();

        ctx.pop_def_path(type_def_path_component).unwrap();
        midend::symtab::StructRepr::new(string_name, generic_params, fields).unwrap()
    }
}

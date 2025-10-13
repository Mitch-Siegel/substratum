use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum EnumVariantData {
    TupleData(Vec<TypeTree>),
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct EnumVariantDataTree {
    pub loc: SourceLoc,
    pub data: EnumVariantData,
}

impl treewalk::Linearize<midend::symtab::enum_definition::EnumVariantRepr> for EnumVariantDataTree {
    fn linearize(
        self,
        context: &mut treewalk::LinearizeCtx,
    ) -> midend::symtab::enum_definition::EnumVariantRepr {
        match self.data {
            EnumVariantData::TupleData(elements) => {
                midend::symtab::enum_definition::EnumVariantRepr::Tuple(
                    elements
                        .into_iter()
                        .map(|type_tree| type_tree.linearize(context))
                        .collect(),
                )
            }
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct EnumVariantTree {
    pub loc: SourceLoc,
    pub name: String,
    pub data: Option<EnumVariantDataTree>,
}
impl EnumVariantTree {
    pub fn new(loc: SourceLoc, name: String, data: Option<EnumVariantDataTree>) -> Self {
        Self { loc, name, data }
    }
}
impl Display for EnumVariantTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.data {
            Some(variant_data) => write!(f, "{}: {:?}", self.name, variant_data),
            None => write!(f, "{}", self.name),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct EnumDefinitionTree {
    pub loc: SourceLoc,
    pub name: generics::IdentifierWithGenericsTree,
    pub variants: Vec<EnumVariantTree>,
}

impl EnumDefinitionTree {
    pub fn new(
        loc: SourceLoc,
        name: generics::IdentifierWithGenericsTree,
        variants: Vec<EnumVariantTree>,
    ) -> Self {
        Self {
            loc,
            name,
            variants,
        }
    }
}

impl Display for EnumDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut fields = String::new();
        for variant in &self.variants {
            fields += &variant.to_string();
            fields += " ";
        }

        write!(f, "Enum Definition: {}: {}", self.name, fields)
    }
}

impl treewalk::CollectSymbols for EnumDefinitionTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        ctx.declare(midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(self.name.name.clone()),
        ))
        .unwrap();
    }
}

impl treewalk::Linearize<midend::symtab::EnumRepr> for EnumDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::symtab::EnumRepr {
        let (string_name, generic_params) = self.name.linearize(ctx);
        let type_def_path_component = midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(string_name.clone()),
        );

        ctx.push_def_path(type_def_path_component.clone(), &generic_params);

        let variants: Vec<(String, midend::symtab::EnumVariantRepr)> = self
            .variants
            .into_iter()
            .map(|variant| {
                let variant_data_type = match variant.data {
                    Some(variant_item) => variant_item.linearize(ctx),
                    None => midend::symtab::EnumVariantRepr::Unit,
                };
                (variant.name, variant_data_type)
            })
            .collect::<Vec<_>>();

        ctx.pop_def_path(type_def_path_component).unwrap();
        midend::symtab::EnumRepr::new(string_name, generic_params, variants).unwrap()
    }
}

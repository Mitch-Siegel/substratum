use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum EnumVariantData {
    TupleData(Vec<TypeTree>),
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct EnumVariantDataTree {
    pub loc: SourceLoc,
    pub data: EnumVariantData,
}

impl ReturnWalk<midend::symtab::enum_definition::EnumVariantRepr> for EnumVariantDataTree {
    fn walk(
        self,
        context: &mut impl DefContext,
    ) -> midend::symtab::enum_definition::EnumVariantRepr {
        match self.data {
            EnumVariantData::TupleData(elements) => {
                midend::symtab::enum_definition::EnumVariantRepr::Tuple(
                    elements
                        .into_iter()
                        .map(|type_tree| type_tree.walk(context))
                        .collect(),
                )
            }
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
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

impl ReturnWalk<midend::symtab::enum_definition::EnumVariant> for EnumVariantTree {
    fn walk(self, context: &mut impl DefContext) -> midend::symtab::enum_definition::EnumVariant {
        let data = match self.data {
            Some(variant_data) => variant_data.walk(context),
            None => midend::symtab::enum_definition::EnumVariantRepr::Unit,
        };

        midend::symtab::enum_definition::EnumVariant::new(self.name, data)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
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

impl ReturnWalk<midend::symtab::EnumRepr> for EnumDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl midend::linearizer::DefContext) -> midend::symtab::EnumRepr {
        let (string_name, generic_params) = self.name.walk(());
        let type_def_path_component = midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(string_name.clone()),
        );
        context.push_def_path(type_def_path_component.clone(), &generic_params);

        let variants: Vec<(String, midend::symtab::EnumVariantRepr)> = self
            .variants
            .into_iter()
            .map(|variant| {
                let variant_data_type = match variant.data {
                    Some(variant_item) => variant_item.walk(context),
                    None => midend::symtab::EnumVariantRepr::Unit,
                };
                (variant.name, variant_data_type)
            })
            .collect::<Vec<_>>();

        context.pop_def_path(type_def_path_component).unwrap();
        midend::symtab::EnumRepr::new(string_name, generic_params, variants).unwrap()
    }
}

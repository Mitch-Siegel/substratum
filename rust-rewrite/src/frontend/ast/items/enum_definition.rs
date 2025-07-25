use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct EnumVariantTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub data: Option<TypeTree>,
}
impl EnumVariantTree {
    pub fn new(loc: SourceLocWithMod, name: String, data: Option<TypeTree>) -> Self {
        Self { loc, name, data }
    }
}
impl Display for EnumVariantTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.data {
            Some(variant_type) => write!(f, "{}: {}", self.name, variant_type),
            None => write!(f, "{}", self.name),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct EnumDefinitionTree {
    pub loc: SourceLocWithMod,
    pub name: generics::IdentifierWithGenericsTree,
    pub variants: Vec<EnumVariantTree>,
}
impl EnumDefinitionTree {
    pub fn new(
        loc: SourceLocWithMod,
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

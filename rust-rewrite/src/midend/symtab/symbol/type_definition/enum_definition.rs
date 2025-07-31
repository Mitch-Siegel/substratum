use crate::midend::symtab::type_definition::*;

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct EnumVariant {
    pub name: String,
    pub type_: Option<types::Type>,
}

impl EnumVariant {
    pub fn new(name: String, type_: Option<types::Type>) -> Self {
        Self { name, type_ }
    }
}

impl std::fmt::Display for EnumVariant {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.type_ {
            Some(type_) => write!(f, "{}({})", self.name, type_),
            None => write!(f, "{}", self.name),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct EnumRepr {
    pub name: String,
    generic_params: Vec<String>,
    variants: BTreeMap<String, EnumVariant>,
    size: Option<usize>,
    alignment: Option<usize>,
}

impl EnumRepr {
    pub fn new(
        name: String,
        generic_params: Vec<String>,
        variant_definitions: Vec<(String, Option<types::Type>)>,
    ) -> Result<Self, EnumVariant> {
        let mut variants = BTreeMap::<String, EnumVariant>::new();
        for (name, type_) in variant_definitions {
            let variant = EnumVariant::new(name.clone(), type_);
            match variants.insert(name, variant) {
                Some(existing_variant) => return Err(existing_variant),
                None => (),
            }
        }

        Ok(Self {
            name,
            generic_params,
            variants,
            size: None,
            alignment: None,
        })
    }

    pub fn get_variant(&self, variant_name: &String) -> Option<&EnumVariant> {
        self.variants.get(variant_name)
    }
}

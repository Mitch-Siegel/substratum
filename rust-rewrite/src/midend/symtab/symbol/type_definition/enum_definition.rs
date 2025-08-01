use crate::midend::symtab::type_definition::*;

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub enum EnumVariantRepr {
    Unit,
    Tuple(Vec<types::Type>),
    // TODO: struct-like enums
    //   break out StructRepr logic to minimal subset for reuse here?
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct EnumVariant {
    pub name: String,
    pub data: EnumVariantRepr,
}

impl EnumVariant {
    pub fn new(name: String, data: EnumVariantRepr) -> Self {
        Self { name, data }
    }

    pub fn new_unit(name: String) -> Self {
        Self {
            name,
            data: EnumVariantRepr::Unit,
        }
    }

    pub fn new_tuple(name: String, elements: Vec<types::Type>) -> Self {
        Self {
            name,
            data: EnumVariantRepr::Tuple(elements),
        }
    }

    pub fn type_(&self) -> types::Type {
        match &self.data {
            EnumVariantRepr::Unit => types::Type::Unit,
            EnumVariantRepr::Tuple(elements) => types::Type::Tuple(elements.clone()),
        }
    }
}

impl std::fmt::Display for EnumVariant {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)?;
        match &self.data {
            EnumVariantRepr::Unit => Ok(()),
            EnumVariantRepr::Tuple(elements) => write!(f, "{:?}", elements),
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
        variant_definitions: Vec<(String, EnumVariantRepr)>,
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

use crate::midend::symtab::type_definition::*;

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub enum EnumVariantRepr {
    Unit,
    Tuple(Vec<types::Syntactic>),
    // TODO: struct-like enums
    //   break out StructRepr logic to minimal subset for reuse here?
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct EnumVariant {
    pub discriminant: usize,
    pub name: String,
    pub data: EnumVariantRepr,
}

impl EnumVariant {
    pub fn new(discriminant: usize, name: String, data: EnumVariantRepr) -> Self {
        Self {
            discriminant,
            name,
            data,
        }
    }

    pub fn new_unit(discriminant: usize, name: String) -> Self {
        Self {
            discriminant,
            name,
            data: EnumVariantRepr::Unit,
        }
    }

    pub fn new_tuple(discriminant: usize, name: String, elements: Vec<types::Syntactic>) -> Self {
        Self {
            discriminant,
            name,
            data: EnumVariantRepr::Tuple(elements),
        }
    }

    pub fn syntactic(&self) -> types::Syntactic {
        match &self.data {
            EnumVariantRepr::Unit => types::Syntactic::Unit,
            EnumVariantRepr::Tuple(elements) => types::Syntactic::Tuple(elements.clone()),
        }
    }

    pub fn data(&self) -> &EnumVariantRepr {
        &self.data
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
    discriminants: BTreeMap<String, usize>,
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
        for (discriminant, (name, type_)) in variant_definitions.into_iter().enumerate() {
            let variant = EnumVariant::new(discriminant, name.clone(), type_);
            match variants.insert(name, variant) {
                Some(existing_variant) => return Err(existing_variant),
                None => (),
            }
        }

        let discriminants: BTreeMap<String, usize> = variants
            .values()
            .map(|variant| (variant.name.clone(), variant.discriminant))
            .collect();

        Ok(Self {
            name,
            generic_params,
            variants,
            discriminants,
            size: None,
            alignment: None,
        })
    }

    pub fn variants(&self) -> &BTreeMap<String, EnumVariant> {
        &self.variants
    }

    pub fn get_variant(&self, variant_name: &String) -> Option<&EnumVariant> {
        self.variants.get(variant_name)
    }
}

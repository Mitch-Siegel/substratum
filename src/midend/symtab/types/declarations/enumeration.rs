use crate::midend::symtab::{midend, BTreeMap};
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub(crate) enum EnumVariantRepr {
    Unit,
    Tuple(Vec<midend::types::Syntactic>),
    // TODO: struct-like enums
    //   break out StructRepr logic to minimal subset for reuse here?
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub(crate) struct EnumVariant {
    pub discriminant: usize,
    pub name: String,
    pub data: EnumVariantRepr,
}

impl EnumVariant {
    pub(crate) fn new(discriminant: usize, name: String, data: EnumVariantRepr) -> Self {
        Self {
            discriminant,
            name,
            data,
        }
    }

    pub(crate) fn new_unit(discriminant: usize, name: String) -> Self {
        Self {
            discriminant,
            name,
            data: EnumVariantRepr::Unit,
        }
    }

    pub(crate) fn new_tuple(
        discriminant: usize,
        name: String,
        elements: Vec<midend::types::Syntactic>,
    ) -> Self {
        Self {
            discriminant,
            name,
            data: EnumVariantRepr::Tuple(elements),
        }
    }

    pub(crate) fn syntactic(&self) -> midend::types::Syntactic {
        match &self.data {
            EnumVariantRepr::Unit => midend::types::Syntactic::Unit,
            EnumVariantRepr::Tuple(elements) => {
                let wrapped_elems: Vec<Option<midend::types::Syntactic>> =
                    elements.iter().map(|ty| Some(ty.clone())).collect();
                midend::types::Syntactic::Tuple(wrapped_elems)
            }
        }
    }

    pub(crate) fn data(&self) -> &EnumVariantRepr {
        &self.data
    }
}

impl std::fmt::Display for EnumVariant {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)?;
        match &self.data {
            EnumVariantRepr::Unit => Ok(()),
            EnumVariantRepr::Tuple(elements) => write!(f, "{elements:?}"),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub(crate) struct EnumRepr {
    pub name: String,
    variants: BTreeMap<String, EnumVariant>,
    discriminants: BTreeMap<String, usize>,
    size: Option<usize>,
    alignment: Option<usize>,
}

impl EnumRepr {
    pub(crate) fn new(
        name: String,
        variant_definitions: Vec<(String, EnumVariantRepr)>,
    ) -> Result<Self, EnumVariant> {
        let mut variants = BTreeMap::<String, EnumVariant>::new();
        for (discriminant, (name, type_)) in variant_definitions.into_iter().enumerate() {
            let variant = EnumVariant::new(discriminant, name.clone(), type_);
            if let Some(existing_variant) = variants.insert(name, variant) {
                return Err(existing_variant);
            }
        }

        let discriminants: BTreeMap<String, usize> = variants
            .values()
            .map(|variant| (variant.name.clone(), variant.discriminant))
            .collect();

        Ok(Self {
            name,
            variants,
            discriminants,
            size: None,
            alignment: None,
        })
    }

    pub(crate) fn variants(&self) -> &BTreeMap<String, EnumVariant> {
        &self.variants
    }

    pub(crate) fn get_variant(&self, variant_name: &String) -> Option<&EnumVariant> {
        self.variants.get(variant_name)
    }
}

impl std::fmt::Display for EnumRepr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "enum {}", self.name)
    }
}

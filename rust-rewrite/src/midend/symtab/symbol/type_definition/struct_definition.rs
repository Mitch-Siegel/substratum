use crate::midend::symtab::type_definition::*;

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct StructField {
    pub name: String,
    pub type_: types::Syntactic,
    pub offset: Option<usize>,
}

impl StructField {
    pub fn new(name: String, type_: types::Syntactic) -> Self {
        Self {
            name,
            type_,
            offset: None,
        }
    }
}

impl std::fmt::Display for StructField {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self.offset {
            Some(offset) => write!(f, "{}: {} (@{})", self.name, self.type_, offset),
            None => write!(f, "{}: {}", self.name, self.type_),
        }
    }
}

#[derive(Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub struct StructRepr {
    pub name: String,
    generic_params: Vec<String>,
    field_order: Vec<String>,
    fields: BTreeMap<String, StructField>,
    size: Option<usize>,
    alignment: Option<usize>,
}

impl StructRepr {
    pub fn new(
        name: String,
        generic_params: Vec<String>,
        field_definitions: Vec<(String, types::Syntactic)>,
    ) -> Result<Self, StructField> {
        let field_order: Vec<String> = field_definitions
            .iter()
            .map(|(name, _)| name.clone())
            .collect();

        let mut fields = BTreeMap::<String, StructField>::new();
        for (name, type_) in field_definitions {
            trace::trace!("Insert struct field {} (type: {})", name, type_,);
            let field = StructField::new(name.clone(), type_);
            match fields.insert(name, field) {
                Some(existing_field) => return Err(existing_field),
                None => (),
            }
        }

        Ok(Self {
            name,
            generic_params,
            field_order,
            fields,
            size: None,
            alignment: None,
        })
    }

    pub fn lookup_field(&self, name: &str) -> Result<&StructField, String> {
        match self.fields.get(name) {
            Some(field) => Ok(field),
            None => Err(name.into()),
        }
    }
}

impl<'a> IntoIterator for &'a StructRepr {
    type Item = (&'a String, &'a StructField);
    type IntoIter = std::collections::btree_map::Iter<'a, String, StructField>;

    fn into_iter(self) -> Self::IntoIter {
        self.fields.iter()
    }
}

use crate::midend::symtab::{midend, trace, BTreeMap};
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub(crate) struct FieldRepr {
    pub name: String,
    pub(crate) type_: midend::types::Syntactic,
    pub offset: Option<usize>,
}

impl FieldRepr {
    pub(crate) fn new(name: String, type_: midend::types::Syntactic) -> Self {
        Self {
            name,
            type_,
            offset: None,
        }
    }
}

impl std::fmt::Display for FieldRepr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self.offset {
            Some(offset) => write!(f, "{}: {} (@{})", self.name, self.type_, offset),
            None => write!(f, "{}: {}", self.name, self.type_),
        }
    }
}

#[derive(Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub(crate) struct StructRepr {
    pub name: String,
    field_order: Vec<String>,
    fields: BTreeMap<String, FieldRepr>,
    size: Option<usize>,
    alignment: Option<usize>,
}

impl StructRepr {
    pub(crate) fn new(
        name: String,
        field_definitions: Vec<(String, midend::types::Syntactic)>,
    ) -> Result<Self, FieldRepr> // TODO: struct duplicat field error
    {
        let field_order: Vec<String> = field_definitions
            .iter()
            .map(|(name, _)| name.clone())
            .collect();

        let mut fields = BTreeMap::<String, FieldRepr>::new();
        for (name, type_) in field_definitions {
            trace::trace!("Insert struct field {} (type: {})", name, type_,);
            let field = FieldRepr::new(name.clone(), type_);
            if let Some(existing_field) = fields.insert(name, field) {
                return Err(existing_field);
            }
        }

        Ok(Self {
            name,
            field_order,
            fields,
            size: None,
            alignment: None,
        })
    }

    pub(crate) fn lookup_field(&self, name: &str) -> Result<&FieldRepr, String> {
        match self.fields.get(name) {
            Some(field) => Ok(field),
            None => Err(name.into()),
        }
    }
}

impl std::fmt::Display for StructRepr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "struct {}", self.name)
    }
}

impl<'a> IntoIterator for &'a StructRepr {
    type Item = (&'a String, &'a FieldRepr);
    type IntoIter = std::collections::btree_map::Iter<'a, String, FieldRepr>;

    fn into_iter(self) -> Self::IntoIter {
        self.fields.iter()
    }
}

use std::collections::{BTreeMap, HashMap};

use serde::{Deserialize, Serialize};

use crate::{
    backend,
    midend::{symtab::*, types},
};

use super::Function;

pub type TypeId = usize;
pub type GenericParams = Vec<Type>;

#[derive(Clone, PartialEq, Eq, Hash)]
pub struct TypeKey {
    pub definition_path: DefPath,
    pub generic_params: GenericParams,
}

impl TypeKey {
    pub fn new(definition_path: DefPath, generic_params: GenericParams) -> Self {
        Self {
            definition_path,
            generic_params,
        }
    }
}

pub struct TypeInterner {
    ids: HashMap<TypeKey, TypeId>,
    types: HashMap<TypeId, TypeDefinition>,
}

impl TypeInterner {
    pub fn new() -> Self {
        Self {
            ids: HashMap::new(),
            types: HashMap::new(),
        }
    }

    pub fn insert(
        &mut self,
        key: TypeKey,
        definition: TypeDefinition,
    ) -> Result<TypeId, DefinedSymbol> {
        let defined_type = definition.type_.clone();

        let next_id = self.ids.len();
        let id = match self.ids.insert(key.clone(), next_id) {
            Some(existing_id) => return Err(DefinedSymbol::Type(key.definition_path)),
            None => next_id,
        };

        match self.types.insert(next_id, definition) {
            Some(existing_definition) => return Err(DefinedSymbol::Type(key.definition_path)),
            None => (),
        };

        Ok(next_id)
    }

    pub fn get_by_id(&self, id: &TypeId) -> Option<&TypeDefinition> {
        self.types.get(id)
    }

    pub fn get_by_key(&self, key: &TypeKey) -> Option<&TypeDefinition> {
        let id_from_key = self.ids.get(key)?;
        self.types.get(id_from_key)
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct TypeDefinition {
    type_: types::Type,
    pub repr: TypeRepr,
}

impl PartialEq for TypeDefinition {
    fn eq(&self, other: &Self) -> bool {
        self.type_.eq(&other.type_) && self.repr.eq(&other.repr)
    }
}

impl Eq for TypeDefinition {}

impl TypeDefinition {
    pub fn new(type_: types::Type, repr: TypeRepr) -> Self {
        TypeDefinition { type_, repr }
    }

    pub fn type_(&self) -> &types::Type {
        &self.type_
    }
}

#[derive(Clone, Debug, PartialOrd, Ord, Hash, PartialEq, Eq, Serialize, Deserialize)]
pub enum TypeRepr {
    UnsignedInteger(PrimitiveIntegerRepr),
    SignedInteger(PrimitiveIntegerRepr),
    Struct(StructRepr),
}

impl TypeRepr {
    pub fn name(&self) -> String {
        match self {
            Self::UnsignedInteger(repr) => format!("u{}", repr.size),
            Self::SignedInteger(repr) => format!("i{}", repr.size),
            Self::Struct(struct_repr) => struct_repr.name.clone(),
        }
    }
}

#[derive(Clone, Debug, Hash, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub struct PrimitiveIntegerRepr {
    size: usize,
}

impl PrimitiveIntegerRepr {
    pub fn new(size: usize) -> Self {
        Self { size }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct StructField {
    pub name: String,
    pub type_: types::Type,
    pub offset: Option<usize>,
}

impl StructField {
    pub fn new(name: String, type_: types::Type) -> Self {
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
    fields: BTreeMap<String, StructField>,
    size: Option<usize>,
    alignment: Option<usize>,
}

impl StructRepr {
    pub fn new(
        name: String,
        field_definitions: Vec<(String, types::Type)>,
    ) -> Result<Self, DefinedSymbol> {
        let mut fields = BTreeMap::<String, StructField>::new();
        for (name, type_) in field_definitions {
            trace::trace!("Insert struct field {} (type: {})", name, type_,);
            let field = StructField::new(name.clone(), type_);
            match fields.insert(name, field) {
                Some(existing_field) => return Err(DefinedSymbol::field(existing_field)),
                None => (),
            }
        }

        Ok(Self {
            name,
            fields,
            size: None,
            alignment: None,
        })
    }

    pub fn lookup_field(&self, name: &str) -> Result<&StructField, UndefinedSymbol> {
        match self.fields.get(name) {
            Some(field) => Ok(field),
            None => Err(UndefinedSymbol::field(name.into())),
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

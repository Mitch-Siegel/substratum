use std::collections::{BTreeMap, HashMap};

use serde::{Deserialize, Serialize};

use crate::{
    backend,
    midend::{symtab::*, types},
};

use super::Function;

pub type TypeId = usize;
pub type GenericParams = Vec<Type>;

pub struct TypeInterner<'a> {
    ids: HashMap<<TypeDefinition as Symbol<'a>>::SymbolKey, TypeId>,
    types: HashMap<TypeId, TypeDefinition>,
}

impl<'a> TypeInterner<'a> {
    pub fn new() -> Self {
        Self {
            ids: HashMap::new(),
            types: HashMap::new(),
        }
    }

    pub fn insert(
        &'a mut self,
        def_path: DefPath<'a>,
        definition: TypeDefinition,
    ) -> Result<TypeId, SymbolError> {
        assert!(&def_path.is_type());

        let next_id = self.ids.len();
        let key = definition.symbol_key();
        let id = match self.ids.insert(key.clone(), next_id) {
            Some(existing_id) => return Err(SymbolError::Defined(def_path.clone())),
            None => next_id,
        };

        match self.types.insert(next_id, definition) {
            Some(existing_definition) => return Err(SymbolError::Defined(def_path.clone())),
            None => (),
        };

        Ok(next_id)
    }

    pub fn get_by_id(&self, id: &TypeId) -> Option<&TypeDefinition> {
        self.types.get(id)
    }

    pub fn get_by_key(
        &self,
        key: &<TypeDefinition as Symbol<'a>>::SymbolKey,
    ) -> Option<&TypeDefinition> {
        let id_from_key = self.ids.get(key)?;
        self.types.get(id_from_key)
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct TypeDefinition {
    type_: types::Type,
    pub repr: TypeRepr,
}

impl TypeDefinition {
    pub fn new(type_: types::Type, repr: TypeRepr) -> Self {
        TypeDefinition { type_, repr }
    }

    pub fn type_(&self) -> &types::Type {
        &self.type_
    }
}

impl<'a> From<DefinitionResolver<'a>> for &'a TypeDefinition {
    fn from(resolver: DefinitionResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Type(type_id) => resolver.type_interner.get_by_id(type_id).unwrap(),
            symbol => panic!("Unexpected symbol seen for type: {}", symbol),
        }
    }
}

impl<'a> Into<DefPathComponent<'a>> for &TypeDefinition {
    fn into(self) -> DefPathComponent<'a> {
        DefPathComponent::Type(self.symbol_key().clone())
    }
}

impl<'a> Into<SymbolDef> for SymbolDefGenerator<'a, TypeDefinition> {
    fn into(self) -> SymbolDef {
        let type_id = self
            .type_interner
            .insert(self.def_path, self.to_generate_def_for)
            .unwrap();
        SymbolDef::Type(type_id)
    }
}

impl<'a> Symbol<'a> for TypeDefinition {
    type SymbolKey = types::Type;

    fn symbol_key(&self) -> &Self::SymbolKey {
        self.type_()
    }
}

impl PartialEq for TypeDefinition {
    fn eq(&self, other: &Self) -> bool {
        self.type_.eq(&other.type_) && self.repr.eq(&other.repr)
    }
}

impl Eq for TypeDefinition {}

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
    ) -> Result<Self, StructField> {
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

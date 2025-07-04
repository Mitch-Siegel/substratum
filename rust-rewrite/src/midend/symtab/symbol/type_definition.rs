use std::collections::{BTreeMap, HashMap};

use serde::{Deserialize, Serialize};

use crate::{
    backend,
    midend::{symtab::*, types},
};

pub mod struct_definition;

pub use struct_definition::*;

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

impl<'a> From<DefResolver<'a>> for &'a TypeDefinition {
    fn from(resolver: DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Type(type_id) => resolver.type_interner.get_by_id(type_id).unwrap(),
            symbol => panic!("Unexpected symbol seen for type: {}", symbol),
        }
    }
}
impl<'a> From<MutDefResolver<'a>> for &'a mut TypeDefinition {
    fn from(resolver: MutDefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Type(type_id) => resolver.type_interner.get_mut_by_id(type_id).unwrap(),
            symbol => panic!("Unexpected symbol seen for type: {}", symbol),
        }
    }
}

impl Into<DefPathComponent> for &TypeDefinition {
    fn into(self) -> DefPathComponent {
        DefPathComponent::Type(self.symbol_key().clone())
    }
}

impl<'a> Into<SymbolDef> for DefGenerator<'a, TypeDefinition> {
    fn into(self) -> SymbolDef {
        let type_id = self
            .type_interner
            .insert(self.def_path, self.to_generate_def_for)
            .unwrap();
        SymbolDef::Type(type_id)
    }
}

impl Symbol for TypeDefinition {
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

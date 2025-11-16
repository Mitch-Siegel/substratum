use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

use crate::midend::{symtab::*, types};

pub mod enum_definition;
pub mod struct_definition;

pub use enum_definition::*;
pub use struct_definition::*;

#[derive(Debug, Clone, Serialize)]
pub struct TypeDefinition {
    type_: types::Syntactic,
    generic_params: types::GenericParamsList,
    pub repr: TypeRepr,
}

impl TypeDefinition {
    pub fn new(
        type_: types::Syntactic,
        generic_params: Vec<types::type_interner::GenericParam>,
        repr: TypeRepr,
    ) -> Self {
        TypeDefinition {
            type_,
            generic_params,
            repr,
        }
    }

    pub fn generic_params(&self) -> &types::GenericParamsList {
        &self.generic_params
    }

    pub fn syntactic(&self) -> &types::Syntactic {
        &self.type_
    }
}

impl<'a> From<DefResolver<'a>> for &'a TypeDefinition {
    #[tracing::instrument(skip(resolver))]
    fn from(resolver: DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Type(type_id) => {
                resolver.type_interner.get_type_definition(type_id).unwrap()
            }
            symbol => panic!("Unexpected symbol seen for type: {}", symbol),
        }
    }
}
impl<'a> From<MutDefResolver<'a>> for &'a mut TypeDefinition {
    fn from(_resolver: MutDefResolver<'a>) -> Self {
        panic!("type definitions may not be mutated");
        /*match resolver.to_resolve {
            SymbolDef::Type(type_id) => resolver
                .type_interner
                .get_type_definition_mut(type_id)
                .unwrap(),
            symbol => panic!("Unexpected symbol seen for type: {}", symbol),
        }*/
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
            .insert_type(self.def_path, self.to_generate_def_for)
            .unwrap();
        SymbolDef::Type(type_id)
    }
}

impl Symbol for TypeDefinition {
    type SymbolKey = types::Syntactic;

    fn symbol_key(&self) -> &Self::SymbolKey {
        self.syntactic()
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
    Unit,
    UnsignedInteger(PrimitiveIntegerRepr),
    SignedInteger(PrimitiveIntegerRepr),
    Struct(StructRepr),
    Enum(EnumRepr),
}

impl TypeRepr {
    pub fn name(&self) -> String {
        match self {
            Self::Unit => String::from("()"),
            Self::UnsignedInteger(repr) => format!("u{}", repr.size),
            Self::SignedInteger(repr) => format!("i{}", repr.size),
            Self::Struct(struct_repr) => struct_repr.name.clone(),
            Self::Enum(enum_repr) => enum_repr.name.clone(),
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

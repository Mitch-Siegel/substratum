use serde::Serialize;
use std::fmt::Display;

use super::symtab::{self, ScopedLookups};

use crate::backend;

#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, serde::Deserialize, Hash)]
pub enum Mutability {
    Mutable,
    Immutable,
}
impl From<bool> for Mutability {
    fn from(mutability_bool: bool) -> Self {
        if mutability_bool {
            Self::Mutable
        } else {
            Self::Immutable
        }
    }
}
impl Into<bool> for Mutability {
    fn into(self) -> bool {
        match self {
            Self::Mutable => true,
            Self::Immutable => false,
        }
    }
}

impl Display for Mutability {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Mutability::Mutable => write!(f, "mut"),
            Mutability::Immutable => std::fmt::Result::Ok(()),
        }
    }
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, serde::Deserialize, Hash)]
pub enum Type {
    Unit,
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    _Self,
    UDT(String),
    Reference(Mutability, Box<Type>),
    Pointer(Mutability, Box<Type>),
}

impl Type {
    // size of the type in bytes
    pub fn size<Target: backend::arch::TargetArchitecture>(
        &self,
        scope_stack: &symtab::ScopeStack,
    ) -> usize {
        match self {
            Type::Unit => 0,
            Type::U8 => 1,
            Type::U16 => 2,
            Type::U32 => 4,
            Type::U64 => 8,
            Type::I8 => 1,
            Type::I16 => 2,
            Type::I32 => 4,
            Type::I64 => 8,
            Type::_Self => scope_stack.self_type().size::<Target>(scope_stack),
            Type::UDT(type_name) => {
                let type_definition = scope_stack.lookup_type(self).expect(&format!(
                    "Missing UDT definition for '{}' in Type::size()",
                    type_name
                ));

                match &type_definition.repr {
                    symtab::TypeRepr::Struct(struct_repr) => {
                        let mut struct_size: usize = 0;
                        for (_, field) in struct_repr {
                            struct_size += field.size::<Target>(scope_stack);
                        }

                        struct_size
                    }
                }
            }
            Type::Reference(_, _) => Target::word_size(),
            Type::Pointer(_, _) => Target::word_size(),
        }
    }
}

impl Display for Type {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Unit => write!(f, "()"),
            Self::U8 => write!(f, "u8"),
            Self::U16 => write!(f, "u16"),
            Self::U32 => write!(f, "u32"),
            Self::U64 => write!(f, "u64"),
            Self::I8 => write!(f, "i8"),
            Self::I16 => write!(f, "i16"),
            Self::I32 => write!(f, "i32"),
            Self::I64 => write!(f, "i64"),
            Self::_Self => write!(f, "self"),
            Self::UDT(name) => write!(f, "user-defined type {}", name),
            Self::Reference(mutability, to) => write!(f, "&{} {}", mutability, to),
            Self::Pointer(mutability, to) => write!(f, "*{} {}", mutability, to),
        }
    }
}

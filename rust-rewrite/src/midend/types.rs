use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fmt::Display;

use crate::backend;
use crate::midend::symtab;

#[derive(
    Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, serde::Deserialize, Hash
)]
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

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, serde::Deserialize, Hash)]
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
    GenericParam(String),
    _Self,
    Named(String),
    UserDefined(symtab::TypeId),
    Reference(Mutability, Box<Type>),
    Pointer(Mutability, Box<Type>),
}

impl Type {
    pub fn is_integral<C>(&self, context: &C) -> Result<bool, symtab::UndefinedSymbol>
    {
        match self {
            Type::Unit => Ok(false),
            Type::U8
            | Type::U16
            | Type::U32
            | Type::U64
            | Type::I8
            | Type::I16
            | Type::I32
            | Type::I64 => Ok(true),
            Type::GenericParam(_) => Ok(false),
            Type::_Self => unimplemented!(),
            Type::Named(_) => Ok(false),
            Type::UserDefined(_) => Ok(false),
            Type::Reference(_, _) | Type::Pointer(_, _) => Ok(true),
        }
    }

    // size of the type in bytes
    pub fn size<Target>(&self, interner: &symtab::TypeInterner) -> Result<usize, symtab::UndefinedSymbol>
    where
        Target: backend::arch::TargetArchitecture,
    {
        let size = match self {
            Type::Unit => 0,
            Type::U8 => 1,
            Type::U16 => 2,
            Type::U32 => 4,
            Type::U64 => 8,
            Type::I8 => 1,
            Type::I16 => 2,
            Type::I32 => 4,
            Type::I64 => 8,
            Type::GenericParam(param) => panic!("Can't size generic param {}", param),
            Type::_Self => panic!("Can't size Self type"),
            Type::Named(name) => panic!("Can't size named type {}", name)
            Type::UserDefined(id) => unimplemented!(), /*interner.get_by_id(id).unwrap()*/
            Type::Reference(_, _) => Target::word_size(),
            Type::Pointer(_, _) => Target::word_size(),
        };

        Ok(size)
    }

    pub fn align_size_power_of_two(size: usize) -> usize {
        if size == 0 {
            0
        } else if size == 1 {
            1
        } else {
            size.next_power_of_two()
        }
    }

    pub fn alignment<Target, C>(&self, context: &C) -> Result<usize, symtab::UndefinedSymbol>
    where
        Target: backend::arch::TargetArchitecture,
    {
        unimplemented!();
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
            Self::GenericParam(name) => write!(f, "{}", name),
            Self::_Self => write!(f, "self"),
            Self::UserDefined(typeid) => write!(f, "type{}", typeid),
            Self::Named(name) => write!(f, "user-defined type {}", name),
            Self::Reference(mutability, to) => write!(f, "&{} {}", mutability, to),
            Self::Pointer(mutability, to) => write!(f, "*{} {}", mutability, to),
        }
    }
}

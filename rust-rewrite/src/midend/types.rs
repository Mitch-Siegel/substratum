use serde::Serialize;
use std::fmt::Display;

use crate::backend;
use crate::midend::symtab;

pub trait TypeSizingContext: symtab::TypeOwner + symtab::SelfTypeOwner {}

pub trait SizedType {
    fn size<C>(&self, context: &C) -> Result<usize, symtab::UndefinedSymbol>
    where
        C: TypeSizingContext;

    fn alignment<C>(&self, context: &C) -> Result<usize, symtab::UndefinedSymbol>
    where
        C: TypeSizingContext;
}

#[derive(
    Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, serde::Deserialize, Hash,
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
    _Self,
    UDT(String),
    Reference(Mutability, Box<Type>),
    Pointer(Mutability, Box<Type>),
}

impl Type {
    // size of the type in bytes
    pub fn size<Target, C>(&self, context: &C) -> Result<usize, symtab::UndefinedSymbol>
    where
        Target: backend::arch::TargetArchitecture,
        C: TypeSizingContext,
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
            Type::_Self => context.self_type().size::<Target, C>(context)?,
            Type::UDT(_) => {
                let type_definition = context.lookup_type(self)?;
                type_definition.size(context)?
            }
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
        C: TypeSizingContext,
    {
        match self {
            Type::Unit
            | Type::U8
            | Type::U16
            | Type::U32
            | Type::U64
            | Type::I8
            | Type::I16
            | Type::I32
            | Type::I64
            | Type::Reference(_, _)
            | Type::Pointer(_, _) => Ok(Self::align_size_power_of_two(
                self.size::<Target, C>(context)?,
            )),
            Type::_Self => Ok(context.self_type().alignment::<Target, C>(context)?),
            Type::UDT(_) => {
                let type_definition = context.lookup_type(self)?;
                Ok(type_definition.alignment(context)?)
            }
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

use serde::Serialize;
use std::fmt::Display;

#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, serde::Deserialize)]
pub enum Mutability {
    Mutable,
    Immutable,
}

impl Display for Mutability {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Mutability::Mutable => write!(f, "mut"),
            Mutability::Immutable => std::fmt::Result::Ok(()),
        }
    }
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, serde::Deserialize)]
pub enum Type {
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    Struct(String),
    Reference(Mutability, Box<Type>),
    Pointer(Mutability, Box<Type>),
}

impl Display for Type {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::U8 => write!(f, "u8"),
            Self::U16 => write!(f, "u16"),
            Self::U32 => write!(f, "u32"),
            Self::U64 => write!(f, "u64"),
            Self::I8 => write!(f, "i8"),
            Self::I16 => write!(f, "i16"),
            Self::I32 => write!(f, "i32"),
            Self::I64 => write!(f, "i64"),
            Self::Struct(name) => write!(f, "struct {}", name),
            Self::Reference(mutability, to) => write!(f, "&{} {}", mutability, to),
            Self::Pointer(mutability, to) => write!(f, "*{} {}", mutability, to),
        }
    }
}

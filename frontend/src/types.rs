use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, Deserialize, Hash)]
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

impl fmt::Display for Mutability {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Mutable => write!(f, "mut"),
            Self::Immutable => std::fmt::Result::Ok(()),
        }
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, Deserialize, Hash)]
pub enum Syntactic {
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
    Reference(Mutability, Box<Self>),
    Pointer(Mutability, Box<Self>),
    Tuple(Vec<Option<Self>>),
    Function { args: Vec<Self>, ret_ty: Box<Self> }, // (arguments, return_type)
}

impl fmt::Display for Syntactic {
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
            Self::GenericParam(name) | Self::Named(name) => write!(f, "{name}"),
            Self::_Self => write!(f, "self"),
            Self::Reference(mutability, to) => write!(f, "&{mutability} {to}"),
            Self::Pointer(mutability, to) => write!(f, "*{mutability} {to}"),
            Self::Tuple(elements) => {
                write!(f, "(")?;
                for element in elements {
                    match element {
                        Some(ty) => write!(f, "{ty}, ")?,
                        None => write!(f, "_, ")?,
                    }
                }
                write!(f, ")")
            }
            Self::Function { args, ret_ty } => {
                write!(f, "fn(")?;
                for arg in args {
                    write!(f, "{arg}, ")?;
                }
                write!(f, ") -> {ret_ty}")
            }
        }
    }
}

use serde::{Deserialize, Serialize};

use crate::midend::types::*;

impl Into<UnresolvedType> for PrimitiveType {
    fn into(self) -> UnresolvedType {
        UnresolvedType::Primitive(self)
    }
}

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, serde::Deserialize, Hash)]
pub enum UnresolvedType {
    Primitive(PrimitiveType),
    Named(String),
    Generic(String),
    SelfType,
    Reference(Mutability, Box<UnresolvedType>),
    Pointer(Mutability, Box<UnresolvedType>),
}

impl std::fmt::Display for UnresolvedType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Primitive(primitive_type) => write!(f, "{}", primitive_type),
            Self::Named(name) => write!(f, "{}", name),
            Self::Generic(param) => write!(f, "{}", param),
            Self::SelfType => write!(f, "Self"),
            Self::Reference(mutability, reference_to) => {
                if (*mutability).into() {
                    write!(f, "&mut {}", reference_to)
                } else {
                    write!(f, "& {}", reference_to)
                }
            }
            Self::Pointer(mutability, pointer_to) => {
                if (*mutability).into() {
                    write!(f, "*mut {}", pointer_to)
                } else {
                    write!(f, "* {}", pointer_to)
                }
            }
        }
    }
}

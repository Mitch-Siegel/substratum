use crate::frontend::ast::*;
use serde::{Deserialize, Serialize};
use std::fmt::Display;

use crate::backend;
use crate::midend::symtab;

pub mod semantic_types;
pub mod syntactic_types;
pub mod type_interner;

pub use semantic_types::Semantic;
pub use syntactic_types::Syntactic;
pub use type_interner::Interner;

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

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum Type {
    Unknown,
    Syntactic(Syntactic),
    Semantic(Semantic),
}

use serde::{Deserialize, Serialize};
use std::fmt::Display;

pub(crate) mod semantic_types;
pub(crate) mod syntactic_types;
pub(crate) mod type_interner;

pub(crate) use semantic_types::Semantic;
pub(crate) use syntactic_types::Syntactic;
pub(crate) use type_interner::{GenericParam, GenericParamsList, Interner, ParamSubst, ParamSubstMap};

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, Deserialize, Hash)]
pub(crate) enum Mutability {
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

impl Display for Mutability {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Mutability::Mutable => write!(f, "mut"),
            Mutability::Immutable => std::fmt::Result::Ok(()),
        }
    }
}

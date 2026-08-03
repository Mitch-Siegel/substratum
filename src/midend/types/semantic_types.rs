use serde::{Deserialize, Serialize};

#[derive(Copy, Clone, Debug, Serialize, Deserialize, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub(crate) enum Semantic {
    Unit,
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    UserDefined { id: usize },
}

impl std::fmt::Display for Semantic {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Unit => write!(f, "()"),
            Self::U8 => write!(f, "U8"),
            Self::U16 => write!(f, "U16"),
            Self::U32 => write!(f, "U32"),
            Self::U64 => write!(f, "U64"),
            Self::I8 => write!(f, "I8"),
            Self::I16 => write!(f, "I16"),
            Self::I32 => write!(f, "I32"),
            Self::I64 => write!(f, "I64"),
            Self::UserDefined { id } => write!(f, "{id}"),
        }
    }
}

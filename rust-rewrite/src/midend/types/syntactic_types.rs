use crate::midend::{types::*, *};

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, serde::Deserialize, Hash)]
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
    Reference(Mutability, Box<Syntactic>),
    Pointer(Mutability, Box<Syntactic>),
    Tuple(Vec<Syntactic>),
}

impl Syntactic {
    pub fn resolve(&self, context: &impl linearizer::DefContext) -> Option<Semantic> {
        let (definition, path) = match context.lookup_with_path::<symtab::TypeDefinition>(self) {
            Ok((def, path)) => (def, path),
            Err(_) => return None,
        };

        context.symtab().types.get_semantic(&path)
    }
}

impl Display for Syntactic {
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
            Self::Named(name) => write!(f, "user-defined type {}", name),
            Self::Reference(mutability, to) => write!(f, "&{} {}", mutability, to),
            Self::Pointer(mutability, to) => write!(f, "*{} {}", mutability, to),
            Self::Tuple(elements) => {
                write!(f, "(")?;
                for element in elements {
                    write!(f, "{}, ", element)?;
                }
                write!(f, ")")
            }
        }
    }
}

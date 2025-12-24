use crate::midend::symtab::*;

pub mod types;
pub mod values;

pub use types::*;
pub use values::*;

#[enum_delegate::implement(Symbol)]
#[derive(Debug)]
pub enum SymbolDef {
    Type(Type),
    Value(Value),
}

impl std::fmt::Display for SymbolDef {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Type(t) => write!(f, "type {}", t),
            Self::Value(v) => write!(f, "value {}", v),
        }
    }
}

#[enum_delegate::register]
pub trait Symbol {
    fn name(&self) -> &str;

    fn path_segment(&self) -> PathSegment;

    fn into_repr(self) -> SymbolDef;
}

use crate::midend::symtab::*;

pub mod types;
pub mod values;

pub use types::{EnumRepr, StructRepr, Type};
pub use values::{Function, Value};

#[enum_delegate::implement(Symbol)]
pub enum SymbolRepr {
    Type(Type),
    Value(Value),
}

impl std::fmt::Display for SymbolRepr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Item(i) => write!(f, "item {}", i),
            Self::Value(v) => write!(f, "value {}", v),
        }
    }
}

#[enum_delegate::register]
pub trait Symbol {
    fn name(&self) -> &str;

    fn path_component(&self) -> DefPathComponent;

    fn into_repr(self) -> SymbolRepr;
}

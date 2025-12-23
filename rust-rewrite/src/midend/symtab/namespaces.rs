use crate::midend::symtab::*;

pub mod types;

pub use types::Type;
pub use values::Value;

pub enum SymbolRepr {
    Type(Type),
    Value(Value),
}

impl std::fmt::Display for Entity {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Item(i) => write!(f, "item {}", i),
        }
    }
}

#[enum_delegate::register]
pub trait Symbol {
    fn name(&self) -> String;

    fn component(&self) -> DefPathComponent;
}

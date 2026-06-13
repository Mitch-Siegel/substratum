use crate::midend::symtab::{Implementation, PathSegment, Type, Value};

#[enum_delegate::implement(Symbol)]
#[derive(Debug)]
pub enum SymbolDef {
    Type(Type),
    Value(Value),
    Impl(Implementation),
}

impl std::fmt::Display for SymbolDef {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Type(t) => write!(f, "type {}", t),
            Self::Value(v) => write!(f, "value {}", v),
            Self::Impl(i) => write!(f, "impl {}", i.id.0),
        }
    }
}

#[enum_delegate::register]
pub trait Symbol {
    fn name(&self) -> &str;

    fn path_segment(&self) -> PathSegment;

    fn into_repr(self) -> SymbolDef;
}

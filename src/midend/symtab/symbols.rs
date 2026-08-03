use crate::midend::symtab::{Implementation, PathSegment, Type, Value};

pub(crate) trait Symbol: Sized
where
    SymbolDef: From<Self>,
{
    fn name(&self) -> &str;

    fn path_segment(&self) -> PathSegment;
}

#[derive(Debug)]
pub(crate) enum SymbolDef {
    Type(Type),
    Value(Value),
    Impl(Implementation),
}

impl Symbol for SymbolDef {
    fn name(&self) -> &str {
        match self {
            Self::Type(t) => t.name(),
            Self::Value(v) => v.name(),
            Self::Impl(i) => i.name(),
        }
    }

    fn path_segment(&self) -> PathSegment {
        match self {
            Self::Type(t) => t.path_segment(),
            Self::Value(v) => v.path_segment(),
            Self::Impl(i) => i.path_segment(),
        }
    }
}

impl std::fmt::Display for SymbolDef {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Type(t) => write!(f, "type {t}"),
            Self::Value(v) => write!(f, "value {v}"),
            Self::Impl(i) => write!(f, "impl {}", i.id.0),
        }
    }
}

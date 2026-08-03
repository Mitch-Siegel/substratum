use crate::midend::symtab::{symbols::SymbolDef, PathSegment, Symbol};

pub(crate) mod binding;
pub(crate) mod function;

pub(crate) use binding::*;
pub(crate) use function::*;

#[derive(Clone, Debug)]
pub(crate) enum Value {
    Function(Box<Function>),
    LocalBinding(LocalBinding),
}

impl Symbol for Value {
    fn name(&self) -> &str {
        match self {
            Self::Function(f) => f.name(),
            Self::LocalBinding(lb) => lb.name(),
        }
    }

    fn path_segment(&self) -> PathSegment {
        match self {
            Self::Function(f) => f.path_segment(),
            Self::LocalBinding(lb) => lb.path_segment(),
        }
    }
}

impl From<Value> for SymbolDef {
    fn from(value: Value) -> Self {
        Self::Value(value)
    }
}

impl From<Function> for Value {
    fn from(value: Function) -> Self {
        Self::Function(Box::new(value))
    }
}

impl std::fmt::Display for Value {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Function(fu) => write!(f, "{fu}"),
            Self::LocalBinding(lb) => write!(f, "{lb}"),
        }
    }
}

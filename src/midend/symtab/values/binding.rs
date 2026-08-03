use crate::midend::symtab::{midend, PathSegment, Symbol, SymbolDef, Value};
use serde::Serialize;
use std::fmt::Display;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Hash)]
pub(crate) struct Variable {
    pub name: String,
    type_: Option<midend::types::Syntactic>,
}

impl Display for Variable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}: {}",
            self.name,
            match &self.type_ {
                Some(type_) => format!("{type_}"),
                None => "?Unknown Type?".into(),
            },
        )
    }
}

impl Variable {
    pub(crate) fn new(name: String, type_: Option<midend::types::Syntactic>) -> Self {
        Self { name, type_ }
    }

    pub(crate) fn type_(&self) -> Option<&midend::types::Syntactic> {
        self.type_.as_ref()
    }

    pub(crate) fn mangle_name_at_index(&mut self, index: usize) {
        self.name = format!("{}_{}", index, self.name);
    }
}

#[derive(Clone, Debug)]
pub(crate) enum LocalBinding {
    FunctionParam(Variable),
    Let(Variable),
}

impl Symbol for LocalBinding {
    fn name(&self) -> &str {
        match self {
            Self::FunctionParam(v) | Self::Let(v) => &v.name,
        }
    }

    fn path_segment(&self) -> PathSegment {
        PathSegment::Value(self.name().into())
    }
}

impl From<LocalBinding> for SymbolDef {
    fn from(value: LocalBinding) -> Self {
        Self::Value(Value::LocalBinding(value))
    }
}

impl std::fmt::Display for LocalBinding {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::FunctionParam(fp) => write!(f, "{fp}"),
            Self::Let(v) => write!(f, "{v}"),
        }
    }
}

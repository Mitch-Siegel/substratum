use crate::midend::{
    symtab::{Symbol, *},
    *,
};
use serde::{Deserialize, Serialize};
use std::fmt::Display;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Hash)]
pub struct Variable {
    pub name: String,
    type_: Option<types::Syntactic>,
}

impl Display for Variable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}: {}",
            self.name,
            match &self.type_ {
                Some(type_) => format!("{}", type_),
                None => "?Unknown Type?".into(),
            },
        )
    }
}

impl Variable {
    pub fn new(name: String, type_: Option<types::Syntactic>) -> Self {
        Variable { name, type_ }
    }

    pub fn type_(&self) -> Option<&types::Syntactic> {
        self.type_.as_ref()
    }

    pub fn mangle_name_at_index(&mut self, index: usize) {
        self.name = String::from(format!("{}_{}", index, self.name));
    }
}

#[derive(Clone, Debug)]
pub enum LocalBinding {
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

    fn into_repr(self) -> SymbolDef {
        SymbolDef::Value(Value::LocalBinding(self))
    }
}

impl std::fmt::Display for LocalBinding {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::FunctionParam(fp) => write!(f, "{}", fp),
            Self::Let(v) => write!(f, "{}", v),
        }
    }
}

use crate::midend::{ir::*, *};

mod value_interner;

pub use value_interner::*;

#[derive(Copy, Clone, Debug, Serialize, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub struct ValueId {
    index: usize,
}

impl Display for ValueId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "%{}", self.index)
    }
}

impl ValueId {
    pub fn new(index: usize) -> Self {
        Self { index }
    }
}
pub enum ValueKind {
    Argument(u32),
    Variable(symtab::DefPath),
    Temporary,
    Constant(usize),
}

pub struct Value {
    pub kind: ValueKind,
    pub type_: Option<symtab::TypeId>,
}

impl Value {
    pub fn new(kind: ValueKind, type_: Option<symtab::TypeId>) -> Self {
        Self { kind, type_ }
    }
}

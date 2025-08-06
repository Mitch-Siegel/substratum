use crate::midend::{ir::*, *};

mod value_interner;
pub use value_interner::ValueInterner;

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

#[derive(Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub enum ValueKind {
    Argument(usize),
    Variable(symtab::DefPath),
    Temporary(usize),
    Constant(usize),
}

#[derive(Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub struct Value {
    pub kind: ValueKind,
    pub type_: Option<types::Semantic>,
}

impl Value {
    pub fn new(kind: ValueKind, type_: Option<types::Semantic>) -> Self {
        Self { kind, type_ }
    }
}

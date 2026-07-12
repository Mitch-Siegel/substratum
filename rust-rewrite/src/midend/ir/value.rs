use crate::midend::{ir::*, *};

mod value_interner;
pub(crate) use value_interner::{ValueError, ValueInterner};

#[derive(Copy, Clone, Debug, Serialize, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub(crate) struct ValueId {
    index: usize,
}

impl Display for ValueId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "%{}", self.index)
    }
}

impl ValueId {
    pub(crate) fn new(index: usize) -> Self {
        Self { index }
    }
}

#[allow(dead_code)]
#[derive(Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub(crate) enum ValueKind {
    Argument(usize),
    Variable(symtab::ValuePath),
    Temporary(usize),
    StaticFunction(symtab::ValuePath),
    Constant(usize),
}

#[derive(Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub(crate) struct Value {
    kind: ValueKind,
    ty: Option<types::Semantic>,
}

impl Value {
    pub(crate) fn new(kind: ValueKind, type_: Option<types::Semantic>) -> Self {
        Self { kind, ty: type_ }
    }

    pub(crate) fn set_type(&mut self, ty: types::Semantic) -> Result<(), ValueError> {
        match self.ty.replace(ty) {
            Some(existing_type) => Err(ValueError::ValueAlreadyHasType(existing_type)),
            None => Ok(()),
        }
    }

    pub(crate) fn ty(&self) -> Result<types::Semantic, ValueError> {
        match self.ty {
            Some(t) => Ok(t),
            None => Err(ValueError::ValueHasNoType),
        }
    }
}

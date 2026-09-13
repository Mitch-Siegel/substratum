use std::{fmt, hash};

use crate::{ir::Serialize, symtab, types};

mod value_interner;
pub(crate) use value_interner::{ValueError, ValueInterner};

#[derive(Copy, Clone, Debug, Serialize)]
pub(crate) struct ValueId {
    index: usize,
    #[cfg(feature = "value_locs")]
    loc: frontend::sourceloc::StaticSourceLoc,
}

impl PartialEq for ValueId {
    fn eq(&self, other: &Self) -> bool {
        self.index.eq(&other.index)
    }
}

impl Eq for ValueId{}

impl PartialOrd for ValueId {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for ValueId {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.index.cmp(&other.index)
    }
}

impl hash::Hash for ValueId {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.index.hash(state);
    }
}



impl fmt::Display for ValueId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "%{}", self.index)
    }
}

impl ValueId {
    pub(crate) fn new(
        index: usize,
        #[cfg(feature = "value_locs")] loc: frontend::sourceloc::StaticSourceLoc,
    ) -> Self {
        Self {
            index,
            #[cfg(feature = "value_locs")]
            loc,
        }
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

impl fmt::Display for ValueKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Argument(idx) => write!(f, "arg {idx}"),
            Self::Variable(p) => write!(f, "{p}"),
            Self::Temporary(t) => write!(f, "temp {t}"),
            Self::StaticFunction(sf) => write!(f, "{sf}()"),
            Self::Constant(val) => write!(f, "{val}"),
        }
    }
}

#[derive(Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub(crate) struct Value<T> {
    kind: ValueKind,
    ty: T,
}

impl<T> Value<T> {
    pub(crate) fn new(kind: ValueKind, ty: T) -> Self {
        Self { kind, ty }
    }

    pub(crate) fn kind(&self) -> &ValueKind {
        &self.kind
    }

    pub(crate) fn ty(&self) -> &T {
        &self.ty
    }

    #[allow(unused)]
    pub(crate) fn ty_mut(&mut self) -> &mut T {
        &mut self.ty
    }
}

impl<T> fmt::Display for Value<T>
where
    T: fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: {}", self.kind, self.ty)
    }
}

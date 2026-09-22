use std::{fmt, hash};

use crate::{ir::Serialize, symtab, types};

mod value_interner;
pub(crate) use value_interner::{ValueError, ValueInterner};

#[derive(Copy, Clone, Debug, Serialize, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct ValueId {
    index: usize,
}

impl fmt::Display for ValueId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
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

#[derive(Clone, Debug)]
pub(crate) struct Value<T>
where
    T: Eq + Ord + hash::Hash,
{
    kind: ValueKind,
    ty: T,
    #[cfg(feature = "value_locs")]
    #[allow(unused)]
    loc: frontend::sourceloc::StaticSourceLoc,
}

impl<T> Value<T>
where
    T: Eq + Ord + hash::Hash,
{
    pub(crate) fn new(
        kind: ValueKind,
        ty: T,
        #[cfg(feature = "value_locs")] loc: frontend::sourceloc::StaticSourceLoc,
    ) -> Self {
        Self { kind, ty, loc }
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

impl<T> PartialEq for Value<T>
where
    T: Eq + Ord + hash::Hash,
{
    fn eq(&self, other: &Self) -> bool {
        self.kind.eq(&other.kind) && self.ty.eq(&other.ty)
    }
}

impl<T> Eq for Value<T> where T: Eq + Ord + hash::Hash {}

impl<T> PartialOrd for Value<T>
where
    T: Eq + Ord + hash::Hash,
{
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for Value<T>
where
    T: Eq + Ord + hash::Hash,
{
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.kind.cmp(&other.kind).then(self.ty.cmp(&other.ty))
    }
}

impl<T> hash::Hash for Value<T>
where
    T: Eq + Ord + hash::Hash,
{
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.kind.hash(state);
        self.ty.hash(state);
    }
}

impl<T> fmt::Display for Value<T>
where
    T: Eq + Ord + hash::Hash + fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: {}", self.kind, self.ty)
    }
}

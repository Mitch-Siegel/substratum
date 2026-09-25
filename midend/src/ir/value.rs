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
pub(crate) struct Value<'a, T>
where
    T: Eq + Ord + hash::Hash,
{
    pub kind: &'a ValueKind,
    pub ty: &'a T,
    #[cfg(feature = "value_locs")]
    #[allow(unused)]
    pub loc: &'a frontend::sourceloc::StaticSourceLoc,
}

impl<'a, T> Value<'a, T>
where
    T: Eq + Ord + hash::Hash,
{
    pub(crate) fn new(
        kind: &'a ValueKind,
        ty: &'a T,
        #[cfg(feature = "value_locs")] loc: &'a frontend::sourceloc::StaticSourceLoc,
    ) -> Self {
        Self { kind, ty, loc }
    }
}

impl<'a, T> TryFrom<&'a value_interner::SsaValue<T>> for Value<'a, T>
where
    T: Eq + Ord + hash::Hash,
{
    type Error = &'static str;

    fn try_from(value: &'a value_interner::SsaValue<T>) -> Result<Self, Self::Error> {
        let kind: &ValueKind = match &value.kind {
            value_interner::SsaValueKind::Base(b) => Ok(b),
            value_interner::SsaValueKind::Instance { .. } => Err("value is an ssa instance"),
        }?;

        Ok(Self::new(
            kind,
            &value.ty,
            #[cfg(feature = "value_locs")]
            &value.loc,
        ))
    }
}

impl<T> PartialEq for Value<'_, T>
where
    T: Eq + Ord + hash::Hash,
{
    fn eq(&self, other: &Self) -> bool {
        self.kind.eq(other.kind) && self.ty.eq(other.ty)
    }
}

impl<T> Eq for Value<'_, T> where T: Eq + Ord + hash::Hash {}

impl<T> PartialOrd for Value<'_, T>
where
    T: Eq + Ord + hash::Hash,
{
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for Value<'_, T>
where
    T: Eq + Ord + hash::Hash,
{
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.kind.cmp(other.kind).then(self.ty.cmp(other.ty))
    }
}

impl<T> hash::Hash for Value<'_, T>
where
    T: Eq + Ord + hash::Hash,
{
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.kind.hash(state);
        self.ty.hash(state);
    }
}

impl<T> fmt::Display for Value<'_, T>
where
    T: Eq + Ord + hash::Hash + fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: {}", self.kind, self.ty)
    }
}

#[derive(Debug)]
pub(crate) struct ValueMut<'a, T>
where
    T: Eq + Ord + hash::Hash,
{
    #[allow(unused)]
    pub kind: &'a ValueKind,
    pub ty: &'a mut T,
    #[cfg(feature = "value_locs")]
    #[allow(unused)]
    pub loc: &'a frontend::sourceloc::StaticSourceLoc,
}

impl<'a, T> TryFrom<&'a mut value_interner::SsaValue<T>> for ValueMut<'a, T>
where
    T: Eq + Ord + hash::Hash,
{
    type Error = &'static str;

    fn try_from(value: &'a mut value_interner::SsaValue<T>) -> Result<Self, Self::Error> {
        let kind: &ValueKind = match &value.kind {
            value_interner::SsaValueKind::Base(b) => Ok(b),
            value_interner::SsaValueKind::Instance { .. } => Err("value is an ssa instance"),
        }?;

        Ok(Self {
            kind,
            ty: &mut value.ty,
            #[cfg(feature = "value_locs")]
            loc: &value.loc,
        })
    }
}

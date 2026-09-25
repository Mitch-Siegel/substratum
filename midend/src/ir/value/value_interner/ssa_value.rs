use std::{fmt, hash};

use crate::ir::value::{ValueInterner, ValueId, ValueKind};

#[derive(Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Hash)]
pub(crate) enum SsaValueKind {
    Base(ValueKind),
    Instance { base_id: ValueId, instance: usize },
}

impl From<ValueKind> for SsaValueKind {
    fn from(value: ValueKind) -> Self {
        Self::Base(value)
    }
}

impl fmt::Display for SsaValueKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Base(k) => write!(f, "{k}"),
            Self::Instance { base_id, instance } => write!(f, "{base_id}.{instance}"),
        }
    }
}

#[derive(Clone, Debug)]
pub(crate) struct SsaValue<T> {
    pub kind: SsaValueKind,
    pub ty: T,
    #[cfg(feature = "value_locs")]
    #[allow(unused)]
    pub loc: frontend::sourceloc::StaticSourceLoc,
}

impl<T> SsaValue<T> {
    pub(crate) fn new(
        kind: SsaValueKind,
        ty: T,

        #[cfg(feature = "value_locs")] loc: frontend::sourceloc::StaticSourceLoc,
    ) -> Self {
        Self { kind, ty, loc }
    }

    pub(crate) fn pretty_print(&self, values: &ValueInterner<T>) -> String 
    where T: Clone + Eq + Ord + hash::Hash{
        match &self.kind {
            SsaValueKind::Base(ValueKind::Argument(idx)) => format!("arg{idx}"),
            SsaValueKind::Base(ValueKind::Variable(path)) => format!("{path}"),
            SsaValueKind::Base(ValueKind::Temporary(idx)) => format!("t{idx}"),
            SsaValueKind::Base(ValueKind::StaticFunction(path)) => format!("{path}()"),
            SsaValueKind::Base(ValueKind::Constant(value)) => format!("{value}"),
            SsaValueKind::Instance { base_id, instance } => {
                format!("{}.{}", values.ssa_value_for_id(*base_id).unwrap().pretty_print(values), instance)
            }
        }
    }
}

impl<T> PartialEq for SsaValue<T>
where
    T: Eq + Ord + hash::Hash,
{
    fn eq(&self, other: &Self) -> bool {
        self.kind.eq(&other.kind) && self.ty.eq(&other.ty)
    }
}

impl<T> Eq for SsaValue<T> where T: Eq + Ord + hash::Hash {}

impl<T> PartialOrd for SsaValue<T>
where
    T: Eq + Ord + hash::Hash,
{
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for SsaValue<T>
where
    T: Eq + Ord + hash::Hash,
{
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.kind.cmp(&other.kind).then(self.ty.cmp(&other.ty))
    }
}

impl<T> hash::Hash for SsaValue<T>
where
    T: Eq + Ord + hash::Hash,
{
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.kind.hash(state);
        self.ty.hash(state);
    }
}

impl<T> fmt::Display for SsaValue<T>
where
    T: Eq + Ord + hash::Hash + fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: {}", self.kind, self.ty)
    }
}

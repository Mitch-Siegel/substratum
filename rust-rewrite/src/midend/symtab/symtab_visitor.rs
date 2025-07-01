use crate::midend::{symtab::*, types};

pub struct MutSymtabVisitor<C> {
    data: C,
}

impl<C> MutSymtabVisitor<C> {
    pub fn new(data: C) -> Self {
        Self { data }
    }
}

pub struct SymtabVisitor<'a, C> {
    data: &'a C,
}

impl<'a, C> SymtabVisitor<'a, C> {
    pub fn new(data: &'a C) -> Self {
        Self { data }
    }
}

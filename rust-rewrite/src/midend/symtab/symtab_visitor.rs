use crate::midend::{symtab::*, *};

pub struct MutBasicBlockVisitor<C> {
    data: C,
    on_block: fn(&mut ir::BasicBlock, &mut C),
}

impl<C> MutBasicBlockVisitor<C> {}

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

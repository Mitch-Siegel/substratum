use crate::midend::symtab::*;

pub struct SymtabWalker<'a> {
    symtab: &'a SymbolTable,
}

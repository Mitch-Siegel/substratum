use crate::midend::{symtab::Symtab, treewalk::*};

use std::collections::HashSet;

pub struct UnpathedCollectCtx {
    symtab: symtab::SymbolTable,
}

impl UnpathedCollectCtx {
    pub fn new(symtab: symtab::SymbolTable) -> Self {
        Self { symtab }
    }

    pub fn take(self) -> symtab::SymbolTable {
        self.symtab
    }
}

impl PathableContext for UnpathedCollectCtx {}

impl Symtab for UnpathedCollectCtx {
    fn insert(
        &mut self,
        path: symtab::DefPath,
        maybe_symbol: Option<symtab::SymbolDef>,
    ) -> Result<symtab::DefPath, symtab::SymbolError> {
        self.symtab.insert(path, maybe_symbol)
    }

    fn lookup_at(
        &self,
        path: &symtab::DefPath,
    ) -> Result<Option<&symtab::SymbolDef>, symtab::SymbolError> {
        self.symtab.lookup_at(path)
    }

    fn get_impls_for(
        &self,
        path: &symtab::DefPath,
    ) -> Result<&HashSet<symtab::DefPath>, symtab::SymbolError> {
        self.symtab.get_impls_for(path)
    }

    fn create_impl(
        &mut self,
        impl_parent_path: symtab::DefPath,
        impl_for_path: symtab::DefPath,
    ) -> Result<symtab::DefPath, symtab::SymbolError> {
        self.symtab.create_impl(impl_parent_path, impl_for_path)
    }
}

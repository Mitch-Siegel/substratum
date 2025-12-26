use crate::midend::{symtab::Symtab, treewalk::*};

pub struct UnpathedCollectCtx {
    symtab: Box<symtab::SymbolTable>,
}

impl UnpathedCollectCtx {
    pub fn new(symtab: Box<symtab::SymbolTable>) -> Self {
        Self { symtab }
    }

    pub fn take(self) -> Box<symtab::SymbolTable> {
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

    fn lookup(
        &self,
        search_path: symtab::DefPath,
        lookup_path: symtab::DefPath,
    ) -> Result<(&symtab::SymbolDef, symtab::DefPath), symtab::SymbolError> {
        self.symtab.lookup(search_path, lookup_path)
    }
}

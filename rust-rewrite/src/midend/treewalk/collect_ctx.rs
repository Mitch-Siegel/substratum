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

    fn lookup_at(
        &self,
        path: &symtab::DefPath,
    ) -> Result<Option<&symtab::SymbolDef>, symtab::SymbolError> {
        self.symtab.lookup_at(path)
    }
}

use crate::midend::{symtab::Symtab, treewalk::*};

pub struct CollectCtx {
    symtab: Box<symtab::SymbolTable>,
    cur_path: symtab::DefPath,
}

impl CollectCtx {
    pub fn new(symtab: Box<symtab::SymbolTable>, cur_path: symtab::DefPath) -> Self {
        Self { symtab, cur_path }
    }

    pub fn take(self) -> Box<symtab::SymbolTable> {
        self.symtab
    }

    pub fn def_path(&self) -> &symtab::DefPath {
        &self.cur_path
    }

    pub fn with_path(self, path: symtab::DefPath) -> (Self, symtab::DefPath) {
        let new_self = Self {
            symtab: self.symtab,
            cur_path: path,
        };

        (new_self, self.cur_path)
    }

    pub fn declare_value(&mut self, name: String) -> Result<symtab::DefPath, symtab::SymbolError> {
        self.declare(
            self.cur_path
                .clone()
                .with_segment(symtab::PathSegment::Value(name))
                .unwrap(),
        )
    }

    pub fn declare_type(&mut self, name: String) -> Result<symtab::DefPath, symtab::SymbolError> {
        self.declare(
            self.cur_path
                .clone()
                .with_segment(symtab::PathSegment::Type(name))
                .unwrap(),
        )
    }
}

impl Symtab for CollectCtx {
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

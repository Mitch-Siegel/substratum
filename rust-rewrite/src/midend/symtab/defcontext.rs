use crate::midend::{
    symtab::{self, SymbolTable},
    types,
};

pub struct BasicDefContext<'a> {
    symtab: &'a mut SymbolTable,
    definition_path: symtab::DefPath,
}

impl<'a> std::fmt::Debug for BasicDefContext<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "DefContext @ {}", self.definition_path)
    }
}

impl<'a> BasicDefContext<'a> {
    pub fn new(symtab: &'a mut SymbolTable) -> Self {
        Self {
            symtab,
            definition_path: symtab::DefPath::new(),
        }
    }

    pub fn with_child_def_path(&'a mut self, child_def_path: symtab::DefPath) -> Self {
        Self {
            symtab: self.symtab,
            definition_path: self.definition_path.clone_with_join(child_def_path),
        }
    }
}

pub trait DefContext {
    fn symtab(&self) -> &symtab::SymbolTable;
    fn symtab_mut(&mut self) -> &mut symtab::SymbolTable;
    fn definition_path(&self) -> symtab::DefPath;
}

impl<'a> DefContext for BasicDefContext<'a> {
    fn symtab(&self) -> &symtab::SymbolTable {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut symtab::SymbolTable {
        &mut self.symtab
    }

    fn definition_path(&self) -> symtab::DefPath {
        self.definition_path.clone()
    }
}

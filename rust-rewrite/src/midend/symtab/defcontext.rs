use crate::midend::{symtab::*, types};

pub struct BasicDefContext<'a> {
    symtab: &'a mut SymbolTable,
    definition_path: DefPath,
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
            definition_path: DefPath::new(),
        }
    }

    pub fn with_child_def_path(&'a mut self, child_def_path: DefPath) -> Self {
        Self {
            symtab: self.symtab,
            definition_path: self.definition_path.clone_with_join(child_def_path),
        }
    }
}

pub trait DefContext {
    fn symtab(&self) -> &SymbolTable;
    fn symtab_mut(&mut self) -> &mut SymbolTable;
    fn def_path(&self) -> DefPath;

    fn lookup<'a, S>(&'a self, key: &<S as Symbol<'a>>::SymbolKey) -> Result<&'a S, SymbolError>
    where
        S: Symbol<'a>,
        &'a S: From<DefinitionResolver<'a>>,
        SymbolDefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup::<S>(self.def_path(), key)
    }
}

impl<'a> DefContext for BasicDefContext<'a> {
    fn symtab(&self) -> &SymbolTable {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut SymbolTable {
        &mut self.symtab
    }

    fn def_path(&self) -> DefPath {
        self.definition_path.clone()
    }
}

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
            definition_path: DefPath::empty(),
        }
    }
}

pub trait DefContext {
    fn symtab(&self) -> &SymbolTable;
    fn symtab_mut(&mut self) -> &mut SymbolTable;
    fn def_path(&self) -> DefPath;
    fn def_path_mut(&mut self) -> &mut DefPath;

    fn lookup<S>(&self, key: &<S as Symbol>::SymbolKey) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup::<S>(&self.def_path(), key)
    }

    fn lookup_mut<S>(&mut self, key: &<S as Symbol>::SymbolKey) -> Result<&mut S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let def_path = self.def_path();
        self.symtab_mut().lookup_mut::<S>(&def_path, key)
    }

    fn lookup_at<S>(
        &self,
        def_path: &DefPath,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup_at::<S>(def_path, key)
    }

    // add a DefPathComponent for 'symbol' at the end of the current def path
    fn insert<S>(&mut self, symbol: S) -> Result<DefPath, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let def_path = self.def_path();
        let symtab_mut = self.symtab_mut();
        symtab_mut.insert::<S>(def_path, symbol)
    }

    fn insert_at<S>(&mut self, def_path: DefPath, symbol: S) -> Result<DefPath, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab_mut().insert::<S>(def_path, symbol)
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

    fn def_path_mut(&mut self) -> &mut DefPath {
        &mut self.definition_path
    }
}

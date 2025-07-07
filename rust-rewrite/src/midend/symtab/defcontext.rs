use crate::midend::{symtab::*, types};

pub struct BasicDefContext<'s> {
    symtab: &'s mut SymbolTable,
    definition_path: DefPath,
}

impl<'s> std::fmt::Debug for BasicDefContext<'s> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "DefContext @ {}", self.definition_path)
    }
}

impl<'s> BasicDefContext<'s> {
    pub fn new(symtab: &'s mut SymbolTable) -> Self {
        Self {
            symtab,
            definition_path: DefPath::empty(),
        }
    }
}

pub trait DefContext<'b, 's>: std::fmt::Debug
where
    's: 'b,
{
    fn symtab(&'b self) -> &'s SymbolTable;
    fn symtab_mut(&'b mut self) -> &'s mut SymbolTable;
    fn def_path(&self) -> DefPath;
    fn def_path_mut(&mut self) -> &mut DefPath;

    fn id_for_type(&'b self, type_: &types::Type) -> Result<TypeId, SymbolError> {
        self.symtab().id_for_type(&self.def_path(), type_)
    }

    fn type_for_id(&'b self, id: &TypeId) -> Option<&TypeDefinition> {
        self.symtab().type_for_id(id)
    }

    fn lookup<S>(&'b self, key: &<S as Symbol>::SymbolKey) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup::<S>(&self.def_path(), key)
    }

    fn lookup_with_path<S>(
        &'b self,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<(&'s S, DefPath), SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup_with_path::<S>(&self.def_path(), key)
    }

    fn lookup_mut<S>(&'b mut self, key: &<S as Symbol>::SymbolKey) -> Result<&'s mut S, SymbolError>
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
        &'b self,
        def_path: &DefPath,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<&'s S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup_at::<S>(def_path, key)
    }

    fn lookup_at_mut<S>(
        &'b mut self,
        def_path: &DefPath,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<&'s mut S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab_mut().lookup_at_mut::<S>(def_path, key)
    }

    // add a DefPathComponent for 'symbol' at the end of the current def path
    fn insert<S>(&'b mut self, symbol: S) -> Result<DefPath, SymbolError>
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

    fn insert_at<S>(&'b mut self, def_path: DefPath, symbol: S) -> Result<DefPath, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab_mut().insert::<S>(def_path, symbol)
    }
}

impl<'a, 's> DefContext<'a, 's> for BasicDefContext<'s>
where
    's: 'a,
    'a: 's,
{
    fn symtab(&'a self) -> &'s SymbolTable {
        &self.symtab
    }

    fn symtab_mut(&'a mut self) -> &'s mut SymbolTable {
        &mut self.symtab
    }

    fn def_path(&self) -> DefPath {
        self.definition_path.clone()
    }

    fn def_path_mut(&mut self) -> &mut DefPath {
        &mut self.definition_path
    }
}

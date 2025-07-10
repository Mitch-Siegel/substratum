use crate::midend::{symtab::*, types};

pub struct BasicDefContext {
    symtab: Box<SymbolTable>,
    definition_path: DefPath,
}

impl std::fmt::Debug for BasicDefContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "DefContext @ {}", self.definition_path)
    }
}

impl BasicDefContext {
    pub fn new(symtab: Box<SymbolTable>) -> Self {
        Self {
            symtab,
            definition_path: DefPath::empty(),
        }
    }

    pub fn with_path(symtab: Box<SymbolTable>, definition_path: DefPath) -> Self {
        Self {
            symtab,
            definition_path,
        }
    }
}

pub trait DefContext: std::fmt::Debug {
    fn symtab(&self) -> &SymbolTable;
    fn symtab_mut(&mut self) -> &mut SymbolTable;
    fn def_path(&self) -> DefPath;
    fn def_path_mut(&mut self) -> &mut DefPath;

    fn id_for_type(&self, type_: &types::Type) -> Result<TypeId, SymbolError> {
        self.symtab().id_for_type(&self.def_path(), type_)
    }

    fn type_for_id(&self, id: &TypeId) -> Option<&TypeDefinition> {
        self.symtab().type_for_id(id)
    }

    fn lookup<S>(&self, key: &<S as Symbol>::SymbolKey) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup::<S>(&self.def_path(), key)
    }

    fn lookup_with_path<S>(
        &self,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<(&S, DefPath), SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup_with_path::<S>(&self.def_path(), key)
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

    fn lookup_at_mut<S>(
        &mut self,
        def_path: &DefPath,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<&mut S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab_mut().lookup_at_mut::<S>(def_path, key)
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

    fn lookup_implemented_function(
        &self,
        receiver_type: &types::Type,
        name: &str,
    ) -> Result<&Function, SymbolError> {
        let (_, receiver_type_definition_path) =
            self.lookup_with_path::<TypeDefinition>(receiver_type)?;

        self.lookup_at::<Function>(
            &receiver_type_definition_path,
            &FunctionName { name: name.into() },
        )
    }

    fn take(self) -> Result<(Box<SymbolTable>, DefPath), ()>;
}

impl DefContext for BasicDefContext {
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

    fn take(self) -> Result<(Box<SymbolTable>, DefPath), ()> {
        Ok((self.symtab, self.definition_path))
    }
}

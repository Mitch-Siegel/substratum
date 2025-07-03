use crate::midend::{symtab::*, types};

pub struct BasicDefContext<'a> {
    symtab: &'a mut SymbolTable<'a>,
    definition_path: DefPath<'a>,
}

impl<'a> std::fmt::Debug for BasicDefContext<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "DefContext @ {}", self.definition_path)
    }
}

impl<'a> BasicDefContext<'a> {
    pub fn new(symtab: &'a mut SymbolTable<'a>) -> Self {
        Self {
            symtab,
            definition_path: DefPath::new(),
        }
    }
}

pub trait DefContext<'a> {
    fn symtab(&self) -> &SymbolTable<'a>;
    fn symtab_mut(&mut self) -> &mut SymbolTable<'a>;
    fn def_path(&self) -> DefPath<'a>;

    fn lookup<S>(&'a self, key: &<S as Symbol<'a>>::SymbolKey) -> Result<&'a S, SymbolError>
    where
        S: Symbol<'a>,
        &'a S: From<DefResolver<'a>>,
        DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup::<S>(self.def_path(), key)
    }

    fn insert<S>(&'a mut self, symbol: S) -> Result<(), SymbolError>
    where
        S: Symbol<'a>,
        &'a S: From<DefResolver<'a>>,
        DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let def_path = self.def_path();
        let symtab_mut = self.symtab_mut();
        symtab_mut.insert::<S>(def_path, symbol)
    }
}

impl<'a> DefContext<'a> for BasicDefContext<'a> {
    fn symtab(&self) -> &SymbolTable<'a> {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut SymbolTable<'a> {
        &mut self.symtab
    }

    fn def_path(&self) -> DefPath<'a> {
        self.definition_path.clone()
    }
}

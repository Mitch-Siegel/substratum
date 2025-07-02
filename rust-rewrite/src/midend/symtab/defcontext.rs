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
    fn symtab(&'a self) -> &'a SymbolTable<'a>;
    fn symtab_mut(&'a mut self) -> &'a mut SymbolTable<'a>;
    fn def_path(&'a self) -> DefPath<'a>;

    fn lookup<S>(&'a self, key: &<S as Symbol<'a>>::SymbolKey) -> Result<&'a S, SymbolError>
    where
        S: Symbol<'a>,
        &'a S: From<DefResolver<'a>>,
        DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup::<S>(self.def_path(), key)
    }
}

impl<'a, 'b> DefContext<'b> for BasicDefContext<'a>
where
    'b: 'a,
{
    fn symtab(&'b self) -> &'b SymbolTable<'b> {
        &self.symtab
    }

    fn symtab_mut(&'b mut self) -> &'b mut SymbolTable<'b> {
        &mut self.symtab
    }

    fn def_path(&'b self) -> DefPath<'b> {
        self.definition_path.clone()
    }
}

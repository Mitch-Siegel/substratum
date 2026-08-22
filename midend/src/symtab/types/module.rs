use crate::symtab::{PathSegment, Symbol, SymbolDef, Type};

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub(crate) struct Module {
    pub name: String,
}

impl Module {
    pub(crate) fn new(name: String) -> Self {
        assert!(!name.is_empty());
        Self { name }
    }
}

impl Symbol for Module {
    fn name(&self) -> &str {
        &self.name
    }

    fn path_segment(&self) -> PathSegment {
        PathSegment::Type(self.name.clone())
    }
}

impl From<Module> for SymbolDef {
    fn from(value: Module) -> Self {
        Self::from(Type::Module(value))
    }
}

impl std::fmt::Display for Module {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

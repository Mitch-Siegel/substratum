use crate::midend::symtab::*;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Module {
    pub name: String,
}

impl Module {
    pub fn new(name: String) -> Self {
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

    fn into_repr(self) -> SymbolDef {
        SymbolDef::Type(Type::Module(self))
    }
}

impl std::fmt::Display for Module {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

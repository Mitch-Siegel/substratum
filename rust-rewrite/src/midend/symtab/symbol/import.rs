use crate::midend::symtab::*;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ImportName {
    pub name: String,
}
impl std::fmt::Display for ImportName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Import {
    name: ImportName,
    pub qualified_path: DefPath,
}

impl Import {
    pub fn new(name: String, qualified_path: DefPath) -> Self {
        Self {
            name: ImportName { name },
            qualified_path,
        }
    }
}

impl<'a> From<DefResolver<'a>> for &'a Import {
    fn from(resolver: DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Import(import) => import,
            symbol => panic!("Unexpected symbol seen for import: {}", symbol),
        }
    }
}
impl<'a> From<MutDefResolver<'a>> for &'a mut Import {
    fn from(resolver: MutDefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Import(import) => import,
            symbol => panic!("Unexpected symbol seen for import: {}", symbol),
        }
    }
}

impl Into<DefPathComponent> for &Import {
    fn into(self) -> DefPathComponent {
        DefPathComponent::Import(self.symbol_key().clone())
    }
}
impl<'a> Into<SymbolDef> for DefGenerator<'a, Import> {
    fn into(self) -> SymbolDef {
        SymbolDef::Import(self.to_generate_def_for)
    }
}
impl Symbol for Import {
    type SymbolKey = ImportName;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.name
    }
}
impl std::fmt::Display for Import {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

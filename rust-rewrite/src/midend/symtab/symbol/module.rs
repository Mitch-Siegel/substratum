use crate::midend::symtab::*;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ModuleName(String);
impl std::fmt::Display for ModuleName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Module {
    name: ModuleName,
}
impl<'a> From<DefResolver<'a>> for &'a Module {
    fn from(resolver: DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Module(module) => module,
            symbol => panic!("Unexpected symbol seen for module: {}", symbol),
        }
    }
}

impl<'a> Into<DefPathComponent<'a>> for &Module {
    fn into(self) -> DefPathComponent<'a> {
        DefPathComponent::Module(self.symbol_key().clone())
    }
}
impl<'a> Into<SymbolDef> for DefGenerator<'a, Module> {
    fn into(self) -> SymbolDef {
        SymbolDef::Module(self.to_generate_def_for)
    }
}
impl<'a> Symbol<'a> for Module {
    type SymbolKey = ModuleName;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.name
    }
}
impl std::fmt::Display for Module {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

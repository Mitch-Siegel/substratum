use crate::midend::symtab::*;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ScopeIndex(usize);
impl std::fmt::Display for ScopeIndex {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Scope {
    index: ScopeIndex,
}
impl<'a> From<DefResolver<'a>> for &'a Scope {
    fn from(resolver: DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Scope(scope) => scope,
            symbol => panic!("Unexpected symbol seen for scope: {}", symbol),
        }
    }
}

impl<'a> Into<DefPathComponent<'a>> for &Scope {
    fn into(self) -> DefPathComponent<'a> {
        DefPathComponent::Scope(self.symbol_key().clone())
    }
}

impl<'a> Into<SymbolDef> for DefGenerator<'a, Scope> {
    fn into(self) -> SymbolDef {
        SymbolDef::Scope(self.to_generate_def_for)
    }
}

impl<'a> Into<SymbolDef> for Scope {
    fn into(self) -> SymbolDef {
        SymbolDef::Scope(self)
    }
}
impl<'a> Symbol<'a> for Scope {
    type SymbolKey = ScopeIndex;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.index
    }
}
impl std::fmt::Display for Scope {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.index)
    }
}

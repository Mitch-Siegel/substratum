use crate::midend::symtab::*;

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ImplementationName {
    pub implemented_for: TypeId,
    pub generic_params: Vec<String>,
}
impl std::fmt::Display for ImplementationName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "impl<{:?}> {}",
            self.generic_params, self.implemented_for
        )
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Implementation {
    name: ImplementationName,
}

impl Implementation {
    pub fn new(implemented_for: TypeId, generic_params: Vec<String>) -> Self {
        Self {
            name: ImplementationName {
                implemented_for,
                generic_params,
            },
        }
    }
}

impl<'a> From<DefResolver<'a>> for &'a Implementation {
    fn from(resolver: DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Implementation(module) => module,
            symbol => panic!("Unexpected symbol seen for Implementation: {}", symbol),
        }
    }
}
impl<'a> From<MutDefResolver<'a>> for &'a mut Implementation {
    fn from(resolver: MutDefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Implementation(module) => module,
            symbol => panic!("Unexpected symbol seen for Implementation: {}", symbol),
        }
    }
}

impl Into<DefPathComponent> for &Implementation {
    fn into(self) -> DefPathComponent {
        DefPathComponent::Implementation(self.symbol_key().clone())
    }
}
impl<'a> Into<SymbolDef> for DefGenerator<'a, Implementation> {
    fn into(self) -> SymbolDef {
        SymbolDef::Implementation(self.to_generate_def_for)
    }
}
impl Symbol for Implementation {
    type SymbolKey = ImplementationName;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.name
    }
}
impl std::fmt::Display for Implementation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

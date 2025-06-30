use crate::midend::{self, types::TypeSizingContext};

pub struct RegallocContext<'a, C>
where
    C: midend::types::TypeSizingContext,
{
    parent_modules: &'a C,
    self_type: Option<&'a midend::types::ResolvedType>,
    pub function: &'a midend::symtab::Function,
}

impl<'a, C> RegallocContext<'a, C>
where
    C: midend::types::TypeSizingContext,
{
    pub fn new(
        parent_modules: &'a C,
        self_type: Option<&'a midend::types::ResolvedType>,
        function: &'a midend::symtab::Function,
    ) -> Self {
        Self {
            parent_modules,
            self_type,
            function,
        }
    }
}

impl<'a, C> midend::symtab::VariableOwner for RegallocContext<'a, C>
where
    C: midend::types::TypeSizingContext,
{
    fn variables(&self) -> impl Iterator<Item = &midend::symtab::Variable> {
        self.function.variables()
    }
    fn lookup_variable_by_name(
        &self,
        name: &str,
    ) -> Result<&midend::symtab::Variable, midend::symtab::UndefinedSymbol> {
        self.function.lookup_variable_by_name(name)
    }
}

impl<'a, C> midend::symtab::SelfTypeOwner for RegallocContext<'a, C>
where
    C: midend::types::TypeSizingContext,
{
    fn self_type(&self) -> &midend::types::ResolvedType {
        self.self_type.unwrap()
    }
}

impl<'a, C> midend::symtab::TypeOwner for RegallocContext<'a, C>
where
    C: midend::symtab::TypeOwner + TypeSizingContext,
{
    fn types(&self) -> impl Iterator<Item = &midend::symtab::ResolvedTypeDefinition> {
        self.function.types().chain(self.parent_modules.types())
    }

    fn lookup_type(
        &self,
        type_: &midend::types::ResolvedType,
    ) -> Result<&midend::symtab::ResolvedTypeDefinition, midend::symtab::UndefinedSymbol> {
        match self.function.lookup_type(type_) {
            Ok(definition) => Ok(definition),
            Err(_) => self.parent_modules.lookup_type(type_),
        }
    }
}

impl<'a, C> midend::types::TypeSizingContext for RegallocContext<'a, C> where
    C: midend::symtab::VariableSizingContext
{
}

impl<'a, C> midend::symtab::VariableSizingContext for RegallocContext<'a, C> where
    C: midend::symtab::VariableSizingContext
{
}

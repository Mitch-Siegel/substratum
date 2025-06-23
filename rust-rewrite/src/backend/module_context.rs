use crate::midend;
use std::ops::Deref;

struct ModuleContext {
    module_stack: Vec<std::rc::Rc<midend::symtab::Module>>,
}

impl ModuleContext {
    fn all_modules(&self) -> Vec<&midend::symtab::Module> {
        self.module_stack
            .iter()
            .rev()
            .map(|rc| rc.deref())
            .collect()
    }
}

impl midend::symtab::TypeOwner for ModuleContext {
    fn types(&self) -> impl Iterator<Item = &midend::symtab::TypeDefinition> {
        self.all_modules()
            .into_iter()
            .flat_map(|module| module.types())
    }

    fn lookup_type(
        &self,
        type_: &midend::types::Type,
    ) -> Result<&midend::symtab::TypeDefinition, midend::symtab::UndefinedSymbol> {
        for module in self.all_modules() {
            match module.lookup_type(type_) {
                Ok(type_) => return Ok(type_),
                Err(_) => (),
            }
        }

        Err(midend::symtab::UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_struct(
        &self,
        name: &str,
    ) -> Result<&midend::symtab::StructRepr, midend::symtab::UndefinedSymbol> {
        for module in self.all_modules() {
            match module.lookup_struct(name) {
                Ok(struct_) => return Ok(struct_),
                Err(_) => (),
            }
        }

        Err(midend::symtab::UndefinedSymbol::struct_(name.into()))
    }
}

impl midend::symtab::ModuleOwnerships for ModuleContext {}

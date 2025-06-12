use crate::{trace, midend::{symtab::{self, ModuleOwner}, types}};

pub struct ModuleWalkContext {
    module_stack: Vec<symtab::Module>,
    current_module: symtab::Module,
    intrinsics_module: symtab::Module,
}

impl ModuleWalkContext {
    pub fn new() -> Self {
        Self {
            module_stack: Vec::new(),
            current_module: symtab::Module::new("_implicit_".into()),
            intrinsics_module: symtab::intrinsics::create_module(),
        }
    }

    fn all_modules(&self) -> Vec<&symtab::Module> {
        std::iter::once(&self.current_module)
            .chain(self.module_stack.iter().rev())
            .chain(std::iter::once(&self.intrinsics_module))
            .collect()
    }

    fn all_modules_mut(&mut self) -> Vec<&mut symtab::Module> {
        std::iter::once(&mut self.current_module)
            .chain(self.module_stack.iter_mut().rev())
            .chain(std::iter::once(&mut self.intrinsics_module))
            .collect()
    }

    fn new_submodule(&mut self, name: String) {
        let parent = std::mem::replace(&mut self.current_module, symtab::Module::new(name));
        tracing::debug!("Create new submodule: {}::{}", parent.name, self.current_module.name);
        self.module_stack.push(parent);
    }

    fn pop_current_module_to_submodule_of_next(&mut self) -> Result<(), ()> {
        let parent = match self.module_stack.pop() {
            Some(module) => module,
            None => return Err(()),
        };
        let old = std::mem::replace(&mut self.current_module, parent);
        tracing::debug!("Pop current module to submodule of parent: {}::{}", self.current_module.name, old.name);
        match self.current_module.insert_module(old) {
            Ok(()) => Ok(()),
            Err(_) => Err(()),
        }
    }
}

impl Into<symtab::SymbolTable> for ModuleWalkContext {
    fn into(mut self) -> symtab::SymbolTable {
        while !self.module_stack.is_empty() {
            self.pop_current_module_to_submodule_of_next().unwrap();
        }

        symtab::SymbolTable::new(self.current_module)
    }
}

impl symtab::TypeOwner for ModuleWalkContext {
    fn insert_type(&mut self, type_: symtab::TypeDefinition) -> Result<(), symtab::DefinedSymbol> {
        self.current_module.insert_type(type_)
    }

    fn lookup_type(
        &self,
        type_: &types::Type,
    ) -> Result<&symtab::TypeDefinition, symtab::UndefinedSymbol> {
        let _ = trace::span_auto!(trace::Level::TRACE, "Lookup type in module walk context:", "{}", type_);
        for module in self.all_modules() {
            match module.lookup_type(type_) {
                Ok(type_) => return Ok(type_),
                Err(_) => (),
            }
        }

        Err(symtab::UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_type_mut(
        &mut self,
        type_: &types::Type,
    ) -> Result<&mut symtab::TypeDefinition, symtab::UndefinedSymbol> {
        for module in self.all_modules_mut() {
            match module.lookup_type_mut(type_) {
                Ok(type_) => return Ok(type_),
                Err(_) => (),
            }
        }

        Err(symtab::UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_struct(&self, name: &str) -> Result<&symtab::StructRepr, symtab::UndefinedSymbol> {
        for module in self.all_modules() {
            match module.lookup_struct(name) {
                Ok(struct_) => return Ok(struct_),
                Err(_) => (),
            }
        }

        Err(symtab::UndefinedSymbol::struct_(name.into()))
    }
}

impl symtab::FunctionOwner for ModuleWalkContext {
    fn insert_function(&mut self, function: symtab::Function) -> Result<(), symtab::DefinedSymbol> {
        self.current_module.insert_function(function)
    }
    fn lookup_function(&self, name: &str) -> Result<&symtab::Function, symtab::UndefinedSymbol> {
        for module in self.all_modules() {
            match module.lookup_function(name) {
                Ok(function) => return Ok(function),
                Err(_) => (),
            }
        }

        Err(symtab::UndefinedSymbol::function(name.into()))
    }
}

impl symtab::ModuleOwner for ModuleWalkContext {
    fn insert_module(&mut self, module: symtab::Module) -> Result<(), symtab::DefinedSymbol> {
        unimplemented!("insert_module not to be used by ModuleWalkContext");
    }

    fn lookup_module(&self, name: &str) -> Result<&symtab::Module, symtab::UndefinedSymbol> {
        for module in self.all_modules() {
            match module.lookup_module(name) {
                Ok(module) => return Ok(module),
                Err(_) => (),
            }
        }

        Err(symtab::UndefinedSymbol::module(name.into()))
    }
}

impl symtab::SelfTypeOwner for ModuleWalkContext {
    fn self_type(&self) -> &types::Type {
        panic!("Modules can't have a self type!
Need to implement error handling around self type lookups");
    }
}

impl symtab::ModuleOwnerships for ModuleWalkContext {}
impl types::TypeSizingContext for ModuleWalkContext {}

#[cfg(test)]
mod tests {
    use crate::midend::{
            linearizer::modulewalkcontext::*,
            symtab::{self},
        };

    #[test]
    fn pop_current_module_to_submodule_of_next() {
        let mut c = ModuleWalkContext::new();

        c.new_submodule("A".into());
        assert_eq!(c.pop_current_module_to_submodule_of_next(), Ok(()));
        c.new_submodule("B".into());
        c.new_submodule("B1".into());
        assert_eq!(c.pop_current_module_to_submodule_of_next(), Ok(()));
        assert_eq!(c.pop_current_module_to_submodule_of_next(), Ok(()));
        assert_eq!(c.pop_current_module_to_submodule_of_next(), Err(()));
    }

    #[test]
    fn type_owner() {
        let mut c = ModuleWalkContext::new();
        symtab::tests::test_type_owner(&mut c);
    }

    #[test]
    fn function_owner() {
        let mut c = ModuleWalkContext::new();
        symtab::tests::test_function_owner(&mut c);
    }
}

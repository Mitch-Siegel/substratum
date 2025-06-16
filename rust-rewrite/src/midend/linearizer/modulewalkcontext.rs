use crate::{
    midend::{
        symtab::{self, ModuleOwner, MutModuleOwner},
        types,
    },
    trace,
};

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
        tracing::debug!(
            "Create new submodule: {}::{}",
            parent.name,
            self.current_module.name
        );
        self.module_stack.push(parent);
    }

    fn pop_current_module_to_submodule_of_next(&mut self) -> Result<(), ()> {
        let parent = match self.module_stack.pop() {
            Some(module) => module,
            None => return Err(()),
        };
        let old = std::mem::replace(&mut self.current_module, parent);
        tracing::debug!(
            "Pop current module to submodule of parent: {}::{}",
            self.current_module.name,
            old.name
        );
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
    fn types(&self) -> impl Iterator<Item = &symtab::TypeDefinition> {
        self.all_modules()
            .into_iter()
            .flat_map(|module| module.types())
    }

    fn lookup_type(
        &self,
        type_: &types::Type,
    ) -> Result<&symtab::TypeDefinition, symtab::UndefinedSymbol> {
        for module in self.all_modules() {
            match module.lookup_type(type_) {
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

impl symtab::MutTypeOwner for ModuleWalkContext {
    fn insert_type(&mut self, type_: symtab::TypeDefinition) -> Result<(), symtab::DefinedSymbol> {
        self.current_module.insert_type(type_)
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
}

impl symtab::FunctionOwner for ModuleWalkContext {
    fn functions(&self) -> impl Iterator<Item = &symtab::Function> {
        self.all_modules()
            .into_iter()
            .flat_map(|module| module.functions())
    }

    fn lookup_function(&self, name: &str) -> Result<&symtab::Function, symtab::UndefinedSymbol> {
        for module in self.all_modules() {
            let result = module.lookup_function(name);
            match result {
                Ok(function) => return Ok(function),
                Err(_) => (),
            }
        }

        Err(symtab::UndefinedSymbol::function(name.into()))
    }
}
impl symtab::MutFunctionOwner for ModuleWalkContext {
    fn insert_function(&mut self, function: symtab::Function) -> Result<(), symtab::DefinedSymbol> {
        self.current_module.insert_function(function)
    }
}

impl symtab::ModuleOwner for ModuleWalkContext {
    fn submodules(&self) -> impl Iterator<Item = &symtab::Module> {
        self.current_module.submodules()
    }

    fn lookup_submodule(&self, name: &str) -> Result<&symtab::Module, symtab::UndefinedSymbol> {
        for module in self.all_modules() {
            match module.lookup_submodule(name) {
                Ok(module) => return Ok(module),
                Err(_) => (),
            }
        }

        Err(symtab::UndefinedSymbol::module(name.into()))
    }
}

impl symtab::SelfTypeOwner for ModuleWalkContext {
    fn self_type(&self) -> &types::Type {
        panic!(
            "Modules can't have a self type!
Need to implement error handling around self type lookups"
        );
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

use crate::midend::{
    symtab::{self, FunctionOwner, ModuleOwner},
    types::Type,
};

pub struct ModuleWalkContext {
    module_stack: Vec<symtab::Module>,
    current_module: symtab::Module,
}

impl ModuleWalkContext {
    pub fn new() -> Self {
        Self {
            module_stack: Vec::new(),
            current_module: symtab::Module::new("_implicit_".into()),
        }
    }

    fn all_modules(&self) -> Vec<&symtab::Module> {
        std::iter::once(&self.current_module)
            .chain(self.module_stack.iter().rev())
            .collect()
    }

    fn all_modules_mut(&mut self) -> Vec<&mut symtab::Module> {
        std::iter::once(&mut self.current_module)
            .chain(self.module_stack.iter_mut().rev())
            .collect()
    }

    fn pop_current_module_to_submodule_of_next(&mut self) {
        let parent = self.module_stack.pop().unwrap();
        let old = std::mem::replace(&mut self.current_module, parent);
        self.current_module.insert_module(old).unwrap();
    }
}

impl Into<symtab::SymbolTable> for ModuleWalkContext {
    fn into(mut self) -> symtab::SymbolTable {
        while !self.module_stack.is_empty() {
            self.pop_current_module_to_submodule_of_next();
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
        type_: &Type,
    ) -> Result<&symtab::TypeDefinition, symtab::UndefinedSymbol> {
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
        type_: &Type,
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
    fn lookup_function_prototype(
        &self,
        name: &str,
    ) -> Result<&symtab::FunctionPrototype, symtab::UndefinedSymbol> {
        unimplemented!();
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
    fn lookup_function_or_prototype(
        &self,
        name: &str,
    ) -> Result<&symtab::FunctionOrPrototype, symtab::UndefinedSymbol> {
        unimplemented!();
    }
}

impl symtab::ModuleOwnerships for ModuleWalkContext {}

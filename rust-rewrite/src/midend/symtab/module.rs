use crate::midend::symtab::{StructRepr, TypeOwner, TypeRepr, *};
use serde::Serialize;
use std::collections::HashMap;

#[derive(Debug, Clone, Serialize)]
pub struct Module {
    pub name: String,
    pub functions: HashMap<String, Function>,
    pub type_definitions: HashMap<Type, TypeDefinition>,
    pub submodules: HashMap<String, Module>,
}

impl Module {
    pub fn new(name: String) -> Self {
        Self {
            name,
            functions: HashMap::new(),
            type_definitions: HashMap::new(),
            submodules: HashMap::new(),
        }
    }
}

impl PartialEq for Module {
    fn eq(&self, other: &Self) -> bool {
        self.name == other.name
    }
}

impl TypeOwner for Module {
    fn types(&self) -> impl Iterator<Item = &TypeDefinition> {
        self.type_definitions.values()
    }

    fn lookup_type(&self, type_: &Type) -> Result<&TypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get(type_)
            .ok_or(UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_struct(&self, name: &str) -> Result<&StructRepr, UndefinedSymbol> {
        let definition = self.lookup_type(&Type::UDT(name.into()))?;

        match &definition.repr {
            TypeRepr::Struct(struct_) => Ok(struct_),
            _ => Err(UndefinedSymbol::struct_(name.into())),
        }
    }
}
impl MutTypeOwner for Module {
    fn types_mut(&mut self) -> impl Iterator<Item = &mut TypeDefinition> {
        self.type_definitions.values_mut()
    }

    fn insert_type(&mut self, type_: TypeDefinition) -> Result<(), DefinedSymbol> {
        match self.type_definitions.insert(type_.type_().clone(), type_) {
            Some(existing_type) => Err(DefinedSymbol::type_(existing_type.repr)),
            None => Ok(()),
        }
    }

    fn lookup_type_mut(&mut self, type_: &Type) -> Result<&mut TypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get_mut(type_)
            .ok_or(UndefinedSymbol::type_(type_.clone()))
    }
}

impl ModuleOwner for Module {
    fn submodules(&self) -> impl Iterator<Item = &Module> {
        self.submodules.values()
    }

    fn lookup_submodule(&self, name: &str) -> Result<&Module, UndefinedSymbol> {
        self.submodules
            .get(name)
            .ok_or(UndefinedSymbol::module(name.into()))
    }
}
impl MutModuleOwner for Module {
    fn submodules_mut(&mut self) -> impl Iterator<Item = &mut Module> {
        self.submodules.values_mut()
    }

    fn insert_module(&mut self, module: Module) -> Result<(), DefinedSymbol> {
        match self.submodules.insert(module.name.clone(), module) {
            Some(existing_module) => Err(DefinedSymbol::Module(existing_module.name)),
            None => Ok(()),
        }
    }
}

impl FunctionOwner for Module {
    fn functions(&self) -> impl Iterator<Item = &Function> {
        self.functions.values()
    }

    fn lookup_function(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.functions
            .get(name)
            .ok_or(UndefinedSymbol::function(name.into()))
    }
}
impl MutFunctionOwner for Module {
    fn functions_mut(&mut self) -> impl Iterator<Item = &mut Function> {
        self.functions.values_mut()
    }

    fn insert_function(&mut self, function: Function) -> Result<(), DefinedSymbol> {
        match self.functions.insert(function.name().into(), function) {
            Some(existing_function) => Err(DefinedSymbol::function(existing_function.prototype)),
            None => Ok(()),
        }
    }
}

/*
*
*
        Module
     | ExternCrate
     | UseDeclaration
     | Function
     | TypeAlias
     | Struct
     | Enumeration
     | Union
     | ConstantItem
     | StaticItem
     | Trait
     | Implementation
     | ExternBlock//
                  */

use crate::midend::symtab::{StructRepr, TypeOwner, TypeRepr};

use crate::midend::{symtab::*, types::Type};

use std::collections::HashMap;
pub struct Module {
    pub name: String,
    pub functions: HashMap<String, Function>,
    pub type_definitions: HashMap<Type, TypeDefinition>,
    pub implementations: HashMap<Type, Implementation>,
    pub modules: HashMap<String, Module>,
}

impl Module {
    pub fn new(name: String) -> Self {
        Self {
            name,
            functions: HashMap::new(),
            type_definitions: HashMap::new(),
            implementations: HashMap::new(),
            modules: HashMap::new(),
        }
    }
}

impl TypeOwner for Module {
    fn insert_type(&mut self, type_: TypeDefinition) -> Result<(), DefinedSymbol> {
        match self.type_definitions.insert(type_.type_().clone(), type_) {
            Some(existing_type) => Err(DefinedSymbol::type_(existing_type.repr)),
            None => Ok(()),
        }
    }

    fn lookup_type(&self, type_: &Type) -> Result<&TypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get(type_)
            .ok_or(UndefinedSymbol::type_(type_.clone()))
    }

    fn lookup_type_mut(&mut self, type_: &Type) -> Result<&mut TypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get_mut(type_)
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

impl ModuleOwner for Module {
    fn insert_module(&mut self, module: Module) -> Result<(), DefinedSymbol> {
        match self.modules.insert(module.name.clone(), module) {
            Some(existing_module) => Err(DefinedSymbol::Module(existing_module.name)),
            None => Ok(()),
        }
    }
    fn lookup_module(&self, name: &str) -> Result<&Module, UndefinedSymbol> {
        self.modules
            .get(name)
            .ok_or(UndefinedSymbol::module(name.into()))
    }
}

impl FunctionOwner for Module {
    fn insert_function(&mut self, function: Function) -> Result<(), DefinedSymbol> {
        match self.functions.insert(function.name().into(), function) {
            Some(existing_function) => Err(DefinedSymbol::function(existing_function.prototype)),
            None => Ok(()),
        }
    }
    fn lookup_function_prototype(&self, name: &str) -> Result<&FunctionPrototype, UndefinedSymbol> {
        unimplemented!();
    }
    fn lookup_function(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.functions
            .get(name)
            .ok_or(UndefinedSymbol::function(name.into()))
    }
    fn lookup_function_or_prototype(
        &self,
        name: &str,
    ) -> Result<&FunctionOrPrototype, UndefinedSymbol> {
        unimplemented!();
    }
}

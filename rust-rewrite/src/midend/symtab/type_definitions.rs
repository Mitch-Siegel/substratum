use std::collections::HashMap;

use serde::Serialize;

use crate::midend::{symtab::*, types::Type};

use super::{function::FunctionOrPrototype, Function};

#[derive(Debug, Serialize)]
pub struct TypeDefinition {
    type_: Type,
    pub repr: TypeRepr,
    methods: HashMap<String, Function>,
    associated_functions: HashMap<String, Function>,
}

impl TypeDefinition {
    pub fn new(type_: Type, repr: TypeRepr) -> Self {
        TypeDefinition {
            type_,
            repr,
            methods: HashMap::new(),
            associated_functions: HashMap::new(),
        }
    }

    pub fn type_(&self) -> &Type {
        &self.type_
    }
}

impl AssociatedOwner for TypeDefinition {
    fn insert_associated(&mut self, associated: Function) -> Result<(), DefinedSymbol> {
        match self.methods.get(associated.name()) {
            Some(existing_method) => Err(DefinedSymbol::Method(
                self.type_.clone(),
                existing_method.prototype.clone(),
            )),
            None => {
                match self
                    .associated_functions
                    .insert(associated.name().into(), associated)
                {
                    Some(existing_associated) => Err(DefinedSymbol::associated(
                        self.type_.clone(),
                        existing_associated.prototype,
                    )),
                    None => Ok(()),
                }
            }
        }
    }

    fn lookup_associated(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.associated_functions
            .get(name)
            .ok_or(UndefinedSymbol::associated(self.type_.clone(), name.into()))
    }
}

impl MethodOwner for TypeDefinition {
    fn insert_method(&mut self, method: Function) -> Result<(), DefinedSymbol> {
        match self.associated_functions.get(method.name()) {
            Some(existing_associated) => Err(DefinedSymbol::Associated(
                self.type_.clone(),
                existing_associated.prototype.clone(),
            )),
            None => match self.methods.insert(method.name().into(), method) {
                Some(existing_method) => Err(DefinedSymbol::method(
                    self.type_.clone(),
                    existing_method.prototype,
                )),
                None => Ok(()),
            },
        }
    }

    fn lookup_method(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.methods
            .get(name)
            .ok_or(UndefinedSymbol::Method(self.type_.clone(), name.into()))
    }
}

#[derive(Clone, Debug, Serialize)]
pub enum TypeRepr {
    Struct(StructRepr),
}

#[derive(Clone, Debug, Serialize)]
pub struct StructRepr {
    pub name: String,
    fields: HashMap<String, Type>,
    field_order: Vec<String>,
}

impl StructRepr {
    pub fn new(name: String) -> Self {
        Self {
            name,
            fields: HashMap::new(),
            field_order: Vec::new(),
        }
    }

    pub fn add_field(&mut self, name: String, type_: Type) {
        self.fields.insert(name.clone(), type_);
        self.field_order.push(name);
    }

    pub fn get_field_type(&self, name: &String) -> Option<&Type> {
        self.fields.get(name)
    }
}

impl<'a> IntoIterator for &'a StructRepr {
    type Item = (&'a String, &'a Type);
    type IntoIter = std::collections::hash_map::Iter<'a, String, Type>;

    fn into_iter(self) -> Self::IntoIter {
        self.fields.iter()
    }
}

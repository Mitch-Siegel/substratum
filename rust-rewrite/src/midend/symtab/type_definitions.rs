use std::collections::HashMap;

use serde::Serialize;

use crate::midend::types::Type;

use super::{function::FunctionOrPrototype, variable::Variable, Function, UndefinedSymbolError};

#[derive(Debug, Serialize)]
pub struct TypeDefinition {
    type_: Type,
    pub repr: TypeRepr,
    methods: HashMap<String, FunctionOrPrototype>,
    associated_functions: HashMap<String, FunctionOrPrototype>,
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

    pub fn lookup_method(&self, name: &str) -> Result<&FunctionOrPrototype, UndefinedSymbolError> {
        self.methods
            .get(name)
            .ok_or(UndefinedSymbolError::method(&self.type_, name))
    }

    pub fn add_method(&mut self, method: Function) {
        self.methods
            .insert(method.name(), FunctionOrPrototype::Function(method));
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

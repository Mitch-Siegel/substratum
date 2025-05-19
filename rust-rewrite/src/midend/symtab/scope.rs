use std::collections::HashMap;

use serde::Serialize;

use crate::midend::types::Type;

use super::{
    errors::UndefinedSymbolError,
    type_definitions::{StructRepr, TypeDefinition, TypeRepr},
    variable::Variable,
};

#[derive(Debug, Serialize)]
pub struct Scope {
    subscope_indices: Vec<usize>,
    variables: HashMap<String, Variable>,
    subscopes: Vec<Scope>,
    type_definitions: HashMap<Type, TypeDefinition>,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            subscope_indices: Vec::new(),
            variables: HashMap::new(),
            subscopes: Vec::new(),
            type_definitions: HashMap::new(),
        }
    }

    pub fn insert_variable(&mut self, mut variable: Variable) {
        variable.add_mangled_name(&self.subscope_indices);
        self.variables.insert(variable.name().clone(), variable);
    }

    pub fn lookup_declared_variable_by_name<'a>(&'a self, name: &str) -> &'a Variable {
        self.variables
            .get(name)
            .expect(&format!("Use of undeclared variable {}", name))
    }

    pub fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbolError> {
        self.variables
            .get(name)
            .ok_or(UndefinedSymbolError::variable(name))
    }

    pub fn lookup_type<'a>(
        &'a self,
        type_: &Type,
    ) -> Result<&'a TypeDefinition, UndefinedSymbolError> {
        self.type_definitions
            .get(type_)
            .ok_or(UndefinedSymbolError::type_(type_))
    }

    pub fn lookup_type_mut<'a>(
        &'a mut self,
        type_: &Type,
    ) -> Result<&'a mut TypeDefinition, UndefinedSymbolError> {
        self.type_definitions
            .get_mut(type_)
            .ok_or(UndefinedSymbolError::type_(type_))
    }

    pub fn lookup_struct<'a>(&'a self, name: &str) -> Result<&'a StructRepr, UndefinedSymbolError> {
        let struct_type = Type::UDT(name.into());

        match self.type_definitions.get(&struct_type) {
            Some(definition) => match &definition.repr {
                TypeRepr::Struct(struct_definition) => return Ok(struct_definition),
            },
            None => {}
        }

        Err(UndefinedSymbolError::struct_(name))
    }

    pub fn insert_subscope(&mut self, subscope: Scope) {
        self.subscopes.push(subscope);
    }

    pub fn insert_struct_definition(&mut self, defined_struct: StructRepr) {
        let struct_type = Type::UDT(defined_struct.name.clone());
        let def = TypeDefinition::new(struct_type.clone(), TypeRepr::Struct(defined_struct));
        self.type_definitions.insert(struct_type, def);
    }
}

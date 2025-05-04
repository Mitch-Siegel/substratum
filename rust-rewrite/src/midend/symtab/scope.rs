use std::collections::HashMap;

use serde::Serialize;

use crate::midend::types::Type;

use super::{
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

    pub fn lookup_declared_variable_by_name(&self, name: &str) -> &Variable {
        self.variables
            .get(name)
            .expect(&format!("Use of undeclared variable {}", name))
    }

    pub fn lookup_variable_by_name(&self, name: &str) -> Option<&Variable> {
        self.variables.get(name)
    }

    pub fn lookup_type(&self, type_: &Type) -> Option<&TypeDefinition> {
        self.type_definitions.get(type_)
    }

    pub fn lookup_struct(&self, type_: &Type) -> Option<&StructRepr> {
        match self.type_definitions.get(type_) {
            Some(definition) => match &definition.repr {
                TypeRepr::Struct(struct_definition) => return Some(struct_definition),
            },
            None => {}
        }

        None
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

use std::collections::HashMap;

use serde::Serialize;

use crate::midend::types::Type;

use super::{
    errors::UndefinedSymbolError,
    type_definitions::{StructRepr, TypeDefinition, TypeRepr},
    variable::Variable,
};

pub trait ScopedLookups {
    fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbolError>;
    fn lookup_type<'a>(&'a self, type_: &Type) -> Result<&'a TypeDefinition, UndefinedSymbolError>;
    fn lookup_type_mut<'a>(
        &'a mut self,
        type_: &Type,
    ) -> Result<&'a mut TypeDefinition, UndefinedSymbolError>;
    fn lookup_struct<'a>(&'a self, name: &str) -> Result<&'a StructRepr, UndefinedSymbolError>;
}

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

    pub fn insert_subscope(&mut self, subscope: Scope) {
        self.subscopes.push(subscope);
    }

    pub fn insert_struct_definition(&mut self, defined_struct: StructRepr) {
        let struct_type = Type::UDT(defined_struct.name.clone());
        let def = TypeDefinition::new(struct_type.clone(), TypeRepr::Struct(defined_struct));
        self.type_definitions.insert(struct_type, def);
    }
}

impl ScopedLookups for Scope {
    fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbolError> {
        self.variables
            .get(name)
            .ok_or(UndefinedSymbolError::variable(name))
    }

    fn lookup_type<'a>(&'a self, type_: &Type) -> Result<&'a TypeDefinition, UndefinedSymbolError> {
        self.type_definitions
            .get(type_)
            .ok_or(UndefinedSymbolError::type_(type_))
    }

    fn lookup_type_mut<'a>(
        &'a mut self,
        type_: &Type,
    ) -> Result<&'a mut TypeDefinition, UndefinedSymbolError> {
        self.type_definitions
            .get_mut(type_)
            .ok_or(UndefinedSymbolError::type_(type_))
    }

    fn lookup_struct<'a>(&'a self, name: &str) -> Result<&'a StructRepr, UndefinedSymbolError> {
        let struct_type = Type::UDT(name.into());

        match self.type_definitions.get(&struct_type) {
            Some(definition) => match &definition.repr {
                TypeRepr::Struct(struct_definition) => return Ok(struct_definition),
            },
            None => {}
        }

        Err(UndefinedSymbolError::struct_(name))
    }
}

#[derive(Clone)]
pub struct ScopeStack<'a> {
    scopes: Vec<&'a Scope>,
    self_type: Option<Type>,
}

impl<'a> ScopeStack<'a> {
    pub fn new() -> Self {
        Self {
            scopes: Vec::new(),
            self_type: None,
        }
    }

    pub fn set_self_type(&mut self, self_type: Option<Type>) {
        self.self_type = self_type
    }

    pub fn self_type(&self) -> &Type {
        self.self_type
            .as_ref()
            .expect("ScopeStack::self_type() called with self type of None")
    }

    pub fn push(&mut self, scope: &'a Scope) {
        self.scopes.push(scope);
    }
}

impl<'a> ScopedLookups for ScopeStack<'a> {
    fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbolError> {
        for scope in &self.scopes {
            match scope.lookup_variable_by_name(name) {
                Ok(variable) => return Ok(variable),
                Err(_) => {}
            }
        }

        Err(UndefinedSymbolError::variable(name))
    }

    fn lookup_type(&self, type_: &Type) -> Result<&'a TypeDefinition, UndefinedSymbolError> {
        for scope in &self.scopes {
            match scope.lookup_type(type_) {
                Ok(definition) => return Ok(definition),
                Err(_) => {}
            }
        }

        Err(UndefinedSymbolError::type_(type_))
    }

    fn lookup_type_mut(
        &mut self,
        _type_: &Type,
    ) -> Result<&'a mut TypeDefinition, UndefinedSymbolError> {
        panic!("ScopeStack::lookup_type_mut not supported!");
    }
    fn lookup_struct(&self, name: &str) -> Result<&'a StructRepr, UndefinedSymbolError> {
        for scope in &self.scopes {
            match scope.lookup_struct(name) {
                Ok(struct_) => return Ok(struct_),
                Err(_) => {}
            }
        }

        Err(UndefinedSymbolError::struct_(name))
    }
}

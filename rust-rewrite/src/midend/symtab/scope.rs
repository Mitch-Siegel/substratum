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

pub trait CollapseScopes {
    fn collapse_scopes(&mut self, my_subscope_index_in_parent: Option<usize>);
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize)]
pub struct ScopeRelativeAddress {
    subscopes: Vec<usize>,
    base_name: String,
}

impl ScopeRelativeAddress {
    pub fn new(base_name: String) -> Self {
        Self {
            subscopes: Vec::new(),
            base_name,
        }
    }

    pub fn with_new_level(mut self, address: usize) -> Self {
        self.subscopes.push(address);
        self
    }

    pub fn empty(&self) -> bool {
        self.subscopes.len() == 0
    }
}

#[derive(Debug, Serialize)]
pub struct Scope {
    // a 2-dimensional lookup map
    // first, from base name of the variable to all declarations matching that name
    // second, from the actual subscope address the variable comes from to the variable itself
    variables: HashMap<String, HashMap<ScopeRelativeAddress, Variable>>,
    subscopes: Vec<Scope>,
    type_definitions: HashMap<Type, TypeDefinition>,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            variables: HashMap::new(),
            subscopes: Vec::new(),
            type_definitions: HashMap::new(),
        }
    }

    pub fn insert_variable(&mut self, variable: Variable) {
        self.variables
            .entry(variable.name.clone())
            .or_default()
            .insert(ScopeRelativeAddress::new(variable.name.clone()), variable);
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
        let with_this_name = self
            .variables
            .get(name)
            .ok_or(UndefinedSymbolError::variable(name))?;

        for (address, variable) in with_this_name {
            if address.empty() {
                return Ok(variable);
            }
        }

        Err(UndefinedSymbolError::variable(name))
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

impl CollapseScopes for Scope {
    fn collapse_scopes(&mut self, subscope_number: Option<usize>) {
        let mut index = 0;
        while self.subscopes.len() > 0 {
            let mut subscope = self.subscopes.pop().unwrap();
            subscope.collapse_scopes(Some(index));

            for (base_name, variables_with_base_name) in subscope.variables {
                for (address, variable) in variables_with_base_name {
                    let new_address = match subscope_number {
                        Some(subscope_index) => address.with_new_level(subscope_index),
                        None => address,
                    };
                    self.variables
                        .entry(base_name.clone())
                        .or_default()
                        .insert(new_address, variable);
                }
            }

            index += 1;
        }
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

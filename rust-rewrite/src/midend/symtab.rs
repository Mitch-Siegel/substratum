use crate::midend::types::Type;
use std::collections::HashMap;
use super::ir::ControlFlow;

use serde::Serialize;
use serde_json::Result;


#[derive(Debug, Serialize)]
pub struct Variable {
    name: String,
    type_: Type,
}

impl Variable {
    pub fn new(name: String, type_: Type) -> Self {
        Variable { name, type_ }
    }
}

#[derive(Debug, Serialize)]
pub struct Scope {
    variables: HashMap<String, Variable>,
    subscopes: Vec<Scope>,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            variables: HashMap::new(),
            subscopes: Vec::new(),
        }
    }

    pub fn insert_variable(&mut self, variable: Variable) {
        self.variables.insert(variable.name.clone(), variable);
    }
}

#[derive(Debug, Serialize)]
pub struct Function {
    name: String,
    arguments: HashMap<String, Variable>,
    argument_order: Vec<String>,
    main_scope: Option<Scope>, // None if not defined
    control_flow: ControlFlow,
}

impl Function {
    pub fn new(name: String, arguments: Vec<Variable>, main_scope: Option<Scope>) -> Self {
        let mut args_order: Vec<String> = Vec::new();
        for arg in &arguments {
            args_order.push(arg.name.clone());
        }
        Function {
            name,
            arguments: {
                let mut args_map: HashMap<String, Variable> = HashMap::new();
                for arg in arguments {
                    args_map.insert(arg.name.clone(), arg);
                }
                args_map
            },
            argument_order: args_order,
            main_scope,
            control_flow: ControlFlow::new()
        }
    }

    pub fn add_definition(&mut self, definition_scope: Scope) {
        match &self.main_scope {
            Some(t) => panic!("Function {} is already defined!", self.name),
            None => self.main_scope.replace(definition_scope),
        };
    }

    pub fn control_flow(&mut self) -> &mut ControlFlow {
        &mut self.control_flow
    }
}

#[derive(Debug, Serialize)]
pub struct SymbolTable {
    global_scope: Scope,
    functions: HashMap<String, Function>,
}

impl SymbolTable {
    pub fn new() -> Self {
        SymbolTable {
            global_scope: Scope::new(),
            functions: HashMap::new(),
        }
    }
}

pub trait InsertFunction {
    fn InsertFunction(&mut self, function: Function);
}

impl InsertFunction for SymbolTable {
    fn InsertFunction(&mut self, function: Function) {
        self.functions.insert(function.name.clone(), function);
    }
}

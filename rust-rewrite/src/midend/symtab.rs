use super::ir::ControlFlow;
use crate::midend::types::Type;
use std::collections::HashMap;

use serde::Serialize;
use serde_json::Result;

#[derive(Clone, Debug, Serialize)]
pub struct Variable {
    name: String,
    type_: Type,
}

impl Variable {
    pub fn new(name: String, type_: Type) -> Self {
        Variable { name, type_ }
    }

    pub fn type_(&self) -> Type {
        self.type_
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

    pub fn lookup_variable_by_name(&self, name: &str) -> Option<&Variable> {
        self.variables.get(name)
    }

    pub fn insert_subscope(&mut self, subscope: Scope) {
        self.subscopes.push(subscope);
    }
}

#[derive(Debug, Serialize)]
pub struct FunctionPrototype {
    name: String,
    arguments: Vec<Variable>,
    return_type: Option<Type>,
}

impl FunctionPrototype {
    pub fn new(name: String, arguments: Vec<Variable>, return_type: Option<Type>) -> Self {
        FunctionPrototype {
            name,
            arguments,
            return_type,
        }
    }

    pub fn create_argument_scope(&mut self) -> Scope {
        let mut arg_names: Vec<String> = Vec::new();
        let mut argument_scope = Scope::new();
        for arg in &self.arguments {
            arg_names.push(arg.name.clone());
            argument_scope.insert_variable(arg.clone());
        }

        argument_scope
    }
}

#[derive(Debug, Serialize)]
pub struct Function {
    prototype: FunctionPrototype,
    scope: Scope,
    control_flow: ControlFlow,
}

impl Function {
    pub fn new(prototype: FunctionPrototype, scope: Scope, control_flow: ControlFlow) -> Self {
        Function {
            prototype,
            scope,
            control_flow,
        }
    }

    pub fn name(&self) -> String {
        self.prototype.name.clone()
    }

    pub fn control_flow(&mut self) -> &mut ControlFlow {
        &mut self.control_flow
    }
}

#[derive(Debug, Serialize)]
enum FunctionOrPrototype {
    Function(Function),
    Prototype(FunctionPrototype),
}

#[derive(Debug, Serialize)]
pub struct SymbolTable {
    global_scope: Scope,
    functions: HashMap<String, FunctionOrPrototype>,
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
        self.functions
            .insert(function.name(), FunctionOrPrototype::Function(function));
    }
}

impl SymbolTable {
    pub fn InsertFunctionPrototype(&mut self, prototype: FunctionPrototype) {
        self.functions.insert(
            prototype.name.clone(),
            FunctionOrPrototype::Prototype(prototype),
        );
    }
}

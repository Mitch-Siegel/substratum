use super::control_flow::ControlFlow;
use crate::midend::types::Type;
use std::collections::HashMap;

use serde::Serialize;
use std::fmt::Display;

#[derive(Clone, Debug, Serialize)]
pub struct Variable {
    name: String,
    mangled_name: Option<String>,
    type_: Type,
}

impl Display for Variable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}", self.type_, self.name)
    }
}

impl Variable {
    pub fn new(name: String, type_: Type) -> Self {
        Variable {
            name,
            mangled_name: None,
            type_,
        }
    }

    pub fn add_mangled_name(&mut self, scope_indices: &Vec<usize>) {
        let mut mangled_name = String::new();
        for scope in scope_indices {
            mangled_name.push_str(&(scope.to_string() + &String::from("_")));
        }
        mangled_name.push_str(&self.name.clone());

        match &self.mangled_name {
            Some(current_name) => {
                panic!(
                "Variable {} already has mangled name {}, can't add_mangled_name with new name {}",
                self.name,
                current_name,
                mangled_name)
            }
            None => self.mangled_name.replace(mangled_name),
        };
    }

    pub fn type_(&self) -> Type {
        self.type_
    }
}

#[derive(Debug, Serialize)]
pub struct Scope {
    subscope_indices: Vec<usize>,
    variables: HashMap<String, Variable>,
    subscopes: Vec<Scope>,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            subscope_indices: Vec::new(),
            variables: HashMap::new(),
            subscopes: Vec::new(),
        }
    }

    pub fn new_subscope(&mut self) -> Self {
        let mut new_indices = self.subscope_indices.clone();
        new_indices.push(self.subscopes.len());
        Scope {
            subscope_indices: new_indices,
            variables: HashMap::new(),
            subscopes: Vec::new(),
        }
    }

    pub fn insert_variable(&mut self, mut variable: Variable) {
        variable.add_mangled_name(&self.subscope_indices);
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

impl Display for FunctionPrototype {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut arguments_string = String::new();
        for argument in &self.arguments {
            if arguments_string.len() > 0 {
                arguments_string = format!("{}, {}", arguments_string, argument);
            } else {
                arguments_string = format!("{}", argument);
            }
        }
        match self.return_type {
            Some(return_type) => write!(
                f,
                "fun {}({}) -> {}",
                self.name, arguments_string, return_type
            ),
            None => write!(f, "fun {}({})", self.name, arguments_string),
        }
    }
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
            argument_scope.insert_variable(arg.clone())
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

    pub fn prototype(&self) -> &FunctionPrototype {
        &self.prototype
    }

    pub fn control_flow(&self) -> &ControlFlow {
        &self.control_flow
    }

    pub fn scope(&self) -> &Scope {
        &self.scope
    }
}

#[derive(Debug, Serialize)]
pub enum FunctionOrPrototype {
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

    pub fn print_ir(&self) {
        for function in self.functions.values() {
            match function {
                FunctionOrPrototype::Function(f) => {
                    println!("{}", f.prototype());
                    f.control_flow().print_ir();
                }
                FunctionOrPrototype::Prototype(p) => {}
            }
        }
    }

    pub fn functions(self) -> HashMap<String, FunctionOrPrototype>
    {
        self.functions
    }
}

pub trait InsertFunction {
    fn insert_function(&mut self, function: Function);
}

impl InsertFunction for SymbolTable {
    fn insert_function(&mut self, function: Function) {
        self.functions
            .insert(function.name(), FunctionOrPrototype::Function(function));
    }
}

impl SymbolTable {
    pub fn insert_function_prototype(&mut self, prototype: FunctionPrototype) {
        self.functions.insert(
            prototype.name.clone(),
            FunctionOrPrototype::Prototype(prototype),
        );
    }
}

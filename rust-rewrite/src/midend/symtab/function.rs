use std::fmt::Display;

use serde::Serialize;

use crate::midend::{ir, types::Type};

use super::{scope::Scope, variable::Variable};

#[derive(Debug, Serialize)]
pub enum FunctionOrPrototype {
    Function(Function),
    Prototype(FunctionPrototype),
}

#[derive(Debug, Serialize)]
pub struct Function {
    pub prototype: FunctionPrototype,
    pub scope: Scope,
    pub control_flow: ir::ControlFlow,
}

impl Function {
    pub fn new(prototype: FunctionPrototype, scope: Scope, control_flow: ir::ControlFlow) -> Self {
        Function {
            prototype,
            scope,
            control_flow,
        }
    }

    pub fn name(&self) -> String {
        self.prototype.name.clone()
    }
}

#[derive(Debug, Serialize)]
pub struct FunctionPrototype {
    pub name: String,
    pub arguments: Vec<Variable>,
    pub return_type: Type,
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
        match &self.return_type {
            Type::Unit => write!(f, "fun {}({})", self.name, arguments_string),
            _ => write!(
                f,
                "fun {}({}) -> {}",
                self.name, arguments_string, self.return_type
            ),
        }
    }
}

impl FunctionPrototype {
    pub fn new(name: String, arguments: Vec<Variable>, return_type: Type) -> Self {
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
            arg_names.push(arg.name().clone());
            argument_scope.insert_variable(arg.clone())
        }

        argument_scope
    }
}

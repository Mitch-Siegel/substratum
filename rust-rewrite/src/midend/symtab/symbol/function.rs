use std::collections::HashMap;
use std::fmt::Display;

use serde::Serialize;

use crate::midend::{ir, symtab::*, types::Type};

#[derive(Debug)]
pub enum FunctionOrPrototype {
    Function(Function),
    Prototype(FunctionPrototype),
}

impl FunctionOrPrototype {
    pub fn prototype(&self) -> &FunctionPrototype {
        match self {
            FunctionOrPrototype::Function(function) => &function.prototype,
            FunctionOrPrototype::Prototype(function_prototype) => function_prototype,
        }
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct Function {
    pub prototype: FunctionPrototype,
    pub control_flow: ir::ControlFlow,
}

impl Function {
    pub fn new(prototype: FunctionPrototype, control_flow: ir::ControlFlow) -> Self {
        Function {
            prototype,
            control_flow,
        }
    }

    pub fn name(&self) -> &str {
        self.prototype.name.as_str()
    }
}

impl<'a> From<DefResolver<'a>> for &'a Function {
    fn from(resolver: DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            SymbolDef::Function(function) => function,
            symbol => panic!("Unexpected symbol seen for function: {}", symbol),
        }
    }
}

impl<'a> Into<SymbolDef> for DefGenerator<'a, Function> {
    fn into(self) -> SymbolDef {
        SymbolDef::Function(self.to_generate_def_for)
    }
}

impl<'a> Symbol<'a> for Function {
    type SymbolKey = FunctionPrototype;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.prototype
    }
}

impl PartialEq for Function {
    fn eq(&self, other: &Self) -> bool {
        self.prototype == other.prototype
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize, Hash)]
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
}

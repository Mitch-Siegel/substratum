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
    variables: HashMap<String, Variable>,
    type_definitions: HashMap<Type, TypeDefinition>,
    pub control_flow: ir::ControlFlow,
}

impl Function {
    pub fn new(prototype: FunctionPrototype, main_scope: Scope) -> Self {
        let (variables, subscopes, type_definitions, basic_blocks) = main_scope.take_all();

        assert_eq!(subscopes.len(), 0);

        let control_flow = ir::ControlFlow::from(basic_blocks);
        trace::info!("{}", control_flow.graphviz_string());

        Function {
            prototype,
            variables,
            type_definitions,
            control_flow,
        }
    }

    pub fn name(&self) -> &str {
        self.prototype.name.as_str()
    }
}
impl PartialEq for Function {
    fn eq(&self, other: &Self) -> bool {
        self.prototype == other.prototype
    }
}

impl VariableOwner for Function {
    fn variables(&self) -> impl Iterator<Item = &Variable> {
        self.variables.values()
    }

    fn lookup_variable_by_name(&self, name: &str) -> Result<&Variable, UndefinedSymbol> {
        self.variables
            .get(name)
            .ok_or(UndefinedSymbol::variable(name.into()))
    }
}

impl TypeOwner for Function {
    fn types(&self) -> impl Iterator<Item = &TypeDefinition> {
        self.type_definitions.values()
    }

    fn lookup_type(&self, type_: &Type) -> Result<&TypeDefinition, UndefinedSymbol> {
        self.type_definitions
            .get(type_)
            .ok_or(UndefinedSymbol::type_(type_.clone()))
    }
}

impl BasicBlockOwner for Function {
    fn basic_blocks(&self) -> impl Iterator<Item = &ir::BasicBlock> {
        self.control_flow.basic_blocks()
    }

    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock> {
        self.control_flow.lookup_basic_block(label)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
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

    pub fn create_argument_scope(&self) -> Result<Scope, DefinedSymbol> {
        let mut arg_names: Vec<String> = Vec::new();
        let mut argument_scope = Scope::new();
        for arg in &self.arguments {
            arg_names.push(arg.name.clone());
            argument_scope.insert_variable(arg.clone())?
        }

        Ok(argument_scope)
    }
}

use serde::Serialize;

use crate::midend::{ir, symtab::*};

#[derive(Clone, Hash, PartialOrd, Ord, PartialEq, Eq, Serialize)]
pub struct FunctionName {
    pub name: String,
}

impl FunctionName {
    pub fn new(name: String) -> Self {
        Self { name }
    }

    pub fn as_str(&self) -> &str {
        self.name.as_str()
    }
}
impl std::fmt::Display for FunctionName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}
impl std::fmt::Debug for FunctionName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(Debug, Clone)]
pub struct Function {
    pub prototype: FunctionPrototype,
    pub control_flow: Option<ir::ControlFlow>,
}

impl Function {
    pub fn new(prototype: FunctionPrototype, control_flow: Option<ir::ControlFlow>) -> Self {
        Function {
            prototype,
            control_flow,
        }
    }

    pub fn name(&self) -> &str {
        self.prototype.name.as_str()
    }

    pub fn is_lowered(&self) -> bool {
        if let Some(cf) = &self.control_flow {
            for block in cf {
                for statement in block {
                    match statement.operation {
                        ir::Operation::Lowered(_) => (),
                        ir::Operation::Unlowered(_) => return false,
                    }
                }
            }
        }
        true
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
impl<'a> From<MutDefResolver<'a>> for &'a mut Function {
    fn from(resolver: MutDefResolver<'a>) -> Self {
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

impl Symbol for Function {
    type SymbolKey = FunctionName;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.prototype.name
    }
}

impl PartialEq for Function {
    fn eq(&self, other: &Self) -> bool {
        self.prototype == other.prototype
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize, Hash)]
pub struct FunctionPrototype {
    pub name: FunctionName,
    pub generic_params: Vec<String>,
    pub arguments: Vec<Variable>,
    pub return_type: types::Syntactic,
}

impl std::fmt::Display for FunctionPrototype {
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
            types::Syntactic::Unit => write!(f, "fun {}({})", self.name.as_str(), arguments_string),
            _ => write!(
                f,
                "fun {}({}) -> {}",
                self.name.as_str(),
                arguments_string,
                self.return_type
            ),
        }
    }
}

impl FunctionPrototype {
    pub fn new(
        name: String,
        generic_params: Vec<String>,
        arguments: Vec<Variable>,
        return_type: types::Syntactic,
    ) -> Self {
        FunctionPrototype {
            name: FunctionName { name },
            generic_params,
            arguments,
            return_type,
        }
    }
}

use crate::midend::symtab::{Symbol, *};

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

impl Symbol for Function {
    fn name(&self) -> &str {
        &self.prototype.name
    }

    fn path_segment(&self) -> PathSegment {
        PathSegment::Value(self.name().into())
    }

    fn into_repr(self) -> SymbolDef {
        SymbolDef::Value(Value::Function(self))
    }
}

impl std::fmt::Display for Function {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{} - has cf? {}",
            self.prototype,
            self.control_flow.is_some()
        )
    }
}

impl PartialEq for Function {
    fn eq(&self, other: &Self) -> bool {
        self.prototype == other.prototype
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct FunctionPrototype {
    pub name: String,
    pub generic_params: midend::types::GenericParamsList,
    pub arguments: Vec<Variable>,
    pub return_type: midend::types::Syntactic,
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
            midend::types::Syntactic::Unit => {
                write!(f, "fun {}({})", self.name.as_str(), arguments_string)
            }
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
        generic_params: midend::types::GenericParamsList,
        arguments: Vec<Variable>,
        return_type: midend::types::Syntactic,
    ) -> Self {
        FunctionPrototype {
            name: name,
            generic_params,
            arguments,
            return_type,
        }
    }
}

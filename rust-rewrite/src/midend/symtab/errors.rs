use crate::midend::types::Type;

pub enum UndefinedSymbolError {
    Function(String),
    Associated(Type, String),
    Method(Type, String),
    Variable(String),
    Type(Type),
    Struct(String),
}

impl std::fmt::Display for UndefinedSymbolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Function(function) => {
                write!(f, "Undefined function {}", function)
            }
            Self::Associated(associated_with, method_name) => {
                write!(
                    f,
                    "{} has no associated function {}",
                    associated_with, method_name
                )
            }
            Self::Method(receiver, method_name) => {
                write!(f, "{} has no method {}", receiver, method_name)
            }
            Self::Variable(variable) => {
                write!(f, "Undeclared variable {}", variable)
            }
            Self::Type(type_) => write!(f, "Undeclared type {}", type_),
            Self::Struct(struct_) => write!(f, "Undeclared struct {}", struct_),
        }
    }
}

impl std::fmt::Debug for UndefinedSymbolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self)
    }
}

impl UndefinedSymbolError {
    pub fn function(name: &str) -> Self {
        Self::Function(name.into())
    }

    pub fn associated(associated_with: Type, method_name: String) -> Self {
        Self::Associated(associated_with, method_name)
    }

    pub fn method(receiver: Type, method_name: String) -> Self {
        Self::Method(receiver, method_name)
    }

    pub fn variable(name: String) -> Self {
        Self::Variable(name)
    }

    pub fn type_(type_: Type) -> Self {
        Self::Type(type_)
    }

    pub fn struct_(name: String) -> Self {
        Self::Struct(name)
    }
}

use crate::midend::types::Type;

pub enum UndefinedSymbolError {
    Function(String),
    Variable(String),
    Type(Type),
    Struct(String),
}

impl std::fmt::Display for UndefinedSymbolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            UndefinedSymbolError::Function(function) => {
                write!(f, "Undefined function {}", function)
            }
            UndefinedSymbolError::Variable(variable) => {
                write!(f, "Undeclared variable {}", variable)
            }
            UndefinedSymbolError::Type(type_) => write!(f, "Undeclared type {}", type_),
            UndefinedSymbolError::Struct(struct_) => write!(f, "Undeclared struct {}", struct_),
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

    pub fn variable(name: &str) -> Self {
        Self::Variable(name.into())
    }

    pub fn type_(type_: &Type) -> Self {
        Self::Type(type_.clone())
    }

    pub fn struct_(name: &str) -> Self {
        Self::Struct(name.into())
    }
}

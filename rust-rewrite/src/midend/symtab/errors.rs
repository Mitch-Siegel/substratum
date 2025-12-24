use crate::midend::symtab::*;

#[derive(Debug, PartialEq, Eq)]
pub enum SymbolError {
    // TODO: is there any good reason for the path and component to be separated for
    // undeclared/undefined errors?
    Undeclared(DefPath),
    Undefined(DefPath),
    AlreadyDeclared(DefPath),
    AlreadyDefined(DefPath),
    PathError(PathError),
}

impl From<PathError> for SymbolError {
    fn from(value: PathError) -> Self {
        SymbolError::PathError(value)
    }
}

impl std::fmt::Display for SymbolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Undefined(path) => write!(f, "undefined symbol {}", path),
            Self::Undeclared(path) => write!(f, "undeclared symbol {}", path),
            Self::AlreadyDeclared(path) => write!(f, "DefPath {} is already declared", path),
            Self::AlreadyDefined(path) => write!(f, "DefPath {} is already defined", path),
            Self::PathError(pe) => {
                write!(f, "{}", pe)
            }
        }
    }
}

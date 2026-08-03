use crate::midend::symtab::{PathError, RawPath, TypePath};

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum SymbolError {
    // TODO: is there any good reason for the path and component to be separated for
    // undeclared/undefined errors?
    Undeclared(RawPath),
    Undefined(RawPath),
    UndefinedType(TypePath),
    AlreadyDeclared(RawPath),
    AlreadyDefined(RawPath),
    TypeAlreadyDefined(TypePath),
    PathError(PathError),
}

impl From<PathError> for SymbolError {
    fn from(value: PathError) -> Self {
        Self::PathError(value)
    }
}

impl std::fmt::Display for SymbolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Undefined(path) => write!(f, "undefined symbol {path}"),
            Self::UndefinedType(path) => write!(f, "undefined type {path}"),
            Self::Undeclared(path) => write!(f, "undeclared symbol {path}"),
            Self::AlreadyDeclared(path) => write!(f, "DefPath {path} is already declared"),
            Self::AlreadyDefined(path) => write!(f, "DefPath {path} is already defined"),
            Self::TypeAlreadyDefined(path) => write!(f, "type {path} is already defined"),
            Self::PathError(pe) => {
                write!(f, "{pe}")
            }
        }
    }
}

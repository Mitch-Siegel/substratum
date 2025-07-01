use crate::midend::{symtab::*, types::Type};

#[derive(PartialEq, Eq)]
pub enum SymbolError {
    Undefined(UndefinedSymbol),
    Defined(DefinedSymbol),
    CantOwn(String, String),
}
impl std::fmt::Debug for SymbolError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Undefined(undefined) => write!(f, "{}", undefined),
            Self::Defined(defined) => write!(f, "{}", defined),
            Self::CantOwn(owner, ownee) => write!(f, "{} can't own {}", owner, ownee),
        }
    }
}

impl From<UndefinedSymbol> for SymbolError {
    fn from(undefined: UndefinedSymbol) -> Self {
        Self::Undefined(undefined)
    }
}

impl From<DefinedSymbol> for SymbolError {
    fn from(defined: DefinedSymbol) -> Self {
        Self::Defined(defined)
    }
}

#[derive(PartialEq, Eq)]
pub enum UndefinedSymbol {
    Function(String),
    Associated(Type, String),
    Method(Type, String),
    Variable(String),
    Type(Type),
    Struct(String),
    Module(String),
    Field(String),
}

impl std::fmt::Display for UndefinedSymbol {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Function(function) => {
                write!(f, "Undefined function {}", function)
            }
            Self::Associated(associated_with, associated_name) => {
                write!(
                    f,
                    "{} has no associated function {}",
                    associated_with, associated_name
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
            Self::Module(module) => write!(f, "Undeclared module {}", module),
            Self::Field(field) => write!(f, "Undeclared field {}", field),
        }
    }
}

impl std::fmt::Debug for UndefinedSymbol {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self)
    }
}

impl UndefinedSymbol {
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

    pub fn module(name: String) -> Self {
        Self::Module(name)
    }

    pub fn field(name: String) -> Self {
        Self::Field(name)
    }
}

#[derive(PartialEq, Eq)]
pub enum DefinedSymbol {
    Function(FunctionPrototype),
    Associated(Type, FunctionPrototype),
    Method(Type, FunctionPrototype),
    Variable(DefPath),
    Type(DefPath),
    Struct(DefPath),
    Module(String),
    Field(StructField),
}

impl std::fmt::Display for DefinedSymbol {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Function(prototype) => {
                write!(f, "Function {} is already defined", prototype)
            }
            Self::Associated(associated_with, prototype) => {
                write!(
                    f,
                    "Associated function {}::{} is already defined",
                    associated_with, prototype
                )
            }
            Self::Method(receiver, prototype) => {
                write!(f, "Method {}.{} is already defined", receiver, prototype)
            }
            Self::Variable(variable) => {
                write!(f, "Variable {} is already defined", variable)
            }
            Self::Type(type_) => write!(f, "Type {} is already defined", type_),
            Self::Struct(struct_) => write!(f, "Struct {} is already defined", struct_),
            Self::Module(module) => write!(f, "Module {} is already defined", module),
            Self::Field(field) => write!(f, "Struct field {} is already defined", field),
        }
    }
}

impl std::fmt::Debug for DefinedSymbol {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self)
    }
}

impl DefinedSymbol {
    pub fn function(prototype: FunctionPrototype) -> Self {
        Self::Function(prototype)
    }

    pub fn associated(associated_with: Type, prototype: FunctionPrototype) -> Self {
        Self::Associated(associated_with, prototype)
    }

    pub fn method(receiver: Type, prototype: FunctionPrototype) -> Self {
        Self::Method(receiver, prototype)
    }

    pub fn variable(variable: DefPath) -> Self {
        Self::Variable(variable)
    }

    pub fn type_(type_: DefPath) -> Self {
        Self::Type(type_)
    }

    pub fn struct_(struct_: DefPath) -> Self {
        Self::Struct(struct_)
    }

    pub fn module(module: String) -> Self {
        Self::Module(module)
    }

    pub fn field(field: StructField) -> Self {
        Self::Field(field)
    }
}

use std::fmt;

use crate::{
    ast::{Ast, module},
    sourceloc,
};

pub mod enum_definition;
pub mod function;
pub mod implementation;
pub mod struct_definition;

pub use enum_definition::EnumDefinitionTree;
pub use function::{FunctionDeclarationTree, FunctionDefinitionTree};
pub use implementation::ImplementationTree;
pub use struct_definition::StructDefinitionTree;

#[derive(Debug, PartialEq, Eq, Clone, serde::Serialize, serde::Deserialize)]
pub enum ItemTree {
    FunctionDeclaration(FunctionDeclarationTree),
    FunctionDefinition(FunctionDefinitionTree),
    StructDefinition(StructDefinitionTree),
    EnumDefinition(EnumDefinitionTree),
    Implementation(ImplementationTree),
    // TODO: make this an enum
    Module(Result<module::ModuleTree, (sourceloc::SourceSpan, String)>),
}

impl Ast for ItemTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::FunctionDeclaration(fdecl) => fdecl.loc(),
            Self::FunctionDefinition(fdef) => fdef.loc(),
            Self::StructDefinition(sd) => sd.loc(),
            Self::EnumDefinition(ed) => ed.loc(),
            Self::Implementation(i) => i.loc(),
            Self::Module(module) => match module {
                Ok(module_tree) => module_tree.loc(),
                Err((loc, _)) => loc.clone(),
            },
        }
    }
}

impl fmt::Display for ItemTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::FunctionDeclaration(function_declaration) => {
                write!(f, "Function Declaration: {function_declaration}")
            }
            Self::FunctionDefinition(function_definition) => {
                write!(f, "Function Definition: {function_definition}")
            }
            Self::StructDefinition(struct_definition) => {
                write!(f, "Struct Definition: {struct_definition}")
            }
            Self::EnumDefinition(enum_definition) => {
                write!(f, "Enum Definition: {enum_definition}")
            }
            Self::Implementation(implementation) => {
                write!(f, "Implementation: {implementation}")
            }
            Self::Module(module) => match module {
                Ok(parsed) => write!(f, "Module: {parsed}"),
                Err((_, name)) => write!(f, "Module: {name}"),
            },
        }
    }
}

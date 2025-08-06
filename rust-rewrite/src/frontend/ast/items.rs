use crate::frontend::ast::*;
use std::collections::BTreeSet;

pub mod enum_definition;
pub mod function;
pub mod implementation;
pub mod struct_definition;

pub use enum_definition::EnumDefinitionTree;
pub use function::{FunctionDeclarationTree, FunctionDefinitionTree};
pub use implementation::ImplementationTree;
pub use struct_definition::StructDefinitionTree;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ItemTree {
    pub loc: SourceLoc,
}

impl Display for ItemTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.loc)
    }
}

#[derive(Debug, PartialEq, Clone, serde::Serialize, serde::Deserialize)]
pub enum Item {
    FunctionDeclaration(FunctionDeclarationTree),
    FunctionDefinition(FunctionDefinitionTree),
    StructDefinition(StructDefinitionTree),
    EnumDefinition(EnumDefinitionTree),
    Implementation(ImplementationTree),
    Module((Option<module::ModuleTree>, BTreeSet<String>)),
}

impl Display for Item {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::FunctionDeclaration(function_declaration) => {
                write!(f, "Function Declaration: {}", function_declaration)
            }
            Self::FunctionDefinition(function_definition) => {
                write!(f, "Function Definition: {}", function_definition)
            }
            Self::StructDefinition(struct_definition) => {
                write!(f, "Struct Definition: {}", struct_definition)
            }
            Self::EnumDefinition(enum_definition) => {
                write!(f, "Enum Definition: {}", enum_definition)
            }
            Self::Implementation(implementation) => {
                write!(f, "Implementation: {}", implementation)
            }
            Self::Module((module, child_modules)) => match module {
                Some(parsed) => write!(f, "Module: {}", parsed),
                None => write!(f, "Module: {}", child_modules.first().unwrap()),
            },
        }
    }
}

impl CustomReturnWalk<midend::linearizer::BasicDefContext, midend::linearizer::BasicDefContext>
    for Item
{
    fn walk(
        self,
        mut context: midend::linearizer::BasicDefContext,
    ) -> midend::linearizer::BasicDefContext {
        match self {
            Item::FunctionDeclaration(function_declaration) => {
                unimplemented!(
                    "Function declaration without definitions not yet supported: {}",
                    function_declaration.name
                )
                /*
                let function_context = FunctionWalkContext::new(context);
                let declared_function = function_declaration.walk(function_context);

                    function_declaration.walk(&mut WalkContext::new(&context.global_scope));
                context.insert_function_prototype(declared_function);*/
            }
            Item::FunctionDefinition(function_definition) => function_definition.walk(context),
            Item::StructDefinition(struct_tree) => {
                let struct_repr = struct_tree.walk(&mut context);
                context
                    .insert::<midend::symtab::TypeDefinition>(midend::symtab::TypeDefinition::new(
                        midend::types::Syntactic::Named(struct_repr.name.clone()),
                        midend::symtab::TypeRepr::Struct(struct_repr),
                    ))
                    .unwrap();
                context
            }
            Item::EnumDefinition(enum_tree) => {
                let enum_repr = enum_tree.walk(&mut context);
                context
                    .insert::<midend::symtab::TypeDefinition>(midend::symtab::TypeDefinition::new(
                        midend::types::Syntactic::Named(enum_repr.name.clone()),
                        midend::symtab::TypeRepr::Enum(enum_repr),
                    ))
                    .unwrap();
                context
            }
            Item::Implementation(implementation) => implementation.walk(context),
            Item::Module((module, _)) => match module {
                Some(m) => m.walk(context),
                None => context,
            },
        }
    }
}

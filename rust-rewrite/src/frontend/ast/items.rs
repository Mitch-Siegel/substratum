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

#[derive(Debug, PartialEq, Eq, Clone, serde::Serialize, serde::Deserialize)]
pub enum ItemTree {
    FunctionDeclaration(FunctionDeclarationTree),
    FunctionDefinition(FunctionDefinitionTree),
    StructDefinition(StructDefinitionTree),
    EnumDefinition(EnumDefinitionTree),
    Implementation(ImplementationTree),
    Module(
        (
            Result<module::ModuleTree, sourceloc::SourceSpan>,
            BTreeSet<String>,
        ),
    ),
}

impl Ast<()> for ItemTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::FunctionDeclaration(fdecl) => fdecl.loc(),
            Self::FunctionDefinition(fdef) => fdef.loc(),
            Self::StructDefinition(sd) => sd.loc(),
            Self::EnumDefinition(ed) => ed.loc(),
            Self::Implementation(i) => i.loc(),
            Self::Module((module, child_modules)) => match module {
                Ok(module_tree) => module_tree.loc(),
                Err(loc) => loc.clone(),
            },
        }
    }
}

impl midend::treewalk::Treewalk<()> for ItemTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        match self {
            ItemTree::FunctionDeclaration(function_declaration) => {
                unimplemented!(
                    "Function declaration without definitions not yet supported: {}",
                    function_declaration.name
                )
            }
            ItemTree::FunctionDefinition(function_definition) => {
                function_definition.collect_symbols(ctx)
            }
            ItemTree::StructDefinition(struct_tree) => struct_tree.collect_symbols(ctx),
            ItemTree::EnumDefinition(enum_tree) => enum_tree.collect_symbols(ctx),
            ItemTree::Implementation(implementation) => implementation.collect_symbols(ctx),
            ItemTree::Module((module, _)) => match module {
                Ok(m) => m.collect_symbols(ctx),
                Err(_) => (),
            },
        }
    }

    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> () {
        match self {
            ItemTree::FunctionDeclaration(function_declaration) => {
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
            ItemTree::FunctionDefinition(function_definition) => function_definition.linearize(ctx),
            ItemTree::StructDefinition(struct_tree) => {
                let struct_repr = struct_tree.linearize(ctx);

                ctx.define::<midend::symtab::TypeDefinition>(midend::symtab::TypeDefinition::new(
                    midend::types::Syntactic::Named(struct_repr.name.clone()),
                    struct_repr.generic_params.clone(),
                    midend::symtab::TypeRepr::Struct(struct_repr),
                ))
                .unwrap();
            }
            ItemTree::EnumDefinition(enum_tree) => {
                let enum_repr = enum_tree.linearize(ctx);

                ctx.define::<midend::symtab::TypeDefinition>(midend::symtab::TypeDefinition::new(
                    midend::types::Syntactic::Named(enum_repr.name.clone()),
                    enum_repr.generic_params.clone(),
                    midend::symtab::TypeRepr::Enum(enum_repr),
                ))
                .unwrap();
            }
            ItemTree::Implementation(implementation) => implementation.linearize(ctx),
            ItemTree::Module((module, _)) => match module {
                Ok(m) => m.linearize(ctx),
                Err(_) => (),
            },
        }
    }
}


impl Display for ItemTree {
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
                Ok(parsed) => write!(f, "Module: {}", parsed),
                Err(_) => write!(f, "Module: {}", child_modules.first().unwrap()),
            },
        }
    }
}

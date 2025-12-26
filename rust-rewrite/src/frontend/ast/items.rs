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

impl Ast for ItemTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::FunctionDeclaration(fdecl) => fdecl.loc(),
            Self::FunctionDefinition(fdef) => fdef.loc(),
            Self::StructDefinition(sd) => sd.loc(),
            Self::EnumDefinition(ed) => ed.loc(),
            Self::Implementation(i) => i.loc(),
            Self::Module((module, _)) => match module {
                Ok(module_tree) => module_tree.loc(),
                Err(loc) => loc.clone(),
            },
        }
    }
}

impl midend::treewalk::Collect for ItemTree {
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
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
                Err(_) => Ok(ctx.take()),
            },
        }
    }
}

impl midend::treewalk::Linearize for ItemTree {
    type Data = ();
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let ctx = match self {
            ItemTree::FunctionDeclaration(function_declaration) => {
                unimplemented!(
                    "Function declaration without definitions not yet supported: {}",
                    function_declaration.name
                );
            }
            ItemTree::FunctionDefinition(function_definition) => {
                let function;
                (function, ctx) = function_definition.linearize_same_path(ctx)?;
                ctx.define_value(function)?;
                ctx.take()
            }
            ItemTree::StructDefinition(struct_tree) => {
                let struct_repr;
                (struct_repr, ctx) = struct_tree.linearize_same_path(ctx)?;
                ctx.define_type(struct_repr)?;
                ctx.take()
            }
            ItemTree::EnumDefinition(enum_tree) => {
                let enum_repr;
                (enum_repr, ctx) = enum_tree.linearize_same_path(ctx)?;
                ctx.define_type(enum_repr)?;
                ctx.take()
            }
            ItemTree::Implementation(_implementation) => {
                unimplemented!();
                //let (_, ctx) = implementation.linearize(ctx)?;
                //ctx
            }
            ItemTree::Module((module, _)) => match module {
                Ok(m) => m.linearize(ctx)?.1,
                Err(_) => ctx.take(),
            },
        };
        ctx.into_result(())
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

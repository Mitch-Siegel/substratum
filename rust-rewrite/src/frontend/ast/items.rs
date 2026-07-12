use crate::{
    frontend::ast::*,
    midend::{
        symtab,
        treewalk::{self},
    },
};
use std::collections::BTreeSet;

pub(crate) mod enum_definition;
pub(crate) mod function;
pub(crate) mod implementation;
pub(crate) mod struct_definition;

pub(crate) use enum_definition::EnumDefinitionTree;
pub(crate) use function::{FunctionDeclarationTree, FunctionDefinitionTree};
pub(crate) use implementation::ImplementationTree;
pub(crate) use struct_definition::StructDefinitionTree;

#[derive(Debug, PartialEq, Eq, Clone, serde::Serialize, serde::Deserialize)]
pub(crate) enum ItemTree {
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

impl midend::treewalk::Collect<midend::symtab::TypePath> for ItemTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx = match self {
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
                Err(_) => Ok(ctx),
            },
        }?;

        ctx.into_result()
    }
}

impl<C> treewalk::Linearize<treewalk::UnpathedLinearizeCtx, symtab::TypePath, C> for ItemTree
where
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedLinearizeCtx,
        Path = symtab::TypePath,
    >,
    // TypeNoBoundsTree: treewalk::Linearize<
    //     treewalk::UnpathedLinearizeCtx,
    //     symtab::TypePath,
    //     C,
    //     Data = Option<midend::types::Syntactic>,
    // >,
{
    type Data = ();
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> treewalk::LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let ctx = match self {
            ItemTree::FunctionDeclaration(function_declaration) => {
                unimplemented!(
                    "Function declaration without definitions not yet supported: {}",
                    function_declaration.name
                );
            }
            ItemTree::FunctionDefinition(function_definition) => {
                let function;
                (function, ctx) = function_definition.linearize(ctx)?;
                ctx.define_value(midend::symtab::Value::Function(Box::new(function)))?;
                ctx
            }
            ItemTree::StructDefinition(struct_tree) => {
                let struct_repr;
                let generic_params;
                ((struct_repr, generic_params), ctx) = struct_tree.linearize(ctx)?;
                ctx.define_type(midend::symtab::Type::Decl(
                    midend::symtab::types::TypeDecl {
                        declared_type: struct_repr.into(),
                        generic_params,
                    },
                ))?;
                ctx
            }
            ItemTree::EnumDefinition(enum_tree) => {
                let enum_repr;
                let generic_params;
                ((enum_repr, generic_params), ctx) = enum_tree.linearize(ctx)?;
                ctx.define_type(midend::symtab::Type::Decl(
                    midend::symtab::types::TypeDecl {
                        declared_type: enum_repr.into(),
                        generic_params,
                    },
                ))?;
                ctx
            }
            ItemTree::Implementation(_implementation) => {
                unimplemented!();
                //let (_, ctx) = implementation.linearize(ctx)?;
                //ctx
            }
            ItemTree::Module((module, _)) => match module {
                Ok(m) => m.linearize(ctx)?.1,
                Err(_) => ctx,
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

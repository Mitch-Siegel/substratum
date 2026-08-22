use frontend::ast;

use crate::{
    symtab,
    treewalk::{
        Collect, CollectResult, Linearize, LinearizeResult, PathedCtx, PathedCtxTrait,
        TypeCollectCtx, UnpathedLinearizeCtx,
    },
};

pub(crate) mod walk_enum_definition;
pub(crate) mod walk_function;
pub(crate) mod walk_implementation;
pub(crate) mod walk_module;
pub(crate) mod walk_struct_definition;

impl Collect<symtab::TypePath> for ast::ItemTree {
    fn collect_inner(&self, mut ctx: TypeCollectCtx) -> CollectResult {
        ctx = match self {
            Self::FunctionDeclaration(function_declaration) => {
                unimplemented!(
                    "Function declaration without definitions not yet supported: {}",
                    function_declaration.name
                )
            }
            Self::FunctionDefinition(function_definition) => {
                function_definition.collect_symbols(ctx)
            }
            Self::StructDefinition(struct_tree) => struct_tree.collect_symbols(ctx),
            Self::EnumDefinition(enum_tree) => enum_tree.collect_symbols(ctx),
            Self::Implementation(implementation) => implementation.collect_symbols(ctx),
            Self::Module(module) => match module {
                Ok(m) => m.collect_symbols(ctx),
                Err(_) => Ok(ctx),
            },
        }?;

        ctx.into_result()
    }
}

impl
    Linearize<
        UnpathedLinearizeCtx,
        symtab::TypePath,
        PathedCtx<UnpathedLinearizeCtx, symtab::TypePath>,
    > for ast::ItemTree
{
    type Data = ();
    fn linearize_inner(
        self,
        mut ctx: PathedCtx<UnpathedLinearizeCtx, symtab::TypePath>,
    ) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let ctx = match self {
            Self::FunctionDeclaration(function_declaration) => {
                unimplemented!(
                    "Function declaration without definitions not yet supported: {}",
                    function_declaration.name
                );
            }
            Self::FunctionDefinition(function_definition) => {
                let function;
                (function, ctx) = function_definition.linearize(ctx)?;
                ctx.define_value(symtab::Value::Function(Box::new(function)))?;
                ctx
            }
            Self::StructDefinition(struct_tree) => {
                let struct_repr;
                let generic_params;
                ((struct_repr, generic_params), ctx) = struct_tree.linearize(ctx)?;
                ctx.define_type(symtab::Type::Decl(symtab::types::TypeDecl {
                    declared_type: struct_repr.into(),
                    generic_params,
                }))?;
                ctx
            }
            Self::EnumDefinition(enum_tree) => {
                let enum_repr;
                let generic_params;
                ((enum_repr, generic_params), ctx) = enum_tree.linearize(ctx)?;
                ctx.define_type(symtab::Type::Decl(symtab::types::TypeDecl {
                    declared_type: enum_repr.into(),
                    generic_params,
                }))?;
                ctx
            }
            Self::Implementation(_implementation) => {
                unimplemented!();
                //let (_, ctx) = implementation.linearize(ctx)?;
                //ctx
            }
            Self::Module(module) => match module {
                Ok(m) => m.linearize(ctx)?.1,
                Err(_) => ctx,
            },
        };
        ctx.into_result(())
    }
}

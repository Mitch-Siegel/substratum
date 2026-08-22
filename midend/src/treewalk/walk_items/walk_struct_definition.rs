use frontend::ast;

use crate::{
    symtab,
    treewalk::{
        Collect, CollectResult, Linearize, LinearizeResult, PathedCtx, PathedCtxTrait,
        PathedLinearizeCtxTrait, TypeCollectCtx, UnpathedLinearizeCtxTrait,
    },
    types,
};

impl<U, P, C> Linearize<U, P, C> for ast::items::struct_definition::StructFieldTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    ast::TypeTree: Linearize<U, P, C, Data = types::Syntactic>,
    ast::types::TypeNoBoundsTree: Linearize<U, P, C>,
{
    type Data = (String, types::Syntactic);
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (field_type, ctx) = self.type_.linearize(ctx)?;

        let (name, ctx) = self.name.linearize(ctx)?;
        ctx.into_result((name, field_type))
    }
}

impl Collect<symtab::TypePath> for ast::items::StructDefinitionTree {
    fn collect_inner(&self, mut ctx: TypeCollectCtx) -> CollectResult {
        let _struct_path = ctx.declare_type(self.name.value.clone())?;

        let struct_ctx = ctx.with_child_type(self.name.value.clone());

        self.generic_params
            .collect_symbols(struct_ctx)?
            .into_result()
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::items::StructDefinitionTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path + symtab::TypeOwner,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    ast::generics::OptionalGenericParamsListTree: Linearize<U, P, C>,
    ast::TypeTree: Linearize<U, P, C, Data = types::Syntactic>,
    ast::items::struct_definition::StructFieldTree: Linearize<
            U,
            symtab::TypePath,
            PathedCtx<U, symtab::TypePath>,
            Data = (String, types::Syntactic),
        >,
{
    type Data = (symtab::types::StructRepr, types::GenericParamsList);
    fn linearize_inner(self, mut ctx: C) -> LinearizeResult<Self::Data, U> {
        let struct_name: String;
        (struct_name, ctx) = self.name.linearize(ctx)?;

        let mut ctx: PathedCtx<U, symtab::TypePath> = ctx.with_child_type(struct_name.clone());

        let mut fields = Vec::new();
        for field in self.fields {
            let linearized_field;
            (linearized_field, ctx) = field.linearize(ctx)?;
            fields.push(linearized_field);
        }

        // TODO: struct duplicate field error
        let struct_repr = symtab::types::StructRepr::new(struct_name, fields).unwrap();

        let (params, ctx) = <ast::generics::OptionalGenericParamsListTree as Linearize<
            U,
            symtab::TypePath,
            PathedCtx<U, symtab::TypePath>,
        >>::linearize(self.generic_params, ctx)?;

        ctx.into_result((struct_repr, params))
    }
}

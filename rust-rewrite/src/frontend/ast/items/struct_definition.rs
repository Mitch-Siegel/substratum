use crate::{
    frontend::ast::{
        generics, midend, sourceloc, types::TypeNoBoundsTree, Ast, Display, IdentifierTree,
        NameReflectable, OptionalGenericParamsListTree, ReflectName, TypeTree,
    },
    midend::{
        symtab,
        treewalk::{self, PathedCtxTrait, PathedLinearizeCtxTrait},
    },
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct StructFieldTree {
    pub name: IdentifierTree,
    pub(crate) type_: TypeTree,
}

impl Ast for StructFieldTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc().merge(&self.type_.loc()).unwrap()
    }
}

impl<U, P, C> treewalk::Linearize<U, P, C> for StructFieldTree
where
    U: treewalk::UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    TypeTree: treewalk::Linearize<U, P, C, Data = midend::types::Syntactic>,
    TypeNoBoundsTree: treewalk::Linearize<U, P, C>,
{
    type Data = (String, midend::types::Syntactic);
    fn linearize_inner(self, ctx: C) -> treewalk::LinearizeResult<Self::Data, U> {
        let (field_type, ctx) = self.type_.linearize(ctx)?;

        let (name, ctx) = self.name.linearize(ctx)?;
        ctx.into_result((name, field_type))
    }
}

impl Display for StructFieldTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct StructDefinitionTree {
    pub(crate) struct_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: generics::OptionalGenericParamsListTree,
    pub fields: Vec<StructFieldTree>,
    pub close_brace_loc: sourceloc::SourceSpan,
}

impl Ast for StructDefinitionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.struct_keyword_loc
            .clone()
            .merge(&self.close_brace_loc)
            .unwrap()
    }
}

impl Display for StructDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut fields = String::new();
        for field in &self.fields {
            fields += &field.to_string();
            fields += " ";
        }

        write!(f, "Struct Definition: {}: {}", self.name, fields)
    }
}

impl midend::treewalk::Collect<midend::symtab::TypePath> for StructDefinitionTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        let _struct_path = ctx.declare_type(self.name.value.clone())?;

        let struct_ctx = ctx.with_child_type(self.name.value.clone());

        self.generic_params
            .collect_symbols(struct_ctx)?
            .into_result()
    }
}

impl<U, P, C> treewalk::Linearize<U, P, C> for StructDefinitionTree
where
    U: treewalk::UnpathedLinearizeCtxTrait,
    P: symtab::Path + symtab::TypeOwner,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    generics::OptionalGenericParamsListTree: midend::treewalk::Linearize<U, P, C>,
    TypeTree: treewalk::Linearize<U, P, C, Data = midend::types::Syntactic>,
    StructFieldTree: treewalk::Linearize<
        U,
        symtab::TypePath,
        treewalk::PathedCtx<U, symtab::TypePath>,
        Data = (String, midend::types::Syntactic),
    >,
{
    type Data = (
        midend::symtab::types::StructRepr,
        midend::types::GenericParamsList,
    );
    #[tracing::instrument(skip(self, ctx), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(self, mut ctx: C) -> treewalk::LinearizeResult<Self::Data, U> {
        let struct_name: String;
        (struct_name, ctx) = self.name.linearize(ctx)?;

        let mut ctx: treewalk::PathedCtx<U, symtab::TypePath> =
            ctx.with_child_type(struct_name.clone());

        let mut fields = Vec::new();
        for field in self.fields {
            let linearized_field;
            (linearized_field, ctx) = field.linearize(ctx)?;
            fields.push(linearized_field);
        }

        // TODO: struct duplicate field error
        let struct_repr = midend::symtab::types::StructRepr::new(struct_name, fields).unwrap();

        let (params, ctx) = <OptionalGenericParamsListTree as treewalk::Linearize<
            U,
            symtab::TypePath,
            treewalk::PathedCtx<U, symtab::TypePath>,
        >>::linearize(self.generic_params, ctx)?;

        ctx.into_result((struct_repr, params))
    }
}

use frontend::ast::{self, Ast};

use crate::{
    symtab,
    treewalk::{
        Linearize, LinearizeError, LinearizeResult, PathedLinearizeCtxTrait,
        UnpathedFunctionLinearizeCtx, UnpathedLinearizeCtx, UnpathedLinearizeCtxTrait, types,
        walk_path,
    },
};

impl<P, C> Linearize<UnpathedLinearizeCtx, P, C> for ast::TypeTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
    ast::types::TypeNoBoundsTree: Linearize<UnpathedLinearizeCtx, P, C, Data = types::Syntactic>,
{
    type Data = types::Syntactic;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let (type_, ctx) = match self {
            Self::TypeNoBounds(tnb) => tnb.linearize(ctx)?,
        };

        ctx.into_result(type_)
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C> for ast::TypeTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
    ast::types::TypeNoBoundsTree:
        Linearize<UnpathedFunctionLinearizeCtx, P, C, Data = Option<types::Syntactic>>,
{
    type Data = Option<types::Syntactic>;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let (type_, ctx) = match self {
            Self::TypeNoBounds(tnb) => tnb.linearize(ctx)?,
        };

        ctx.into_result(type_)
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::types::ParenthesizedTypeTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    ast::TypeTree: Linearize<U, P, C>,
{
    type Data = <ast::types::TypeTree as Linearize<U, P, C>>::Data;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (inner, ctx) = self.inner_type.linearize(ctx)?;
        ctx.into_result(inner)
    }
}

impl<P, C> Linearize<UnpathedLinearizeCtx, P, C> for ast::types::TupleTypeTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
    ast::TypeTree: Linearize<UnpathedLinearizeCtx, P, C, Data = types::Syntactic>,
{
    type Data = <ast::TypeTree as Linearize<UnpathedLinearizeCtx, P, C>>::Data;
    fn linearize_inner(self, mut ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let mut members: Vec<Option<types::Syntactic>> = Vec::new();
        for member in self.members {
            let member_type: types::Syntactic;
            (member_type, ctx) = member.linearize(ctx)?;
            members.push(Some(member_type));
        }
        ctx.into_result(types::Syntactic::Tuple(members))
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C> for ast::types::TupleTypeTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
    ast::TypeTree: Linearize<UnpathedFunctionLinearizeCtx, P, C, Data = Option<types::Syntactic>>,
{
    type Data = <ast::TypeTree as Linearize<UnpathedFunctionLinearizeCtx, P, C>>::Data;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let mut members: Vec<Option<types::Syntactic>> = Vec::new();
        for member in self.members {
            let maybe_member_type: Option<types::Syntactic>;
            (maybe_member_type, ctx) = member.linearize(ctx)?;
            members.push(maybe_member_type);
        }
        ctx.into_result(Some(types::Syntactic::Tuple(members)))
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C> for ast::types::InferredTypeTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
{
    type Data = Option<types::Syntactic>;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        ctx.into_result(None)
    }
}

// base linearize ctx, disallow inferred types
impl<P, C> Linearize<UnpathedLinearizeCtx, P, C> for ast::types::TypeNoBoundsTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
{
    type Data = types::Syntactic;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let loc = self.loc();
        let (type_, ctx) = match self {
            Self::ParenthesizedType(p) => p.linearize(ctx)?,
            Self::TypePath(tp) => {
                let (type_, ctx) = tp.linearize(ctx)?;
                (type_, ctx)
            }
            Self::TupleType(tt) => {
                let (type_, ctx) = tt.linearize(ctx)?;
                (type_, ctx)
            }
            Self::ReferenceType(r) => {
                let (type_, ctx) = r.linearize(ctx)?;
                (type_, ctx)
            }
            Self::ArrayType(a) => {
                let (type_, ctx) = a.linearize(ctx)?;
                (type_, ctx)
            }
            Self::InferredType(_) => {
                return LinearizeResult::Err(LinearizeError::DisallowedInferredType(loc));
            }
        };

        ctx.into_result(type_)
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C> for ast::types::TypeNoBoundsTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
{
    type Data = Option<types::Syntactic>;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let (type_, ctx) = match self {
            Self::ParenthesizedType(p) => p.linearize(ctx)?,
            Self::TypePath(tp) => {
                let (type_, ctx) = tp.linearize(ctx)?;
                (Some(type_), ctx)
            }
            Self::TupleType(tt) => {
                let (type_, ctx) = tt.linearize(ctx)?;
                (type_, ctx)
            }
            Self::ReferenceType(r) => {
                let (type_, ctx) = r.linearize(ctx)?;
                (type_, ctx)
            }
            Self::ArrayType(a) => {
                let (type_, ctx) = a.linearize(ctx)?;
                (type_, ctx)
            }
            Self::InferredType(i) => i.linearize(ctx)?,
        };

        ctx.into_result(type_)
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::types::TypePath
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = types::Syntactic;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (path, ctx) = match self {
            Self::Primitive(p) => p.linearize(ctx),
            Self::ItemPath(i) => i.linearize(ctx),
        }?;

        ctx.into_result(path)
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::types::PrimitiveTypePathTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = types::Syntactic;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        ctx.into_result(self.type_)
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::types::TypeItemPathTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = types::Syntactic;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (path, ctx) = self.underlying_path.linearize(ctx)?;
        let walk_path::PathWithSegmentData { path, data } = path.into_type().unwrap();

        if !data.is_empty() {
            unimplemented!("handle monomorphizations");
        }

        let type_ = types::Syntactic::Named(path.into_iter().next_back().unwrap().to_string());

        ctx.into_result(type_)
    }
}

impl<P, C> Linearize<UnpathedLinearizeCtx, P, C> for ast::types::ReferenceTypeTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
{
    type Data = <ast::TypeTree as Linearize<UnpathedLinearizeCtx, P, C>>::Data;
    fn linearize_inner(self, mut ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let type_: types::Syntactic;
        (type_, ctx) = self.type_.linearize(ctx)?;

        let reference = types::Syntactic::Reference(self.mutability, Box::new(type_));
        ctx.into_result(reference)
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C> for ast::types::ReferenceTypeTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
{
    type Data = <ast::TypeTree as Linearize<UnpathedFunctionLinearizeCtx, P, C>>::Data;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let maybe_type: Option<types::Syntactic>;
        (maybe_type, ctx) = self.type_.linearize(ctx)?;
        let type_ = maybe_type.expect("reference types may not be '_'");

        let reference = types::Syntactic::Reference(self.mutability, Box::new(type_));
        ctx.into_result(Some(reference))
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::types::ArrayTypeTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    ast::TypeTree: Linearize<U, P, C>,
{
    type Data = <ast::TypeTree as Linearize<U, P, C>>::Data;
    fn linearize_inner(self, _ctx: C) -> LinearizeResult<Self::Data, U> {
        unimplemented!();
    }
}

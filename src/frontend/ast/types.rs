use crate::{
    frontend::ast::{
        path, sourceloc, symtab, treewalk, Ast, Display, Expression, GenericArgsListTree,
        LinearizeResult, NameReflectable, ReflectName,
    },
    midend::{self, treewalk::linearize_context::UnpathedLinearizeCtxTrait},
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) enum TypeTree {
    TypeNoBounds(TypeNoBoundsTree),
}

impl Ast for TypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::TypeNoBounds(tnb) => tnb.loc(),
        }
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for TypeTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
    TypeNoBoundsTree:
        treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C, Data = midend::types::Syntactic>,
{
    type Data = midend::types::Syntactic;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> treewalk::LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let (type_, ctx) = match self {
            Self::TypeNoBounds(tnb) => tnb.linearize(ctx)?,
        };

        ctx.into_result(type_)
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C> for TypeTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = P,
    >,
    TypeNoBoundsTree: treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        P,
        C,
        Data = Option<midend::types::Syntactic>,
    >,
{
    type Data = Option<midend::types::Syntactic>;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> treewalk::LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let (type_, ctx) = match self {
            Self::TypeNoBounds(tnb) => tnb.linearize(ctx)?,
        };

        ctx.into_result(type_)
    }
}

impl std::fmt::Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TypeNoBounds(tnb) => write!(f, "TypeNoBounds({tnb})"),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct ParenthesizedTypeTree {
    pub open_paren_loc: sourceloc::SourceSpan,
    pub inner_type: Box<TypeTree>,
    pub close_paren_loc: sourceloc::SourceSpan,
}

impl Ast for ParenthesizedTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_paren_loc
            .clone()
            .merge(&self.close_paren_loc)
            .unwrap()
    }
}

impl<U, P, C> midend::treewalk::Linearize<U, P, C> for ParenthesizedTypeTree
where
    U: UnpathedLinearizeCtxTrait,
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    TypeTree: treewalk::Linearize<U, P, C>,
{
    type Data = <TypeTree as treewalk::Linearize<U, P, C>>::Data;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (inner, ctx) = self.inner_type.linearize(ctx)?;
        ctx.into_result(inner)
    }
}

impl std::fmt::Display for ParenthesizedTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({})", self.inner_type)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct TupleTypeTree {
    pub open_paren_loc: sourceloc::SourceSpan,
    pub members: Vec<TypeTree>,
    pub close_paren_loc: sourceloc::SourceSpan,
}

impl Ast for TupleTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_paren_loc
            .clone()
            .merge(&self.close_paren_loc)
            .unwrap()
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for TupleTypeTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
    TypeTree:
        treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C, Data = midend::types::Syntactic>,
{
    type Data = <TypeTree as treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C>>::Data;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let mut members: Vec<Option<midend::types::Syntactic>> = Vec::new();
        for member in self.members {
            let member_type: midend::types::Syntactic;
            (member_type, ctx) = member.linearize(ctx)?;
            members.push(Some(member_type));
        }
        ctx.into_result(midend::types::Syntactic::Tuple(members))
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C>
    for TupleTypeTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = P,
    >,
    TypeTree: treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        P,
        C,
        Data = Option<midend::types::Syntactic>,
    >,
{
    type Data =
        <TypeTree as treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C>>::Data;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let mut members: Vec<Option<midend::types::Syntactic>> = Vec::new();
        for member in self.members {
            let maybe_member_type: Option<midend::types::Syntactic>;
            (maybe_member_type, ctx) = member.linearize(ctx)?;
            members.push(maybe_member_type);
        }
        ctx.into_result(Some(midend::types::Syntactic::Tuple(members)))
    }
}

impl std::fmt::Display for TupleTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "(")?;
        let mut first = true;
        for member in &self.members {
            if first {
                write!(f, "{member}")?;
                first = false;
            } else {
                write!(f, ", {member}")?;
            }
        }

        write!(f, ")")
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct InferredTypeTree {
    pub loc: sourceloc::SourceSpan,
}

impl Ast for InferredTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.loc.clone()
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C>
    for InferredTypeTree
where
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = P,
    >,
{
    type Data = Option<midend::types::Syntactic>;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        ctx.into_result(None)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) enum TypeNoBoundsTree {
    ParenthesizedType(ParenthesizedTypeTree),
    TypePath(TypePath),
    TupleType(TupleTypeTree),
    ReferenceType(ReferenceTypeTree),
    ArrayType(ArrayTypeTree),
    InferredType(InferredTypeTree),
}

impl Ast for TypeNoBoundsTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::ParenthesizedType(inner) => inner.loc(),
            Self::TypePath(path) => path.loc(),
            Self::TupleType(tuple) => tuple.loc(),
            Self::ReferenceType(reference) => reference.loc(),
            Self::ArrayType(array) => array.loc(),
            Self::InferredType(inferred) => inferred.loc(),
        }
    }
}

// base linearize ctx, disallow inferred types
impl<P, C> treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for TypeNoBoundsTree
where
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
{
    type Data = midend::types::Syntactic;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
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
                return LinearizeResult::Err(treewalk::LinearizeError::DisallowedInferredType(loc))
            }
        };

        ctx.into_result(type_)
    }
}

impl<P, C> treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C> for TypeNoBoundsTree
where
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = P,
    >,
{
    type Data = Option<midend::types::Syntactic>;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
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

impl std::fmt::Display for TypeNoBoundsTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ParenthesizedType(inner) => write!(f, "({inner})"),
            Self::TypePath(path) => write!(f, "{path}"),
            Self::TupleType(tuple) => write!(f, "{tuple}"),
            Self::ReferenceType(reference) => write!(f, "{reference}"),
            Self::ArrayType(array) => write!(f, "{array}"),
            Self::InferredType(_) => write!(f, "_"),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) enum TypePath {
    Primitive(PrimitiveTypePathTree),
    ItemPath(TypeItemPathTree),
}

impl Ast for TypePath {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Primitive(p) => p.loc(),
            Self::ItemPath(i) => i.loc(),
        }
    }
}

impl<U, P, C> treewalk::Linearize<U, P, C> for TypePath
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = midend::types::Syntactic;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (path, ctx) = match self {
            Self::Primitive(p) => p.linearize(ctx),
            Self::ItemPath(i) => i.linearize(ctx),
        }?;

        ctx.into_result(path)
    }
}

impl std::fmt::Display for TypePath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Primitive(primitive) => write!(f, "{primitive}"),
            Self::ItemPath(item) => write!(f, "{item}"),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct PrimitiveTypePathTree {
    pub loc: sourceloc::SourceSpan,
    pub(crate) type_: midend::types::Syntactic,
}

impl Ast for PrimitiveTypePathTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.loc.clone()
    }
}

impl<U, P, C> treewalk::Linearize<U, P, C> for PrimitiveTypePathTree
where
    U: UnpathedLinearizeCtxTrait,
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = midend::types::Syntactic;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        ctx.into_result(self.type_)
    }
}

impl std::fmt::Display for PrimitiveTypePathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) enum TypePathSegmentData {
    GenericArgs(GenericArgsListTree),
}

impl Ast for TypePathSegmentData {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::GenericArgs(g) => g.loc(),
        }
    }
}

impl Display for TypePathSegmentData {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::GenericArgs(generics) => write!(f, "<{generics}>"),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct TypeItemPathTree {
    pub underlying_path: path::PathTree<TypePathSegmentData>,
}

impl Ast for TypeItemPathTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.underlying_path.loc()
    }
}

impl<U, P, C> midend::treewalk::Linearize<U, P, C> for TypeItemPathTree
where
    U: UnpathedLinearizeCtxTrait,
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = midend::types::Syntactic;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (path, ctx) = self.underlying_path.linearize(ctx)?;
        let (type_path, path_data) = path.into_type().unwrap();

        if !path_data.is_empty() {
            unimplemented!("handle monomorphizations");
        }

        let type_ =
            midend::types::Syntactic::Named(type_path.into_iter().last().unwrap().to_string());

        ctx.into_result(type_)
    }
}

impl std::fmt::Display for TypeItemPathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.underlying_path)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct ReferenceTypeTree {
    pub reference_token_loc: sourceloc::SourceSpan,
    pub mutability: midend::types::Mutability,
    pub(crate) type_: Box<TypeNoBoundsTree>,
}

impl Ast for ReferenceTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.reference_token_loc
            .clone()
            .merge(&self.type_.loc())
            .unwrap()
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for ReferenceTypeTree
where
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
{
    type Data = <TypeTree as treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C>>::Data;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let type_: midend::types::Syntactic;
        (type_, ctx) = self.type_.linearize(ctx)?;

        let reference = midend::types::Syntactic::Reference(self.mutability, Box::new(type_));
        ctx.into_result(reference)
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C>
    for ReferenceTypeTree
where
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = P,
    >,
{
    type Data =
        <TypeTree as treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C>>::Data;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let maybe_type: Option<midend::types::Syntactic>;
        (maybe_type, ctx) = self.type_.linearize(ctx)?;
        let type_ = maybe_type.expect("reference types may not be '_'");

        let reference = midend::types::Syntactic::Reference(self.mutability, Box::new(type_));
        ctx.into_result(Some(reference))
    }
}

impl std::fmt::Display for ReferenceTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "&{} {}", self.mutability, self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct ArrayTypeTree {
    pub open_bracket_loc: sourceloc::SourceSpan,
    pub inner_type: Box<TypeTree>,
    pub array_size: Expression,
    pub close_bracket_loc: sourceloc::SourceSpan,
}

impl Ast for ArrayTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        unimplemented!();
    }
}

impl<U, P, C> midend::treewalk::Linearize<U, P, C> for ArrayTypeTree
where
    U: UnpathedLinearizeCtxTrait,
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    TypeTree: treewalk::Linearize<U, P, C>,
{
    type Data = <TypeTree as treewalk::Linearize<U, P, C>>::Data;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        unimplemented!();
    }
}

impl std::fmt::Display for ArrayTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "")
    }
}

use crate::{
    frontend::ast::*,
    midend::{self, symtab::Path},
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum TypeTree {
    TypeNoBounds(TypeNoBoundsTree),
}

impl Ast for TypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::TypeNoBounds(tnb) => tnb.loc(),
        }
    }
}

impl midend::treewalk::Linearize for TypeTree {
    type Data = Option<midend::types::Syntactic>;
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        match self {
            Self::TypeNoBounds(tnb) => tnb.linearize(ctx),
        }
    }
}

impl std::fmt::Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TypeNoBounds(tnb) => write!(f, "TypeNoBounds({})", tnb),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ParenthesizedTypeTree {
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

impl midend::treewalk::Linearize for ParenthesizedTypeTree {
    type Data = Option<midend::types::Syntactic>;
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        self.inner_type.linearize(ctx)
    }
}

impl std::fmt::Display for ParenthesizedTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({})", self.inner_type)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct TupleTypeTree {
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

impl midend::treewalk::Linearize for TupleTypeTree {
    type Data = midend::types::Syntactic;
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let mut members = Vec::new();
        for member in self.members {
            let maybe_member_type;
            (maybe_member_type, ctx) = member.linearize_same_path(ctx)?;
            let member_type = maybe_member_type.expect("tuple types may not be '_'");
            members.push(member_type);
        }
        ctx.into_result(midend::types::Syntactic::Tuple(members))
    }
}

impl std::fmt::Display for TupleTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "(")?;
        let mut first = true;
        for member in &self.members {
            if first {
                write!(f, "{}", member)?;
                first = false;
            } else {
                write!(f, ", {}", member)?;
            }
        }

        write!(f, ")")
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct InferredTypeTree {
    pub loc: sourceloc::SourceSpan,
}

impl Ast for InferredTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.loc.clone()
    }
}

impl midend::treewalk::Linearize for InferredTypeTree {
    type Data = Option<midend::types::Syntactic>;
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        ctx.into_result(None)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum TypeNoBoundsTree {
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

impl midend::treewalk::Linearize for TypeNoBoundsTree {
    type Data = Option<midend::types::Syntactic>;
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let (type_, ctx) = match self {
            Self::ParenthesizedType(p) => p.linearize(ctx)?,
            Self::TypePath(tp) => {
                let (type_, ctx) = tp.linearize(ctx)?;
                (Some(type_), ctx)
            }
            Self::TupleType(tt) => {
                let (type_, ctx) = tt.linearize(ctx)?;
                (Some(type_), ctx)
            }
            Self::ReferenceType(r) => {
                let (type_, ctx) = r.linearize(ctx)?;
                (Some(type_), ctx)
            }
            Self::ArrayType(a) => {
                let (type_, ctx) = a.linearize(ctx)?;
                (Some(type_), ctx)
            }
            Self::InferredType(i) => i.linearize(ctx)?,
        };

        ctx.into_result(type_)
    }
}

impl std::fmt::Display for TypeNoBoundsTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ParenthesizedType(inner) => write!(f, "({})", inner),
            Self::TypePath(path) => write!(f, "{}", path),
            Self::TupleType(tuple) => write!(f, "{}", tuple),
            Self::ReferenceType(reference) => write!(f, "{}", reference),
            Self::ArrayType(array) => write!(f, "{}", array),
            Self::InferredType(_) => write!(f, "_"),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum TypePath {
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

impl midend::treewalk::Linearize for TypePath {
    type Data = midend::types::Syntactic;
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        match self {
            Self::Primitive(p) => p.linearize(ctx),
            Self::ItemPath(i) => i.linearize(ctx),
        }
    }
}

impl std::fmt::Display for TypePath {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Primitive(primitive) => write!(f, "{}", primitive),
            Self::ItemPath(item) => write!(f, "{}", item),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct PrimitiveTypePathTree {
    pub loc: sourceloc::SourceSpan,
    pub type_: midend::types::Syntactic,
}

impl Ast for PrimitiveTypePathTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.loc.clone()
    }
}

impl midend::treewalk::Linearize for PrimitiveTypePathTree {
    type Data = midend::types::Syntactic;
    fn linearize(
        self,
        _ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        unimplemented!();
    }
}

impl std::fmt::Display for PrimitiveTypePathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum TypePathSegmentData {
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
            Self::GenericArgs(generics) => write!(f, "<{}>", generics),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct TypeItemPathTree {
    pub underlying_path: path::PathTree<TypePathSegmentData>,
}

impl Ast for TypeItemPathTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.underlying_path.loc()
    }
}

impl midend::treewalk::Linearize for TypeItemPathTree {
    type Data = midend::types::Syntactic;
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let (path, ctx) = self.underlying_path.linearize(ctx)?;
        let (type_path, path_data) = path.as_type().unwrap();

        for (path, segment_data) in path_data {
            unimplemented!(
                "handle monomorphization for path {} data {}",
                path,
                segment_data
            );
        }

        let type_ = midend::types::Syntactic::Named(type_path.last().raw().into());

        ctx.into_result(type_)
    }
}

impl std::fmt::Display for TypeItemPathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.underlying_path)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ReferenceTypeTree {
    pub reference_token_loc: sourceloc::SourceSpan,
    pub mutability: midend::types::Mutability,
    pub type_: Box<TypeNoBoundsTree>,
}

impl Ast for ReferenceTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.reference_token_loc
            .clone()
            .merge(&self.type_.loc())
            .unwrap()
    }
}

impl midend::treewalk::Linearize for ReferenceTypeTree {
    type Data = midend::types::Syntactic;
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let maybe_type;
        (maybe_type, ctx) = self.type_.linearize_same_path(ctx)?;
        let type_ = maybe_type.expect("reference types may not be '_'");

        let reference = midend::types::Syntactic::Reference(self.mutability, Box::new(type_));
        ctx.into_result(reference)
    }
}

impl std::fmt::Display for ReferenceTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "&{} {}", self.mutability, self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ArrayTypeTree {
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

impl midend::treewalk::Linearize for ArrayTypeTree {
    type Data = midend::types::Syntactic;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        unimplemented!();
    }
}

impl std::fmt::Display for ArrayTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "")
    }
}

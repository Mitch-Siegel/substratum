use crate::frontend::ast::*;

use super::expressions::PathIdentSegment;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[enum_delegate::implement(Ast)]
pub enum TypeTree {
    TypeNoBounds(TypeNoBoundsTree),
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

impl treewalk::Linearize<Option<midend::types::Syntactic>> for ParenthesizedTypeTree {
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> Option<midend::types::Syntactic> {
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

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[enum_delegate::implement(Ast)]
pub enum TypeNoBoundsTree {
    ParenthesizedType(ParenthesizedTypeTree),
    TypePath(TypePath),
    TupleType(TupleTypeTree),
    ReferenceType(ReferenceTypeTree),
    ArrayType(ArrayTypeTree),
    InferredType(InferredTypeTree),
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

impl treewalk::Linearize<Option<midend::types::Syntactic>> for TypeNoBoundsTree {
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> Option<midend::types::Syntactic> {
        match self {
            Self::ParenthesizedType(p) => p.linearize(ctx),
            Self::TypePath(tp) => unimplemented!(),
            Self::TupleType(tt) => unimplemented!(),
            Self::ReferenceType(r) => unimplemented!(),
            Self::ArrayType(a) => unimplemented!(),
            Self::InferredType(i) => unimplemented!(),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[enum_delegate::implement(Ast)]
pub enum TypePath {
    Primitive(PrimitiveTypePathTree),
    ItemPath(TypeItemPathTree),
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

impl std::fmt::Display for PrimitiveTypePathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct TypeItemPathTree {
    pub starts_global: Option<sourceloc::SourceSpan>,
    pub segments: Vec<TypePathSegmentTree>,
}

impl Ast for TypeItemPathTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut segments = self.segments.iter();
        let mut loc = if let Some(global_path_sep_loc) = &self.starts_global {
            assert!(
                segments.size_hint().0 > 0,
                "TypeItemPathTree must have at least one segment"
            );
            global_path_sep_loc.clone()
        } else {
            segments
                .next()
                .expect("TypeItemPathTree must have at least one segment")
                .loc()
        };

        while let Some(segment) = segments.next() {
            loc = loc.merge(&segment.loc()).unwrap();
        }

        loc
    }
}

impl std::fmt::Display for TypeItemPathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for segment in &self.segments {
            if !first || self.starts_global.is_some() {
                write!(f, "::{}", segment)?;
            } else {
                first = false;
                write!(f, "{}", segment)?;
            }
        }
        Ok(())
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct TypePathSegmentTree {
    pub ident_segment: PathIdentSegment,
    pub generic_args: Option<generics::GenericArgsListTree>,
}

impl TypePathSegmentTree {
    pub fn loc(&self) -> sourceloc::SourceSpan {
        self.ident_segment.loc()
    }
}

impl std::fmt::Display for TypePathSegmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.ident_segment)?;
        match &self.generic_args {
            Some(generics) => write!(f, "::{}", generics),
            None => Ok(()),
        }
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

impl std::fmt::Display for ReferenceTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "&{} {}", self.mutability, self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ArrayTypeTree {}

impl Ast for ArrayTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        unimplemented!();
    }
}

impl std::fmt::Display for ArrayTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "")
    }
}

impl treewalk::Linearize<Option<midend::types::Syntactic>> for TypeTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> Option<midend::types::Syntactic> {
        match self {
            Self::TypeNoBounds(tnb) => tnb.linearize(ctx),
        }
    }
}

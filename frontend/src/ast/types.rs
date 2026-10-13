use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Display, Expression, GenericArgsListTree, loc, path},
    types,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum TypeTree {
    TypeNoBounds(TypeNoBoundsTree),
}

impl Ast for TypeTree {
    fn span(&self) -> loc::Span {
        match self {
            Self::TypeNoBounds(tnb) => tnb.span(),
        }
    }
}

impl std::fmt::Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TypeNoBounds(tnb) => write!(f, "TypeNoBounds({tnb})"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ParenthesizedTypeTree {
    pub(crate) open_paren: loc::Span,
    pub inner_type: Box<TypeTree>,
    pub(crate) close_paren: loc::Span,
}

impl Ast for ParenthesizedTypeTree {
    fn span(&self) -> loc::Span {
        self.open_paren.clone().merge(&self.close_paren).unwrap()
    }
}

impl std::fmt::Display for ParenthesizedTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({})", self.inner_type)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TupleTypeTree {
    pub(crate) open_paren: loc::Span,
    pub members: Vec<TypeTree>,
    pub(crate) close_paren: loc::Span,
}

impl Ast for TupleTypeTree {
    fn span(&self) -> loc::Span {
        self.open_paren.clone().merge(&self.close_paren).unwrap()
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

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct InferredTypeTree {
    span: loc::Span,
}

impl Ast for InferredTypeTree {
    fn span(&self) -> loc::Span {
        self.span.clone()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum TypeNoBoundsTree {
    ParenthesizedType(ParenthesizedTypeTree),
    TypePath(TypePath),
    TupleType(TupleTypeTree),
    ReferenceType(ReferenceTypeTree),
    ArrayType(ArrayTypeTree),
    InferredType(InferredTypeTree),
}

impl Ast for TypeNoBoundsTree {
    fn span(&self) -> loc::Span {
        match self {
            Self::ParenthesizedType(inner) => inner.span(),
            Self::TypePath(path) => path.span(),
            Self::TupleType(tuple) => tuple.span(),
            Self::ReferenceType(reference) => reference.span(),
            Self::ArrayType(array) => array.span(),
            Self::InferredType(inferred) => inferred.span(),
        }
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

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum TypePath {
    Primitive(PrimitiveTypePathTree),
    ItemPath(TypeItemPathTree),
}

impl Ast for TypePath {
    fn span(&self) -> loc::Span {
        match self {
            Self::Primitive(p) => p.span(),
            Self::ItemPath(i) => i.span(),
        }
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

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PrimitiveTypePathTree {
    pub(crate) span: loc::Span,
    pub type_: types::Syntactic,
}

impl Ast for PrimitiveTypePathTree {
    fn span(&self) -> loc::Span {
        self.span.clone()
    }
}

impl std::fmt::Display for PrimitiveTypePathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum TypePathSegmentData {
    GenericArgs(GenericArgsListTree),
}

impl Ast for TypePathSegmentData {
    fn span(&self) -> loc::Span {
        match self {
            Self::GenericArgs(g) => g.span(),
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

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TypeItemPathTree {
    pub underlying_path: path::PathTree<TypePathSegmentData>,
}

impl Ast for TypeItemPathTree {
    fn span(&self) -> loc::Span {
        self.underlying_path.span()
    }
}

impl std::fmt::Display for TypeItemPathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.underlying_path)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ReferenceTypeTree {
    pub(crate) reference_token: loc::Span,
    pub mutability: types::Mutability,
    pub type_: Box<TypeNoBoundsTree>,
}

impl Ast for ReferenceTypeTree {
    fn span(&self) -> loc::Span {
        self.reference_token
            .clone()
            .merge(&self.type_.span())
            .unwrap()
    }
}

impl std::fmt::Display for ReferenceTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "&{} {}", self.mutability, self.type_)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ArrayTypeTree {
    pub(crate) open_bracket: loc::Span,
    pub inner_type: Box<TypeTree>,
    pub array_size: Expression,
    pub(crate) close_bracket: loc::Span,
}

impl Ast for ArrayTypeTree {
    fn span(&self) -> loc::Span {
        unimplemented!();
    }
}

impl std::fmt::Display for ArrayTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "")
    }
}

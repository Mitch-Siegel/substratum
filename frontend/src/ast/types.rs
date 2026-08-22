use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Display, Expression, GenericArgsListTree, path, sourceloc},
    types,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
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

impl std::fmt::Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TypeNoBounds(tnb) => write!(f, "TypeNoBounds({tnb})"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ParenthesizedTypeTree {
    pub(crate) open_paren_loc: sourceloc::SourceSpan,
    pub inner_type: Box<TypeTree>,
    pub(crate) close_paren_loc: sourceloc::SourceSpan,
}

impl Ast for ParenthesizedTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_paren_loc
            .clone()
            .merge(&self.close_paren_loc)
            .unwrap()
    }
}

impl std::fmt::Display for ParenthesizedTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({})", self.inner_type)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TupleTypeTree {
    pub(crate) open_paren_loc: sourceloc::SourceSpan,
    pub members: Vec<TypeTree>,
    pub(crate) close_paren_loc: sourceloc::SourceSpan,
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
    loc: sourceloc::SourceSpan,
}

impl Ast for InferredTypeTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.loc.clone()
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
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Primitive(p) => p.loc(),
            Self::ItemPath(i) => i.loc(),
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
    pub(crate) loc: sourceloc::SourceSpan,
    pub type_: types::Syntactic,
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

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
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
            Self::GenericArgs(generics) => write!(f, "<{generics}>"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TypeItemPathTree {
    pub underlying_path: path::PathTree<TypePathSegmentData>,
}

impl Ast for TypeItemPathTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.underlying_path.loc()
    }
}

impl std::fmt::Display for TypeItemPathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.underlying_path)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ReferenceTypeTree {
    pub(crate) reference_token_loc: sourceloc::SourceSpan,
    pub mutability: types::Mutability,
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

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ArrayTypeTree {
    pub(crate) open_bracket_loc: sourceloc::SourceSpan,
    pub inner_type: Box<TypeTree>,
    pub array_size: Expression,
    pub(crate) close_bracket_loc: sourceloc::SourceSpan,
}

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

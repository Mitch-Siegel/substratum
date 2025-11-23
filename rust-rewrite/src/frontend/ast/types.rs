use crate::frontend::ast::*;

use super::expressions::PathIdentSegment;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum TypeTree {
    TypeNoBounds(TypeNoBounds),
}

impl std::fmt::Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::TypeNoBounds(tnb) => write!(f, "TypeNoBounds({})", tnb),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum TypeNoBounds {
    ParenthesizedType(Box<TypeTree>),
    TypePath(TypePath),
    TupleType(Vec<TypeTree>),
    ReferenceType(ReferenceTypeTree),
    ArrayType(ArrayTypeTree),
    InferredType(SourceLoc),
}

impl std::fmt::Display for TypeNoBounds {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::ParenthesizedType(inner) => write!(f, "({})", inner),
            Self::TypePath(path) => write!(f, "{}", path),
            Self::TupleType(tuple) => {
                let mut first = true;
                for member in tuple {
                    if !first {
                        write!(f, ", {}", member)?;
                    } else {
                        first = false;
                        write!(f, "{}", member)?;
                    }
                }
                Ok(())
            }
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
    pub loc: SourceLoc,
    pub type_: midend::types::Syntactic,
}

impl std::fmt::Display for PrimitiveTypePathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct TypeItemPathTree {
    pub loc: SourceLoc,
    pub starts_global: bool,
    pub segments: Vec<TypePathSegmentTree>,
}

impl std::fmt::Display for TypeItemPathTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for segment in &self.segments {
            if !first || self.starts_global {
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
    pub fn loc(&self) -> &SourceLoc {
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
    pub loc: SourceLoc,
    pub mutability: midend::types::Mutability,
    pub type_: Box<TypeNoBounds>,
}

impl std::fmt::Display for ReferenceTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "&{} {}", self.mutability, self.type_)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ArrayTypeTree {}

impl std::fmt::Display for ArrayTypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "")
    }
}

impl treewalk::Linearize<midend::types::Syntactic> for TypeTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::types::Syntactic {
        unimplemented!();
    }
}

use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, IdentifierTree},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum IdentSegment {
    Ident(IdentifierTree),
    Super(sourceloc::SourceSpan),
    SelfLower(sourceloc::SourceSpan),
    SelfUpper(sourceloc::SourceSpan),
}

impl Ast for IdentSegment {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Ident(ident) => ident.loc(),
            Self::Super(loc) | Self::SelfUpper(loc) | Self::SelfLower(loc) => loc.clone(),
        }
    }
}

impl fmt::Display for IdentSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Ident(ident) => write!(f, "{ident}"),
            Self::Super(_) => write!(f, "super"),
            Self::SelfLower(_) => write!(f, "self"),
            Self::SelfUpper(_) => write!(f, "Self"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathSegmentTree<T>
where
    T: Ast,
{
    pub ident: IdentSegment,
    pub data: Option<T>,
}

impl<T> Ast for PathSegmentTree<T>
where
    T: Ast,
{
    fn loc(&self) -> sourceloc::SourceSpan {
        match &self.data {
            Some(data) => self.ident.loc().merge(&data.loc()).unwrap(),
            None => self.ident.loc(),
        }
    }
}

impl<T> fmt::Display for PathSegmentTree<T>
where
    T: Ast + fmt::Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.ident)?;
        if let Some(args) = &self.data {
            write!(f, "<{args}>")?;
        }
        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathTree<T>
where
    T: Ast,
{
    pub starts_global: Option<sourceloc::SourceSpan>,
    pub segments: Vec<PathSegmentTree<T>>,
}

impl<T> Ast for PathTree<T>
where
    T: Ast,
{
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut seg_iter = self.segments.iter();
        let mut loc_span = seg_iter
            .next()
            .expect("PathTree must have at least one segment")
            .loc();

        for segment in seg_iter {
            loc_span = loc_span.merge(&segment.loc()).unwrap();
        }

        loc_span
    }
}

impl<T> fmt::Display for PathTree<T>
where
    T: Ast + fmt::Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.starts_global.is_some() {
            write!(f, "::")?;
        }

        let mut first = true;
        for segment in &self.segments {
            if first {
                write!(f, "{segment}")?;
                first = false;
            } else {
                write!(f, "::{segment}")?;
            }
        }

        Ok(())
    }
}

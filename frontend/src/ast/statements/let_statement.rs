use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, Expression, IdentifierTree, TypeTree},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct LetTree {
    pub(crate) let_keyword: loc::Span,
    pub name: IdentifierTree,
    pub(crate) type_: Option<TypeTree>,
    pub mutable: bool,
    pub value: Expression,
}

impl Ast for LetTree {
    fn span(&self) -> loc::Span {
        self.let_keyword.clone().merge(&self.value.span()).unwrap()
    }
}

impl fmt::Display for LetTree {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self.type_ {
            Some(type_) => write!(f, "let {}: {}", self.name, type_)?,
            None => write!(f, "let {}", self.name)?,
        }

        if self.mutable {
            write!(f, "mut ")?;
        }

        match &self.type_ {
            Some(type_) => write!(f, ": {type_}"),
            None => write!(f, ": ?"),
        }
    }
}

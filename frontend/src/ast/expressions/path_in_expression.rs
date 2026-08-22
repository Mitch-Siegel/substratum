use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, generics, path},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathInExpressionTree {
    pub underlying_path: path::PathTree<generics::GenericArgsListTree>,
}

impl Ast for PathInExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.underlying_path.loc()
    }
}

impl fmt::Display for PathInExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.underlying_path)
    }
}

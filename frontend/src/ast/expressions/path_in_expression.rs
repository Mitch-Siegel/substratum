use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, generics, path},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathInExpressionTree {
    pub underlying_path: path::PathTree<generics::GenericArgsListTree>,
}

impl Ast for PathInExpressionTree {
    fn span(&self) -> loc::Span {
        self.underlying_path.span()
    }
}

impl fmt::Display for PathInExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.underlying_path)
    }
}

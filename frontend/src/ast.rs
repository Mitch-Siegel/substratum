use std::fmt::Display;

use serde::{Deserialize, Serialize};

use crate::sourceloc;

pub mod expressions;
pub mod generics;
pub mod items;
pub mod module;
pub mod path;
pub mod statements;
pub mod types;

pub use expressions::Expression;
pub use generics::*;
pub use items::ItemTree;
pub use module::ModuleTree;
pub use path::PathTree;
pub use statements::StatementTree;
pub use types::TypeTree;

#[cfg(test)]
pub(crate) mod builder;

pub trait Ast {
    fn loc(&self) -> sourceloc::SourceSpan;

    #[must_use]
    fn name() -> &'static str {
        std::any::type_name::<Self>()
            .split("::")
            .last()
            .unwrap_or("unknown ast type")
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct IdentifierTree {
    pub loc: sourceloc::SourceSpan,
    pub value: String,
}

impl Ast for IdentifierTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.loc.clone()
    }
}

impl std::fmt::Display for IdentifierTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.value)
    }
}

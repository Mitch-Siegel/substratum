use std::fmt::Display;

use serde::{Deserialize, Serialize};

use crate::loc;

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
    fn span(&self) -> loc::Span;

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
    pub span: loc::Span,
    pub value: String,
}

impl Ast for IdentifierTree {
    fn span(&self) -> loc::Span {
        self.span.clone()
    }
}

impl std::fmt::Display for IdentifierTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.value)
    }
}

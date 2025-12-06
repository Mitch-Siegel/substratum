use crate::{
    frontend::*,
    midend::{self, treewalk},
};
use std::fmt::Display;

use name_derive::{NameReflectable, ReflectName};

pub mod expressions;
pub mod generics;
pub mod items;
pub mod module;
pub mod statements;
pub mod types;

pub use expressions::Expression;
pub use items::ItemTree;
pub use module::ModuleTree;
pub use statements::StatementTree;
pub use types::TypeTree;

#[cfg(test)]
pub mod builder;

#[enum_delegate::register]
pub trait Ast {
    fn loc(&self) -> sourceloc::SourceSpan;
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
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

impl treewalk::Linearize<String> for IdentifierTree {
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> String {
        self.value
    }
}

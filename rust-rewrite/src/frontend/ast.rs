use crate::{frontend::*, midend};
use std::fmt::Display;

use name_derive::{NameReflectable, ReflectName};

pub mod expressions;
pub mod generics;
pub mod items;
pub mod module;
pub mod statements;
pub mod types;

pub use expressions::Expression;
pub use generics::*;
pub use items::ItemTree;
pub use module::ModuleTree;
pub use statements::StatementTree;
pub use types::TypeTree;

#[cfg(test)]
pub mod builder;

pub trait Ast<LinearizeResult>: midend::treewalk::Treewalk<LinearizeResult> {
    fn loc(&self) -> sourceloc::SourceSpan;
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct IdentifierTree {
    pub loc: sourceloc::SourceSpan,
    pub value: String,
}

impl Ast<String> for IdentifierTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.loc.clone()
    }
}

impl midend::treewalk::Treewalk<String> for IdentifierTree {
    fn linearize(self, _ctx: &mut midend::treewalk::LinearizeCtx) -> String {
        self.value
    }
}

impl std::fmt::Display for IdentifierTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.value)
    }
}

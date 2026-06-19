use crate::{frontend::*, midend};
use std::fmt::Display;

use name_derive::{NameReflectable, ReflectName};

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
pub use statements::StatementTree;
pub use types::TypeTree;

#[cfg(test)]
pub mod builder;

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

impl<P> midend::treewalk::Linearize<P> for IdentifierTree
where
    P: midend::symtab::Path,
{
    type Data = String;
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx<P>,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        ctx.into_result(self.value)
    }
}

impl std::fmt::Display for IdentifierTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.value)
    }
}

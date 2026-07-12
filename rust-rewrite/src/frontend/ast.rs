use crate::{
    frontend::*,
    midend::{
        self, symtab,
        treewalk::{self, LinearizeResult},
    },
};
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

impl<U, P, C> midend::treewalk::Linearize<U, P, C> for IdentifierTree
where
    U: treewalk::UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = String;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        ctx.into_result(self.value)
    }
}

impl std::fmt::Display for IdentifierTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.value)
    }
}

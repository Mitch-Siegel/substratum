use crate::{
    frontend::sourceloc,
    midend::{
        self, symtab,
        treewalk::{self, LinearizeResult},
    },
};
use std::fmt::Display;

use name_derive::{NameReflectable, ReflectName};

pub(crate) mod expressions;
pub(crate) mod generics;
pub(crate) mod items;
pub(crate) mod module;
pub(crate) mod path;
pub(crate) mod statements;
pub(crate) mod types;

pub(crate) use expressions::Expression;
pub(crate) use generics::*;
pub(crate) use items::ItemTree;
pub(crate) use module::ModuleTree;
pub(crate) use statements::StatementTree;
pub(crate) use types::TypeTree;

#[cfg(test)]
pub(crate) mod builder;

pub(crate) trait Ast {
    fn loc(&self) -> sourceloc::SourceSpan;
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct IdentifierTree {
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

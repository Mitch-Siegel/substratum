use crate::{frontend::sourceloc::SourceLocWithMod, midend};
use std::fmt::Display;

use name_derive::{NameReflectable, ReflectName};

pub mod expressions;
pub mod generics;
pub mod items;
pub mod module;
pub mod statements;
pub mod types;

pub use expressions::{Expression, ExpressionTree};
pub use items::Item;
pub use module::ModuleTree;
pub use statements::StatementTree;

pub trait AstName {
    fn ast_name(&self) -> String;
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ItemTree {
    pub loc: SourceLocWithMod,
}

impl Display for ItemTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.loc)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct TypeTree {
    pub loc: SourceLocWithMod,
    pub type_: midend::types::Type,
}
impl TypeTree {
    pub fn new(loc: SourceLocWithMod, type_: midend::types::Type) -> Self {
        Self { loc, type_ }
    }
}
impl Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

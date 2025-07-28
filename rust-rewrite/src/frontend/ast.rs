use crate::{
    frontend::sourceloc::SourceLocWithMod,
    midend::{
        self,
        linearizer::{
            def_context::DefContext, CustomReturnWalk, FunctionWalkContext, ReturnFunctionWalk,
            ReturnWalk, ValueWalk,
        },
    },
};
use std::fmt::Display;

use name_derive::{NameReflectable, ReflectName};

pub mod expressions;
pub mod generics;
pub mod items;
pub mod module;
pub mod statements;
pub mod types;

pub use expressions::{Expression, ExpressionTree};
pub use items::{Item, ItemTree};
pub use module::ModuleTree;
pub use statements::StatementTree;
pub use types::TypeTree;

pub trait AstName {
    fn ast_name(&self) -> String;
}

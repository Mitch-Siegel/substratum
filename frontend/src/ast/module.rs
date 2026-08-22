use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, IdentifierTree, ItemTree},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ModuleTree {
    pub module_path: Vec<String>,
    pub(crate) mod_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub items: Vec<ItemTree>,
}

impl Ast for ModuleTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = self
            .mod_keyword_loc
            .clone()
            .merge(&self.name.loc())
            .unwrap();

        for item in &self.items {
            loc = loc.merge(&item.loc()).unwrap();
        }

        loc
    }
}

impl fmt::Display for ModuleTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "Module {}", self.name)?;
        for item in &self.items {
            writeln!(f, " - {item}")?;
        }
        Ok(())
    }
}

impl Ord for ModuleTree {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.module_path.cmp(&other.module_path)
    }
}

impl PartialOrd for ModuleTree {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

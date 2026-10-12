use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, IdentifierTree, ItemTree},
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ModuleTree {
    pub module_path: Vec<String>,
    pub(crate) maybe_mod_keyword_and_name: Option<(sourceloc::SourceSpan, IdentifierTree)>,
    pub name: String,
    pub items: Vec<ItemTree>,
}

impl Ast for ModuleTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = None;

        if let Some((mod_kw_loc, name)) = &self.maybe_mod_keyword_and_name {
            loc.replace(mod_kw_loc.clone().merge(&name.loc()).unwrap());
        }

        for item in &self.items {
            let new_loc = match &loc {
                Some(existing) => existing.clone().merge(&item.loc()).unwrap(),
                None => item.loc(),
            };
            loc.replace(new_loc);
        }

        loc.expect("module with no items")
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

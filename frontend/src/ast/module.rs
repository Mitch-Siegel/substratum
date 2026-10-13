use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{Ast, IdentifierTree, ItemTree},
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ModuleTree {
    pub module_path: Vec<String>,
    pub(crate) maybe_mod_keyword_and_name: Option<(loc::Span, IdentifierTree)>,
    pub name: String,
    pub items: Vec<ItemTree>,
}

impl Ast for ModuleTree {
    fn span(&self) -> loc::Span {
        let mut loc = None;

        if let Some((mod_kw_loc, name)) = &self.maybe_mod_keyword_and_name {
            loc.replace(mod_kw_loc.clone().merge(&name.span()).unwrap());
        }

        for item in &self.items {
            let new_loc = match &loc {
                Some(existing) => existing.clone().merge(&item.span()).unwrap(),
                None => item.span(),
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

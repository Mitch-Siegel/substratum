use std::{collections::BTreeSet, path};

pub mod ast;
pub(crate) mod lexer;
pub(crate) mod parser;
pub mod sourceloc;

pub(crate) use lexer::Lexer;

pub mod types;

// return result to match other parse function conventions
/// # Panics
///
/// when any error is generated during parsing
#[allow(clippy::unnecessary_wraps)]
#[must_use]
pub fn parse_crate(
    crate_name: &str,
    bin_name: &str,
    crate_path: &path::Path,
) -> BTreeSet<ast::ModuleTree> {
    let mut modules = BTreeSet::new();
    let mut worklist = BTreeSet::<parser::WorklistItem>::new();

    let parser::ModuleResult {
        module_tree,
        module_worklist,
    } = parser::find_and_parse_module(
        parser::WorklistItem::new(String::from(bin_name), vec![]),
        crate_name,
        crate_path,
        false,
    )
    .expect("error parsing crate root ");
    modules.insert(module_tree);
    worklist.extend(module_worklist);

    while let Some(worklist_item) = worklist.pop_last() {
        let module_name = worklist_item.module_name.clone();
        let parser::ModuleResult {
            module_tree,
            mut module_worklist,
        } = parser::find_and_parse_module(worklist_item, crate_name, crate_path, true)
            .unwrap_or_else(|e| panic!("Error in file {module_name}: {e}"));

        worklist.append(&mut module_worklist);

        assert!(modules.insert(module_tree));
    }

    modules
}

#[cfg(test)]
mod tests {
    use super::*;
}

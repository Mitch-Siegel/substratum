use treewalk::ModuleWalk;

use crate::{frontend::ast::TranslationUnitTree, midend::symtab};

use super::symtab::SymbolTable;

pub mod functionwalkcontext;
pub mod modulewalkcontext;
mod treewalk;
pub mod walkcontext;

pub use functionwalkcontext::FunctionWalkContext;
pub use modulewalkcontext::ModuleWalkContext;

pub fn linearize(program: Vec<TranslationUnitTree>) -> symtab::SymbolTable {
    let mut context = ModuleWalkContext::new();
    for translation_unit in program {
        translation_unit.walk(&mut context);
    }

    context.into()
}

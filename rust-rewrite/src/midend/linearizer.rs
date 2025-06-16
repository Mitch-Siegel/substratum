use treewalk::ModuleWalk;

use crate::{frontend::ast::TranslationUnitTree, midend::symtab};

mod block_convergences;
mod block_manager;
mod functionwalkcontext;
mod modulewalkcontext;
mod treewalk;
pub mod walkcontext;

use block_convergences::*;
pub use block_manager::BlockManager;
pub use functionwalkcontext::FunctionWalkContext;
pub use modulewalkcontext::ModuleWalkContext;

pub fn linearize(program: Vec<TranslationUnitTree>) -> symtab::SymbolTable {
    let mut context = ModuleWalkContext::new();
    for translation_unit in program {
        translation_unit.walk(&mut context);
    }

    context.into()
}

use treewalk::ModuleWalk;

use crate::{frontend::ast::TranslationUnitTree, midend::symtab};

mod block_convergences;
mod block_manager;
mod functionwalkcontext;
mod treewalk;

use block_convergences::*;
pub use block_manager::BlockManager;
pub use functionwalkcontext::FunctionWalkContext;

pub fn linearize(program: Vec<TranslationUnitTree>) -> symtab::SymbolTable {
    let mut symtab = symtab::SymbolTable::new();
    let mut context = symtab::BasicDefContext::new(&mut symtab);
    for translation_unit in program {
        translation_unit.walk(&mut context);
    }

    symtab
}

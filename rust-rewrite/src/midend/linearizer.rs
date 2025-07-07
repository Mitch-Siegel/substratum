use treewalk::CustomReturnWalk;

use crate::{
    frontend::*,
    midend::{symtab::DefContext, *},
};

mod block_convergences;
mod block_manager;
mod functionwalkcontext;
mod treewalk;

use block_convergences::*;
pub use block_manager::BlockManager;
pub use functionwalkcontext::FunctionWalkContext;

pub fn linearize(program: Vec<frontend::ast::TranslationUnitTree>) -> Box<symtab::SymbolTable> {
    let mut symtab = symtab::SymbolTable::new();
    let mut context = symtab::BasicDefContext::new(Box::new(symtab));
    for translation_unit in program {
        context = translation_unit.walk(context);
    }

    context.take().unwrap().0
}

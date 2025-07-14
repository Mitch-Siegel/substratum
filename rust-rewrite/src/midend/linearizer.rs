use treewalk::CustomReturnWalk;

use crate::midend::{symtab::DefContext, *};

mod block_manager;
mod functionwalkcontext;
mod treewalk;

pub use block_manager::BlockManager;
pub use functionwalkcontext::FunctionWalkContext;

pub fn linearize(program: Vec<frontend::ast::ModuleTree>) -> Box<symtab::SymbolTable> {
    let symtab = symtab::SymbolTable::new();
    let mut context = symtab::BasicDefContext::new(Box::new(symtab));
    for module in program {
        trace::trace!("walk module {}", module.name);
        context = module.walk(context);
    }

    context.take().unwrap().0
}

use treewalk::CustomReturnWalk;

use crate::midend::{symtab::DefContext, *};

mod block_manager;
mod functionwalkcontext;
mod treewalk;

pub use block_manager::BlockManager;
pub use functionwalkcontext::FunctionWalkContext;

pub fn linearize(program: Vec<frontend::ast::TranslationUnitTree>) -> Box<symtab::SymbolTable> {
    let symtab = symtab::SymbolTable::new();
    let mut context = symtab::BasicDefContext::new(Box::new(symtab));
    context
        .def_path_mut()
        .push(symtab::DefPathComponent::Module(symtab::ModuleName {
            name: "example_module".into(),
        }))
        .unwrap();
    for translation_unit in program {
        context = translation_unit.walk(context);
    }

    context.take().unwrap().0
}

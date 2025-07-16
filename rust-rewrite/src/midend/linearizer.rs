use treewalk::CustomReturnWalk;

use crate::midend::{symtab::DefContext, *};

mod block_manager;
mod functionwalkcontext;
mod treewalk;

pub use block_manager::BlockManager;
pub use functionwalkcontext::FunctionWalkContext;

pub fn linearize(program: Vec<frontend::ast::ModuleTree>) -> Box<symtab::SymbolTable> {
    let mut symtab = Box::new(symtab::SymbolTable::new());
    for module in program {
        let mut module_def_path = symtab::DefPath::empty();
        for module_name in module.module_path.as_slice().split_last().unwrap().1 {
            module_def_path
                .push(symtab::DefPathComponent::Module(symtab::ModuleName {
                    name: module_name.clone(),
                }))
                .unwrap();
        }

        trace::debug!(
            "walk module \"{}\": {:?} (defpath {})",
            module.name,
            module.module_path,
            module_def_path
        );
        let mut context = symtab::BasicDefContext::with_path(
            symtab,
            module_def_path,
            symtab::GenericParamsContext::new(),
        );
        context = module.walk(context);
        symtab = context.take().unwrap().0;
    }

    symtab
}

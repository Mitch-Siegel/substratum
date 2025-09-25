use crate::{frontend, midend::*, trace};

pub mod def_context;
mod functionwalkcontext;

pub use def_context::{GenericParamsContext, WalkContext};
pub use functionwalkcontext::FunctionWalkContext;

pub trait Walk<T> {
    fn walk(self, context: &mut WalkContext) -> T;
}

pub trait CustomWalk<C, T> {
    fn walk(self, context: C) -> T;
}

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
        let mut ctx = WalkContext::new(
            symtab,
            module_def_path,
            def_context::GenericParamsContext::new(),
        );
        module.walk(&mut ctx);
        symtab = ctx.take().unwrap().0;
    }

    symtab
}

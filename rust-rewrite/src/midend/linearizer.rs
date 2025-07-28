use crate::{frontend, midend::*, trace};

mod block_manager;
pub mod def_context;
mod functionwalkcontext;

pub use block_manager::BlockManager;
pub use def_context::{BasicDefContext, DefContext, GenericParamsContext};
pub use functionwalkcontext::FunctionWalkContext;

pub trait Walk {
    fn walk(self, context: &mut impl DefContext);
}

pub trait ValueWalk {
    fn walk(self, context: &mut FunctionWalkContext) -> ir::ValueId;
}

pub trait BasicReturnWalk<U> {
    fn walk(self, context: &mut BasicDefContext) -> U;
}

pub trait ReturnWalk<U> {
    fn walk(self, context: &mut impl DefContext) -> U;
}

pub trait ReturnFunctionWalk<'a, U> {
    fn walk(self, context: &'a mut FunctionWalkContext) -> U;
}

pub trait CustomReturnWalk<C, U> {
    fn walk(self, context: C) -> U;
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
        let mut context = BasicDefContext::with_path(
            symtab,
            module_def_path,
            def_context::GenericParamsContext::new(),
        );
        context = module.walk(context);
        symtab = context.take().unwrap().0;
    }

    symtab
}

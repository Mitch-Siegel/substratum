use crate::{frontend, midend::*, trace};

pub mod collect_ctx;
pub mod function_linearize_context;
pub mod linearize_context;

pub use collect_ctx::CollectCtx;
pub use function_linearize_context::FunctionLinearizeCtx;
pub use linearize_context::{GenericParamsContext, LinearizeCtx};

pub trait CollectSymbols {
    fn collect_symbols(&self, ctx: &mut CollectCtx);
}

pub trait Linearize<T> {
    fn linearize(self, ctx: &mut LinearizeCtx) -> T;
}

pub fn walk(program: Vec<frontend::ast::ModuleTree>) -> Box<symtab::SymbolTable> {
    let mut symtab = Box::new(symtab::SymbolTable::new());

    trace::debug!("collect symbols");

    for module in &program {
        let mut module_def_path = symtab::DefPath::empty();
        for module_name in module.module_path.as_slice().split_last().unwrap().1 {
            module_def_path
                .push(symtab::DefPathComponent::Module(symtab::ModuleName {
                    name: module_name.clone(),
                }))
                .unwrap();
        }
        let mut collect_ctx = CollectCtx::new(symtab, module_def_path.clone());
        module.collect_symbols(&mut collect_ctx);
        let taken = collect_ctx.take();
        symtab = taken.0;
        assert_eq!(taken.1, module_def_path);
    }

    symtab.collect_impls();

    trace::debug!("linearize");

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
        let mut linearize_ctx =
            LinearizeCtx::new(symtab, module_def_path, GenericParamsContext::new());
        module.linearize(&mut linearize_ctx);
        symtab = linearize_ctx.take().unwrap().0;
    }

    symtab
}

use crate::{frontend, midend::*, trace};

pub mod collect_ctx;
pub mod function_linearize_context;
pub mod linearize_context;

pub use collect_ctx::CollectCtx;
pub use function_linearize_context::FunctionLinearizeCtx;
pub use linearize_context::{GenericParamsContext, LinearizeCtx};

#[derive(Debug)]
pub enum CollectError {
    Symbol(symtab::SymbolError),
}

impl From<symtab::SymbolError> for CollectError {
    fn from(value: symtab::SymbolError) -> Self {
        Self::Symbol(value)
    }
}

pub type CollectResult = Result<Box<symtab::SymbolTable>, CollectError>;

pub trait Collect {
    fn collect_symbols(&self, ctx: CollectCtx) -> CollectResult;

    fn collect_to_ctx(&self, ctx: CollectCtx) -> Result<CollectCtx, CollectError> {
        let old_path = ctx.def_path().clone();
        Ok(CollectCtx::new(self.collect_symbols(ctx)?, old_path))
    }
}

pub trait Linearize<T> {
    fn linearize(self, ctx: &mut LinearizeCtx) -> T;
}

pub fn path_from_module(module: &frontend::ast::ModuleTree) -> symtab::DefPath {
    let segments = module
        .module_path
        .iter()
        .map(|segment| symtab::PathSegment::Type(segment.clone()))
        .collect::<Vec<_>>();
    let (last, prefix_segments) = segments.split_last().unwrap();

    symtab::DefPath::new(prefix_segments.into(), last.to_owned())
}

pub fn walk(program: Vec<frontend::ast::ModuleTree>) -> Box<symtab::SymbolTable> {
    let mut symtab = Box::new(symtab::SymbolTable::new());

    trace::debug!("collect symbols");

    for module in &program {
        let path = path_from_module(module);
        let collect_ctx = CollectCtx::new(symtab, path.clone());

        symtab = module.collect_symbols(collect_ctx).unwrap();
    }

    //symtab.collect_impls();

    trace::debug!("linearize");

    for module in program {
        let path = path_from_module(&module);

        trace::debug!(
            "walk module \"{}\": {:?} (defpath {})",
            module.name,
            module.module_path,
            path
        );
        let mut linearize_ctx = LinearizeCtx::new(symtab, path, GenericParamsContext::new());
        module.linearize(&mut linearize_ctx);
        symtab = linearize_ctx.take().unwrap().0;
    }

    symtab
}

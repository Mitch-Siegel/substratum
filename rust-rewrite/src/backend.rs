pub mod arch;
use crate::{midend, trace};
mod codegen;
mod regalloc;

mod module_context;

struct CodegenContext {
    modules: Vec<std::rc::Rc<midend::symtab::Module>>,
}

fn do_backend_for_function(function: &mut midend::symtab::Function, _context: &mut CodegenContext) {
    trace::debug!("Do backend for function {}", function.name());
}

fn do_backend_for_associated(
    associated: &mut midend::symtab::Function,
    associated_with: &midend::types::Type,
    _context: &mut CodegenContext,
) {
    trace::debug!(
        "Do backend for associated {}::{}",
        associated_with,
        associated.name()
    );
}

fn do_backend_for_method(
    method: &mut midend::symtab::Function,
    method_of: &midend::types::Type,
    _context: &mut CodegenContext,
) {
    trace::debug!("Do backend for method {}.{}", method_of, method.name());
}

fn do_backend_for_module(module: &mut midend::symtab::Module, context: &mut CodegenContext) {
    // FUTURE: this seems like a real hack, and likely to cause performance issues
    // probably need to work out a finalized symbol table representation which manages nesting
    // through reference counting to resolve this completely?
    context.modules.push(module.clone().into());
}

fn done_do_backend_for_module(module: &mut midend::symtab::Module, context: &mut CodegenContext) {
    assert_eq!(context.modules.pop().unwrap().name, module.name);
}

pub fn do_backend<'a>(mut symbol_table: midend::symtab::SymbolTable) {
    let visitor = midend::symtab::SymtabVisitor::<CodegenContext>::new(
        Some(do_backend_for_module),
        Some(done_do_backend_for_module),
        Some(do_backend_for_function),
        None,
        Some(do_backend_for_associated),
        Some(do_backend_for_method),
    );

    visitor.visit(
        &mut symbol_table.global_module,
        &mut CodegenContext {
            modules: Vec::new(),
        },
    );
}

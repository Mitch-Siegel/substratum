pub mod arch;
//use crate::{midend, trace};
/*mod codegen;
mod regalloc;

fn do_backend_for_function(
    function: &midend::symtab::Function,
    visitor: &midend::symtab::SymtabVisitor<()>,
    _context: &mut (),
) {
    let allocation = regalloc::allocate_registers::<arch::Target, midend::symtab::SymtabVisitor<()>>(
        regalloc::RegallocContext::new(visitor, None, function),
    );
    trace::debug!("Do backend for function {}", function.name());
}

fn do_backend_for_associated(
    associated: &midend::symtab::Function,
    associated_with: &midend::types::Type,
    _visitor: &midend::symtab::SymtabVisitor<()>,
    _context: &mut (),
) {
    trace::debug!(
        "Do backend for associated {}::{}",
        associated_with,
        associated.name()
    );
}

fn do_backend_for_method(
    method: &midend::symtab::Function,
    method_of: &midend::types::Type,
    _visitor: &midend::symtab::SymtabVisitor<()>,
    _context: &mut (),
) {
    trace::debug!("Do backend for method {}.{}", method_of, method.name());
}

pub fn do_backend<'a>(mut symbol_table: midend::symtab::SymbolTable) {
    let visitor = midend::symtab::SymtabVisitor::<()>::new(
        None,
        Some(do_backend_for_function),
        None,
        Some(do_backend_for_associated),
        Some(do_backend_for_method),
    );

    visitor.visit(&mut symbol_table.global_module, &mut ());
}*/

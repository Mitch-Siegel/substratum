pub mod arch;
use crate::{midend, trace};
//mod regalloc;

struct CodegenContext {}

fn generate_code_for_function(
    function: &mut midend::symtab::Function,
    _context: &mut CodegenContext,
) {
    trace::debug!("Generate code for function {}", function.name());
}

fn generate_code_for_associated(
    associated: &mut midend::symtab::Function,
    associated_with: &midend::types::Type,
    _context: &mut CodegenContext,
) {
    trace::debug!(
        "Generate code for associated {}::{}",
        associated_with,
        associated.name()
    );
}

fn generate_code_for_method(
    method: &mut midend::symtab::Function,
    method_of: &midend::types::Type,
    _context: &mut CodegenContext,
) {
    trace::debug!("Generate code for method {}.{}", method_of, method.name());
}

pub fn generate_code(mut symbol_table: midend::symtab::SymbolTable) {
    let visitor = midend::symtab::SymtabVisitor::<CodegenContext>::new(
        None,
        Some(generate_code_for_function),
        None,
        Some(generate_code_for_associated),
        Some(generate_code_for_method),
    );

    visitor.visit(&mut symbol_table.global_module, &mut CodegenContext {});
}

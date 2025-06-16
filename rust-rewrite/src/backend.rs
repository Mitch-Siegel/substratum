pub mod arch;
use crate::{midend, trace};
//mod regalloc;

struct CodegenContext {}

fn generate_code_for_function<'a>(
    function: &'a midend::symtab::Function,
    walker: &'a midend::symtab::SymtabWalker<CodegenContext>,
    context: &'a mut CodegenContext,
) {
    trace::debug!("Generate code for function {}", function.name());
}

fn generate_code_for_associated<'a>(
    associated: &'a midend::symtab::Function,
    associated_with: midend::types::Type,
    walker: &'a midend::symtab::SymtabWalker<CodegenContext>,
    context: &'a mut CodegenContext,
) {
    trace::debug!(
        "Generate code for associated {}::{}",
        associated_with,
        associated.name()
    );
}

fn generate_code_for_method<'a>(
    method: &'a midend::symtab::Function,
    method_of: midend::types::Type,
    walker: &'a midend::symtab::SymtabWalker<CodegenContext>,
    context: &'a mut CodegenContext,
) {
    trace::debug!("Generate code for method {}.{}", method_of, method.name());
}

pub fn generate_code_for_module(
    module: &midend::symtab::Module,
    parent_modules: Vec<&midend::symtab::Module>,
) {
    let mut parents_this_level = parent_modules.clone();
    parents_this_level.push(&module);

    for (_, submodule) in &module.submodules {
        generate_code_for_module(submodule, parents_this_level.clone());
    }
}

pub fn generate_code(symbol_table: midend::symtab::SymbolTable) {
    let walker = midend::symtab::SymtabWalker::<CodegenContext>::new(
        &symbol_table.global_module,
        None,
        Some(generate_code_for_function),
        None,
        Some(generate_code_for_associated),
        Some(generate_code_for_method),
    );

    generate_code_for_module(&symbol_table.global_module, Vec::new());
}

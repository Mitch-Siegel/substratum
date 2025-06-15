pub mod arch;
use crate::midend;
//mod regalloc;

/*pub fn generate_code_for_function<'a>(
    mut scope_stack: midend::symtab::ScopeStack<'a>,
    function: &'a mut midend::symtab::Function,
) {
    println!("generate code for {}", function.prototype);

    scope_stack.push(&function.scope);
    regalloc::allocate_registers::<arch::Target>(&scope_stack, function);
}*/

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
    generate_code_for_module(&symbol_table.global_module, Vec::new());
}

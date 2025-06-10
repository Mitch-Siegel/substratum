pub mod arch;
//mod regalloc;


/*
pub fn generate_code_for_function<'a>(
    mut scope_stack: midend::symtab::ScopeStack<'a>,
    function: &'a mut midend::symtab::Function,
) {
    println!("generate code for {}", function.prototype);

    scope_stack.push(&function.scope);
    regalloc::allocate_registers::<arch::Target>(&scope_stack, function);
}

pub fn generate_code(mut symbol_table: midend::symtab::SymbolTable) {
        let mut scope_stack = midend::symtab::ScopeStack::new();
    scope_stack.push(&symbol_table.global_scope);
    for (_, member) in &mut symbol_table.functions {
        match member {
            midend::symtab::FunctionOrPrototype::Function(f) => {
                generate_code_for_function(scope_stack.clone(), f)
            }
            midend::symtab::FunctionOrPrototype::Prototype(p) => println!("{}", p),
        }
    }
}
*/

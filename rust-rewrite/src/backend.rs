mod arch;
mod regalloc;

use crate::midend;

pub fn generate_code_for_function(function: &mut midend::symtab::Function) {
    println!("generate code for {}", function.prototype);
    regalloc::allocate_registers(&function.scope, &function.control_flow);
}

pub fn generate_code(mut symbol_table: midend::symtab::SymbolTable) {
    for (_, member) in &mut symbol_table.functions {
        match member {
            midend::symtab::FunctionOrPrototype::Function(f) => generate_code_for_function(f),
            midend::symtab::FunctionOrPrototype::Prototype(p) => println!("{}", p),
        }
    }
}

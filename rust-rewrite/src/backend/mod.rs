mod regalloc;

use crate::midend::*;

pub fn generate_code_for_function(function: &mut symtab::Function) {
    println!("generate code for {}", function.prototype);
    regalloc::allocate_registers(&function.scope, &function.control_flow);
}

pub fn generate_code(mut symbol_table: symtab::SymbolTable) {
    for (_, member) in &mut symbol_table.functions {
        match member {
            symtab::FunctionOrPrototype::Function(f) => generate_code_for_function(f),
            symtab::FunctionOrPrototype::Prototype(p) => println!("{}", p),
        }
    }
}

mod lifetime;
mod regalloc;

use crate::midend::*;

pub fn generate_code_for_function(function: symtab::Function) {
    println!("generate code for {}", function.prototype());
    regalloc::allocate_registers(function.scope(), function.control_flow());
}

pub fn generate_code(symbol_table: symtab::SymbolTable) {
    for (_, member) in symbol_table.functions() {
        match member {
            symtab::FunctionOrPrototype::Function(f) => generate_code_for_function(f),
            symtab::FunctionOrPrototype::Prototype(p) => println!("{}", p),
        }
    }
}

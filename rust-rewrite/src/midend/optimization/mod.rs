use std::collections::HashMap;

use super::symtab::{Function, FunctionOrPrototype};

mod unused_blocks;

fn do_optimizations_on_function(function: &mut Function) {
    // unused_blocks::remove_unused_blocks(function);
}

pub fn optimize_functions(functions: &mut HashMap<String, FunctionOrPrototype>) {
    for (_, function) in functions {
        match function {
            FunctionOrPrototype::Prototype(_) => {}
            FunctionOrPrototype::Function(function) => do_optimizations_on_function(function),
        };
    }
}

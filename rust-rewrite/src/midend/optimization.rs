use std::collections::HashMap;

use crate::midend::symtab::values::Function;

mod unused_blocks;

#[allow(dead_code)]
fn do_optimizations_on_function(_function: &mut Function) {
    // unused_blocks::remove_unused_blocks(function);
}

#[allow(dead_code)]
pub fn optimize_functions(functions: &mut HashMap<String, Function>) {
    for function in functions.values_mut() {
        do_optimizations_on_function(function);
    }
}

use std::collections::HashMap;

pub mod modified_blocks;
use add_block_args::add_block_arguments;
use convert_reads::convert_reads_to_ssa;
use convert_writes::convert_writes_to_ssa;
pub use modified_blocks::ModifiedBlocks;

mod add_block_args;
mod convert_reads;
mod convert_writes;

use super::symtab::{Function, FunctionOrPrototype};

fn convert_function_to_ssa(function: &mut Function) {
    add_block_arguments(function);
    convert_writes_to_ssa(function);
    // convert_reads_to_ssa(function);

    function.control_flow.to_graphviz();
}

pub fn convert_functions_to_ssa(functions: &mut HashMap<String, FunctionOrPrototype>) {
    for (_, function) in functions {
        match function {
            FunctionOrPrototype::Prototype(_) => {}
            FunctionOrPrototype::Function(function) => convert_function_to_ssa(function),
        };
    }
}

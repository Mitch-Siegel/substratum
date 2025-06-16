use std::collections::HashMap;

use add_block_args::add_block_arguments;
use convert_reads::convert_reads_to_ssa;
use convert_writes::convert_writes_to_ssa;

mod add_block_args;
mod convert_reads;
mod convert_writes;

use super::symtab;

fn convert_function_to_ssa(function: &mut symtab::Function) {
    /*add_block_arguments(function);
    convert_writes_to_ssa(function);
    convert_reads_to_ssa(function);*/

    //function.control_flow.to_graphviz();
}

pub fn convert_functions_to_ssa(functions: &mut HashMap<String, symtab::FunctionOrPrototype>) {
    for (_, function_or_prototype) in functions {
        match function_or_prototype {
            symtab::FunctionOrPrototype::Prototype(_) => {}
            symtab::FunctionOrPrototype::Function(function) => convert_function_to_ssa(function),
        };
    }
}

fn remove_ssa_from_function(function: &mut symtab::Function) {
    /*for block in &mut function.control_flow.blocks.values_mut() {
        let old_arguments = block.arguments.clone();
        block.arguments.clear();
        block.arguments = BTreeSet::<ir::OperandName>::new();
        for arg in old_arguments {
            block.arguments.insert(arg.into_non_ssa());
        }
        for statement in &mut block.statements {
            for read in statement.read_operand_names_mut() {
                read.ssa_number = None;
            }

            for write in statement.write_operand_names_mut() {
                write.ssa_number = None;
            }
        }
    }
    function.control_flow.to_graphviz();
    */
}

pub fn remove_ssa_from_functions(functions: &mut HashMap<String, symtab::FunctionOrPrototype>) {
    for (_, function) in functions {
        match function {
            symtab::FunctionOrPrototype::Prototype(_) => {}
            symtab::FunctionOrPrototype::Function(function) => remove_ssa_from_function(function),
        };
    }
}

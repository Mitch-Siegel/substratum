use std::collections::{BTreeSet, HashMap};

use add_block_args::add_block_arguments;
use convert_reads::convert_reads_to_ssa;
use convert_writes::convert_writes_to_ssa;

mod add_block_args;
mod convert_reads;
mod convert_writes;

use super::{ir, symtab};

fn convert_function_to_ssa(mut function: symtab::Function) -> symtab::Function {
    function.control_flow.to_graphviz();
    println!("\n");

    add_block_arguments(&mut function);
    function = convert_writes_to_ssa(function);
    convert_reads_to_ssa(&mut function);

    function.control_flow.to_graphviz();

    function
}

pub fn convert_functions_to_ssa(
    functions: HashMap<String, symtab::FunctionOrPrototype>,
) -> HashMap<String, symtab::FunctionOrPrototype> {
    let mut new_functions = HashMap::<String, symtab::FunctionOrPrototype>::new();

    for (name, function_or_prototype) in functions {
        match function_or_prototype {
            symtab::FunctionOrPrototype::Prototype(prototype) => {
                new_functions.insert(name, symtab::FunctionOrPrototype::Prototype(prototype));
            }
            symtab::FunctionOrPrototype::Function(function) => {
                new_functions.insert(
                    name,
                    symtab::FunctionOrPrototype::Function(convert_function_to_ssa(function)),
                );
            }
        };
    }

    new_functions
}

fn remove_ssa_from_function(function: &mut symtab::Function) {
    for block in &mut function.control_flow.blocks.values_mut() {
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
}

pub fn remove_ssa_from_functions(functions: &mut HashMap<String, symtab::FunctionOrPrototype>) {
    for (_, function) in functions {
        match function {
            symtab::FunctionOrPrototype::Prototype(_) => {}
            symtab::FunctionOrPrototype::Function(function) => remove_ssa_from_function(function),
        };
    }
}

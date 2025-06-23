use std::collections::{BTreeSet, HashMap};

mod add_block_args;
mod convert_reads;
mod convert_writes;

use add_block_args::add_block_arguments;
use convert_reads::convert_reads_to_ssa;
use convert_writes::convert_writes_to_ssa;

use crate::midend::{ir, symtab, types};

fn convert_function_to_ssa(function: &mut symtab::Function, _: &mut ()) {
    add_block_arguments(function);
    convert_writes_to_ssa(function);
    convert_reads_to_ssa(function);
}

fn convert_associated_to_ssa(
    associated: &mut symtab::Function,
    _associated_with: &types::Type,
    context: &mut (),
) {
    convert_function_to_ssa(associated, context)
}

fn convert_method_to_ssa(
    method: &mut symtab::Function,
    _method_of: &types::Type,
    context: &mut (),
) {
    convert_function_to_ssa(method, context);
}

pub fn convert_functions_to_ssa(symtab: &mut symtab::SymbolTable) {
    let visitor = symtab::SymtabVisitor::new(
        None,
        Some(convert_function_to_ssa),
        None,
        Some(convert_associated_to_ssa),
        Some(convert_method_to_ssa),
    );
    visitor.visit(&mut symtab.global_module, &mut ());
}

fn remove_ssa_from_function(function: &mut symtab::Function) {
    for block in &mut function.control_flow {
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
}

pub fn remove_ssa_from_functions(functions: &mut HashMap<String, symtab::FunctionOrPrototype>) {
    for (_, function) in functions {
        match function {
            symtab::FunctionOrPrototype::Prototype(_) => {}
            symtab::FunctionOrPrototype::Function(function) => remove_ssa_from_function(function),
        };
    }
}

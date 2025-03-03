use std::collections::HashMap;

use super::{
    ir::{self, control_flow},
    symtab::{Function, FunctionOrPrototype, SymbolTable},
};

struct SsaWriteConversionMetadata {
    variables: HashMap<String, usize>,
}

impl SsaWriteConversionMetadata {
    pub fn new() -> Self {
        Self {
            variables: HashMap::new(),
        }
    }

    pub fn record_ssa_write_for_variable(&mut self, variable: String) -> usize {
        let entry = self.variables.entry(variable).or_insert(0);
        let returned_write = *entry;
        *entry += 1;

        returned_write
    }
}

fn convert_flow_to_ssa(control_flow: &mut ir::ControlFlow) {
    let mut write_conversion_metadata = SsaWriteConversionMetadata::new();
}

fn convert_function_to_ssa(f: &mut FunctionOrPrototype) {
    match f {
        FunctionOrPrototype::Prototype(_) => {}
        FunctionOrPrototype::Function(function) => convert_flow_to_ssa(&mut function.control_flow),
    };
}

pub fn convert_functions_to_ssa(functions: &mut HashMap<String, FunctionOrPrototype>) {
    functions
        .iter_mut()
        .map(|(function_name, function)| convert_function_to_ssa(function));
}

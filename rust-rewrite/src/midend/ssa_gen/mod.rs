use std::{collections::HashMap, fmt::Display};

use super::{
    ir::{self, control_flow},
    symtab::{Function, FunctionOrPrototype, SymbolTable},
};

struct SsaWriteConversionMetadata {
    variables: HashMap<String, usize>,
}

impl Display for SsaWriteConversionMetadata {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut result: std::fmt::Result = write!(f, "Variables: {{");

        for (variable, ssa_number) in &self.variables {
            result = result.and(writeln!(f, "{}:{}", variable, ssa_number));
        }

        result
    }
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

fn convert_block_writes_to_ssa(
    block: &mut ir::BasicBlock,
    metadata: &mut SsaWriteConversionMetadata,
) {
}

fn convert_flow_to_ssa(control_flow: &mut ir::ControlFlow) {
    let mut write_conversion_metadata = SsaWriteConversionMetadata::new();

    control_flow
        .map_over_blocks_mut_by_bfs(convert_block_writes_to_ssa, &mut write_conversion_metadata);
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

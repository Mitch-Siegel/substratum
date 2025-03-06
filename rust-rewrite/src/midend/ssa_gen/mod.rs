use std::{collections::HashMap, fmt::Display};

use super::{
    idfa,
    ir::{self, control_flow},
    symtab::{Function, FunctionOrPrototype, SymbolTable},
};

#[derive(Debug)]
struct SsaWriteConversionMetadata {
    variables: HashMap<String, usize>,
    reaching_defs: idfa::reaching_defs::Facts,
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
    pub fn new(facts: idfa::reaching_defs::Facts) -> Self {
        Self {
            variables: HashMap::new(),
            reaching_defs: facts,
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
    metadata: Box<SsaWriteConversionMetadata>,
) -> Box<SsaWriteConversionMetadata> {
    println!("Convert block {} writes to ssa", block.label());
    metadata
}

fn convert_flow_to_ssa(control_flow: &mut ir::ControlFlow) {
    let mut reaching_defs = idfa::ReachingDefs::new(control_flow);
    println!("convert flow to ssa");
    reaching_defs.analyze();

    let mut write_conversion_metadata = SsaWriteConversionMetadata::new(reaching_defs.take_facts());

    control_flow.map_over_blocks_mut_by_bfs(convert_block_writes_to_ssa, write_conversion_metadata);
}

fn convert_function_to_ssa(f: &mut FunctionOrPrototype) {
    match f {
        FunctionOrPrototype::Prototype(_) => {}
        FunctionOrPrototype::Function(function) => convert_flow_to_ssa(&mut function.control_flow),
    };
}

pub fn convert_functions_to_ssa(functions: &mut HashMap<String, FunctionOrPrototype>) {
    println!("convert functions to ssa: {} to do", functions.len());

    for (function_name, function) in functions {
        println!("{}", function_name);
        convert_function_to_ssa(function)
    }
    println!("done converting functions to ssa");
}

use std::{
    collections::{HashMap, HashSet},
    fmt::Display,
    fs::read,
};

use super::{
    idfa,
    ir::{self, control_flow},
    symtab::{Function, FunctionOrPrototype, SymbolTable},
};

#[derive(Debug)]
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

    pub fn next_number_for_variable(&mut self, variable: &ir::NamedOperand) -> usize {
        let entry = self
            .variables
            .entry(variable.base_name.clone())
            .or_insert(0);
        let returned_write = *entry;
        *entry += 1;

        returned_write
    }

    pub fn next_number_for_string(&mut self, string: String) -> usize {
        let entry = self.variables.entry(string).or_insert(0);
        let returned_write = *entry;
        *entry += 1;

        returned_write
    }
}

fn convert_block_writes_to_ssa(
    block: &mut ir::BasicBlock,
    mut metadata: Box<SsaWriteConversionMetadata>,
) -> Box<SsaWriteConversionMetadata> {
    for statement in block.statements_mut() {
        for operand in statement.write_operands_mut() {
            println!("{}", operand);
            match operand {
                ir::Operand::Variable(name) => {
                    name.ssa_number = Some(metadata.next_number_for_variable(name));
                }
                ir::Operand::Temporary(name) => {
                    name.ssa_number = Some(metadata.next_number_for_variable(name));
                }
                ir::Operand::UnsignedDecimalConstant(_) => {}
            }
        }
    }
    metadata
}

struct SsaReadConversionMetadata {
    reaching_defs_facts: idfa::reaching_defs::Facts,
    modified_blocks: HashMap<usize, ir::BasicBlock>,
    n_changed_reads: usize,
}

impl<'a> std::fmt::Debug for SsaReadConversionMetadata {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct("SsaReadConversionMetadata")
            .field("reaching_defs_facts", &self.reaching_defs_facts)
            .finish()
    }
}

impl SsaReadConversionMetadata {
    pub fn new(control_flow: &ir::ControlFlow) -> Self {
        let mut analysis = idfa::ReachingDefs::new(control_flow);
        analysis.analyze();

        Self {
            reaching_defs_facts: analysis.take_facts(),
            modified_blocks: HashMap::<usize, ir::BasicBlock>::new(),
            n_changed_reads: 0,
        }
    }

    fn get_read_number_for_variable(
        &mut self,
        name: &ir::NamedOperand,
        block_label: usize,
    ) -> Option<usize> {
        let mut highest_ssa_number = None;
        for out_fact in &self.reaching_defs_facts.for_label(block_label).out_facts {
            if out_fact.base_name == name.base_name {
                highest_ssa_number = Some(highest_ssa_number.unwrap_or(0).max(
                    out_fact.ssa_number.expect(&format!(
                        "Variable {} doesn't have ssa number to read",
                        out_fact
                    )),
                ));
            }
        }
        highest_ssa_number
    }

    pub fn assign_read_number_to_operand(
        &mut self,
        name: &mut ir::NamedOperand,
        block_label: usize,
    ) {
        let number = self.get_read_number_for_variable(name, block_label);
        if number.is_some() {
            let number = number.unwrap();
            match &mut name.ssa_number {
                Some(old_number) => {
                    if number > *old_number {
                        *old_number = number;
                        self.n_changed_reads += 1;
                    }
                }
                None => {
                    name.ssa_number.replace(number);
                    self.n_changed_reads += 1;
                }
            }
        }
    }

    pub fn add_modified_block(&mut self, block: ir::BasicBlock) {
        let label = block.label();
        if self.modified_blocks.insert(label, block).is_some() {
            panic!(
                "Block {} already modified in this pass of SSA read conversion",
                label
            );
        }
    }
}

fn convert_block_reads_to_ssa<'a>(
    block: &ir::BasicBlock,
    mut metadata: Box<SsaReadConversionMetadata>,
) -> Box<SsaReadConversionMetadata> {
    let label = block.label();

    let mut new_block = block.clone();

    for statement in new_block.statements_mut() {
        for operand in statement.read_operands_mut() {
            match operand {
                ir::Operand::Variable(name) => {
                    metadata.assign_read_number_to_operand(name, label);
                }
                ir::Operand::Temporary(name) => {
                    metadata.assign_read_number_to_operand(name, label);
                }
                ir::Operand::UnsignedDecimalConstant(_) => {}
            }
        }
    }
    metadata.add_modified_block(new_block);

    metadata
}

fn convert_function_to_ssa(function: &mut Function) {
    let mut write_conversion_metadata = SsaWriteConversionMetadata::new();
    for argument in &function.prototype.arguments {
        write_conversion_metadata.next_number_for_string(argument.name());
    }
    function
        .control_flow
        .map_over_blocks_mut_by_bfs(convert_block_writes_to_ssa, write_conversion_metadata);

    let mut loop_count = 0;
    loop {
        let read_conversion_metadata = function.control_flow.map_over_blocks_by_bfs(
            convert_block_reads_to_ssa,
            SsaReadConversionMetadata::new(&function.control_flow),
        );
        if read_conversion_metadata.n_changed_reads == 0 {
            break;
        } else {
            for (label, block) in read_conversion_metadata.modified_blocks {
                function.control_flow.blocks[label] = block;
            }
        };

        loop_count += 1;
    }

    function.control_flow.to_graphviz();
}

pub fn convert_functions_to_ssa(functions: &mut HashMap<String, FunctionOrPrototype>) {
    for (function_name, function) in functions {
        match function {
            FunctionOrPrototype::Prototype(_) => {}
            FunctionOrPrototype::Function(function) => convert_function_to_ssa(function),
        };
    }
}

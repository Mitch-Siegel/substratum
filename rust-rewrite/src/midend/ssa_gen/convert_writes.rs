use std::{collections::HashMap, fmt::Display};

use crate::midend::{ir, symtab};

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

    pub fn next_number_for_variable(&mut self, operand_name: &ir::OperandName) -> usize {
        let entry = self
            .variables
            .entry(operand_name.base_name.clone())
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
    let old_args = block.arguments.clone();
    block.arguments.clear();

    for argument in old_args.iter() {
        let mut new_argument = argument.clone();
        new_argument.ssa_number = Some(metadata.next_number_for_variable(argument));
        block.arguments.insert(new_argument);
    }

    for statement in block.statements_mut() {
        for write in statement.write_operand_names_mut() {
            write.ssa_number = Some(metadata.next_number_for_variable(write));
        }
    }
    metadata
}

pub fn convert_writes_to_ssa(function: &mut symtab::Function) {
    let mut write_conversion_metadata = SsaWriteConversionMetadata::new();
    for argument in &function.prototype.arguments {
        write_conversion_metadata.next_number_for_string(argument.name());
    }

    function
        .control_flow
        .map_over_blocks_mut_postorder(convert_block_writes_to_ssa, write_conversion_metadata);
}

use std::collections::HashMap;

use super::{
    ir::{self, control_flow},
    symtab::{Function, FunctionOrPrototype, SymbolTable},
};

fn convert_ir_to_ssa(line: ir::IrLine) -> ir::SsaLine {
    println!("convert {} to ssa", line);
    match line.operation {
        ir::Operations::Assignment(source_dest_operands) => ir::SsaLine::new_assignment(
            line.loc,
            source_dest_operands.source.to_ssa(),
            source_dest_operands.destination.to_ssa(),
        ),
        ir::Operations::BinaryOperation(binary_operations) => ir::SsaLine::new_binary_op(
            line.loc,
            match binary_operations {
                ir::operations::BinaryOperations::Add(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_add(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::Subtract(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_subtract(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::Multiply(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_multiply(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::Divide(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_divide(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::LThan(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_lthan(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::GThan(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_gthan(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::LThanE(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_lthan_e(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::GThanE(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_gthan_e(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::Equals(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_equals(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
                ir::operations::BinaryOperations::NotEquals(binary_arithmetic_operands) => {
                    ir::operations::BinaryOperations::new_not_equals(
                        binary_arithmetic_operands.destination.to_ssa(),
                        binary_arithmetic_operands.sources.a.to_ssa(),
                        binary_arithmetic_operands.sources.b.to_ssa(),
                    )
                }
            },
        ),
        ir::Operations::Jump(jump_operation) => ir::SsaLine::new_jump(
            line.loc,
            jump_operation.destination_block,
            jump_operation.condition.to_ssa(),
        ),
    }
}

fn convert_block_to_ssa_type(block: ir::NonSsaBlock) -> ir::SsaBlock {
    block
        .into_iter()
        .map(|statement| convert_ir_to_ssa(statement))
        .collect()
}

fn convert_blocks_to_ssa(blocks: Vec<ir::NonSsaBlock>) -> Vec<ir::SsaBlock> {
    blocks
        .into_iter()
        .map(|block| convert_block_to_ssa_type(block))
        .collect()
}

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

fn convert_flow_to_ssa(control_flow: ir::ControlFlow) -> ir::ControlFlow {
    let cfg = match control_flow.blocks {
        control_flow::CfgBlocks::Basic(blocks) => {
            let blocks = convert_blocks_to_ssa(blocks);
            blocks
        }
        control_flow::CfgBlocks::Ssa(blocks) => blocks,
    };

    let mut write_conversion_metadata = SsaWriteConversionMetadata::new();

    ir::ControlFlow::new()
}

fn convert_function_to_ssa(f: FunctionOrPrototype) -> Option<FunctionOrPrototype> {
    match f {
        FunctionOrPrototype::Prototype(_) => None,
        FunctionOrPrototype::Function(function) => Some(FunctionOrPrototype::Function(Function {
            prototype: function.prototype,
            scope: function.scope,
            control_flow: convert_flow_to_ssa(function.control_flow),
        })),
    }
}

pub fn convert_functions_to_ssa(
    functions: HashMap<String, FunctionOrPrototype>,
) -> HashMap<String, FunctionOrPrototype> {
    functions
        .into_iter()
        .filter_map(|(function_name, function)| {
            let convert_result = convert_function_to_ssa(function);
            match convert_result {
                Some(function) => Some((function_name, function)),
                None => None,
            }
        })
        .collect()
}

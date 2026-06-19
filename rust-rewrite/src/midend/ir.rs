pub mod basic_block;
pub mod block_manager;
pub mod control_flow;
pub mod lowered;
pub mod lowering;
pub mod type_inference;
pub mod unlowered;
pub mod value;

#[cfg(test)]
mod tests;

use std::fmt::Display;
use type_inference::*;

use crate::{frontend::sourceloc::SourceLoc, midend::*};
use serde::Serialize;

pub use basic_block::*;
pub use block_manager::BlockManager;
pub use control_flow::ControlFlow;
pub use value::*;

#[derive(Debug, PartialEq, Eq, Clone)]
#[enum_delegate::implement(OperandTypeInference)]
pub enum Operation {
    Lowered(lowered::Operation),
    Unlowered(unlowered::Operation),
}

impl Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Lowered(lowered) => write!(f, "{}", lowered),
            Self::Unlowered(unlowered) => write!(f, "{}", unlowered),
        }
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct IrLine {
    pub loc: SourceLoc,
    pub operation: Operation,
}

impl Display for IrLine {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.operation)
    }
}

impl IrLine {
    pub fn is_lowered(&self) -> bool {
        match self.operation {
            Operation::Lowered(_) => true,
            Operation::Unlowered(_) => false,
        }
    }
}

impl OperandTypeInference for IrLine {
    fn infer_types(&mut self, ctx: &TypeInferenceContext) -> bool {
        self.operation.infer_types(ctx)
    }
}

// IrLine constructors
impl IrLine {
    fn new_lowered(loc: SourceLoc, operation: lowered::Operation) -> Self {
        IrLine {
            loc,
            operation: Operation::Lowered(operation),
        }
    }
    fn new_unlowered(loc: SourceLoc, operation: unlowered::Operation) -> Self {
        IrLine {
            loc,
            operation: Operation::Unlowered(operation),
        }
    }

    pub fn new_assignment(loc: SourceLoc, destination: ValueId, source: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_assignment(destination, source))
    }

    pub fn new_binary_arithmetic_expression(
        loc: SourceLoc,
        destination: ValueId,
        operands: lowered::operands::BinaryArithmeticOperands,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_binary_arithmetic_expression(destination, operands),
        )
    }

    pub fn new_binary_comparison_expression(
        loc: SourceLoc,
        destination: ValueId,
        operands: lowered::operands::BinaryComparisonOperands,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_binary_comparison_expression(destination, operands),
        )
    }

    pub fn new_jump(
        loc: SourceLoc,
        destination_block: usize,
        condition: lowered::operands::JumpCondition,
    ) -> Self {
        Self::new_lowered(loc, lowered::new_jump(destination_block, condition))
    }

    pub fn new_call(
        loc: SourceLoc,
        function_operand: ValueId,
        arguments: lowered::operands::OrderedArgumentList,
        return_value_to: ValueId,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_call(function_operand, arguments, Some(return_value_to)),
        )
    }

    pub fn new_compute_field_address(
        loc: SourceLoc,
        receiver: ValueId,
        field_offset: usize,
        destination: ValueId,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_compute_field_address(receiver, field_offset, destination),
        )
    }

    pub fn new_load(loc: SourceLoc, pointer: ValueId, destination: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_load(pointer, destination))
    }

    pub fn new_store(loc: SourceLoc, source: ValueId, pointer: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_store(source, pointer))
    }

    //
    // unlowered IR constructors
    //
    pub fn new_match(
        loc: SourceLoc,
        scrutinee: ValueId,
        arms: Vec<unlowered::operands::MatchArm>,
    ) -> Self {
        Self::new_unlowered(loc, unlowered::new_match(scrutinee, arms))
    }

    pub fn new_discriminant(loc: SourceLoc, enum_value: ValueId, destination: ValueId) -> Self {
        Self::new_unlowered(loc, unlowered::new_discriminant(enum_value, destination))
    }

    pub fn new_get_field_pointer(
        loc: SourceLoc,
        receiver: ValueId,
        field_name: String,
        destination: ValueId,
    ) -> Self {
        Self::new_unlowered(
            loc,
            unlowered::new_get_field_pointer(receiver, field_name, destination),
        )
    }
    //
    // general utility functions
    //
    pub fn read_value_ids(&self) -> Vec<ValueId> {
        match &self.operation {
            Operation::Lowered(lowered) => lowered.read_value_ids(),
            Operation::Unlowered(unlowered) => unlowered.read_value_ids(),
        }
    }

    pub fn write_value_ids(&self) -> Vec<ValueId> {
        match &self.operation {
            Operation::Lowered(lowered) => lowered.write_value_ids(),
            Operation::Unlowered(unlowered) => unlowered.write_value_ids(),
        }
    }
}

pub trait IrOperation: OperandTypeInference {
    fn read_value_ids(&self) -> Vec<ValueId>;
    fn write_value_ids(&self) -> Vec<ValueId>;
}

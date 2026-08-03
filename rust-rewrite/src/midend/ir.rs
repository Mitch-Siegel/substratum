pub(crate) mod basic_block;
pub(crate) mod block_manager;
pub(crate) mod control_flow;
pub(crate) mod lowered;
pub(crate) mod lowering;
pub(crate) mod type_inference;
pub(crate) mod unlowered;
pub(crate) mod value;

#[cfg(test)]
mod tests;

use std::fmt::Display;
use type_inference::{OperandTypeInference, TypeInferenceContext};

use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{ir, symtab, types},
};
use serde::Serialize;

pub(crate) use basic_block::*;
pub(crate) use block_manager::BlockManager;
pub(crate) use control_flow::ControlFlow;
pub(crate) use value::*;

#[derive(Debug, PartialEq, Eq, Clone)]
#[enum_delegate::implement(OperandTypeInference)]
pub(crate) enum Operation {
    Lowered(lowered::Operation),
    Unlowered(unlowered::Operation),
}

impl Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Lowered(lowered) => write!(f, "{lowered}"),
            Self::Unlowered(unlowered) => write!(f, "{unlowered}"),
        }
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub(crate) struct IrLine {
    pub loc: SourceLoc,
    pub operation: Operation,
}

impl Display for IrLine {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.operation)
    }
}

impl IrLine {
    pub(crate) fn is_lowered(&self) -> bool {
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
        Self {
            loc,
            operation: Operation::Lowered(operation),
        }
    }
    fn new_unlowered(loc: SourceLoc, operation: unlowered::Operation) -> Self {
        Self {
            loc,
            operation: Operation::Unlowered(operation),
        }
    }

    pub(crate) fn new_assignment(loc: SourceLoc, destination: ValueId, source: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_assignment(destination, source))
    }

    pub(crate) fn new_binary_arithmetic_expression(
        loc: SourceLoc,
        destination: ValueId,
        operands: lowered::operands::BinaryArithmeticOperands,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_binary_arithmetic_expression(destination, operands),
        )
    }

    pub(crate) fn new_binary_comparison_expression(
        loc: SourceLoc,
        destination: ValueId,
        operands: lowered::operands::BinaryComparisonOperands,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_binary_comparison_expression(destination, operands),
        )
    }

    pub(crate) fn new_jump(
        loc: SourceLoc,
        destination_block: usize,
        condition: lowered::operands::JumpCondition,
    ) -> Self {
        Self::new_lowered(loc, lowered::new_jump(destination_block, condition))
    }

    pub(crate) fn new_call(
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

    pub(crate) fn new_compute_field_address(
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

    pub(crate) fn new_load(loc: SourceLoc, pointer: ValueId, destination: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_load(pointer, destination))
    }

    pub(crate) fn new_store(loc: SourceLoc, source: ValueId, pointer: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_store(source, pointer))
    }

    //
    // unlowered IR constructors
    //
    pub(crate) fn new_match(
        loc: SourceLoc,
        scrutinee: ValueId,
        arms: Vec<unlowered::operands::MatchArm>,
    ) -> Self {
        Self::new_unlowered(loc, unlowered::new_match(scrutinee, arms))
    }

    pub(crate) fn new_discriminant(
        loc: SourceLoc,
        enum_value: ValueId,
        destination: ValueId,
    ) -> Self {
        Self::new_unlowered(loc, unlowered::new_discriminant(enum_value, destination))
    }

    pub(crate) fn new_get_field_pointer(
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
    pub(crate) fn read_value_ids(&self) -> Vec<ValueId> {
        match &self.operation {
            Operation::Lowered(lowered) => lowered.read_value_ids(),
            Operation::Unlowered(unlowered) => unlowered.read_value_ids(),
        }
    }

    pub(crate) fn write_value_ids(&self) -> Vec<ValueId> {
        match &self.operation {
            Operation::Lowered(lowered) => lowered.write_value_ids(),
            Operation::Unlowered(unlowered) => unlowered.write_value_ids(),
        }
    }
}

pub(crate) trait IrOperation: OperandTypeInference {
    fn read_value_ids(&self) -> Vec<ValueId>;
    fn write_value_ids(&self) -> Vec<ValueId>;
}

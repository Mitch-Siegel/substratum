pub mod control_flow;
pub mod operands;
pub mod operations;

use std::fmt::Display;

use crate::frontend::sourceloc::SourceLoc;
use crate::midend::ir;
use serde::Serialize;

pub use control_flow::ControlFlow;
pub use operands::*;
pub use operations::*;

use super::program_point::ProgramPoint;

#[derive(Debug, Serialize)]
pub struct IrLine {
    pub loc: SourceLoc,
    pub program_point: ProgramPoint,
    pub operation: Operations,
}

impl Display for IrLine {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}@{} {}",
            self.program_point,
            self.loc.to_string(),
            self.operation
        )
    }
}

pub type BasicBlock = Vec<IrLine>;

impl IrLine {
    fn new(loc: SourceLoc, operation: Operations) -> Self {
        IrLine {
            loc: loc,
            program_point: ProgramPoint::default(),
            operation: operation,
        }
    }

    pub fn new_assignment(loc: SourceLoc, destination: Operand, source: Operand) -> Self {
        Self::new(
            loc,
            Operations::Assignment(SourceDestOperands {
                destination,
                source,
            }),
        )
    }

    pub fn new_binary_op(loc: SourceLoc, op: BinaryOperations) -> Self {
        Self::new(loc, Operations::BinaryOperation(op))
    }

    pub fn new_jump(
        loc: SourceLoc,
        destination_block: usize,
        condition: operands::JumpCondition,
    ) -> Self {
        Self::new(
            loc,
            Operations::Jump(operations::JumpOperation {
                destination_block,
                condition,
            }),
        )
    }

    pub fn program_point(&self) -> &ProgramPoint {
        &self.program_point
    }

    pub fn read_operands(&self) -> Vec<&Operand> {
        let mut operands: Vec<&Operand> = Vec::new();
        match &self.operation {
            Operations::Assignment(source_dest) => operands.push(&source_dest.source),
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                operands.push(&arithmetic_operands.sources.a);
                operands.push(&arithmetic_operands.sources.b);
            }
            Operations::Jump(jump_operands) => match &jump_operands.condition {
                JumpCondition::Unconditional => {}
                JumpCondition::Eq(condition_operands) => {
                    operands.push(&condition_operands.a);
                    operands.push(&condition_operands.b);
                }
                JumpCondition::NE(condition_operands) => {
                    operands.push(&condition_operands.a);
                    operands.push(&condition_operands.b);
                }
                JumpCondition::G(condition_operands) => {
                    operands.push(&condition_operands.a);
                    operands.push(&condition_operands.b);
                }
                JumpCondition::L(condition_operands) => {
                    operands.push(&condition_operands.a);
                    operands.push(&condition_operands.b);
                }
                JumpCondition::GE(condition_operands) => {
                    operands.push(&condition_operands.a);
                    operands.push(&condition_operands.b);
                }
                JumpCondition::LE(condition_operands) => {
                    operands.push(&condition_operands.a);
                    operands.push(&condition_operands.b);
                }
            },
        }

        operands
    }

    pub fn write_operands(&self) -> Vec<&Operand> {
        let mut operands: Vec<&Operand> = Vec::new();
        match &self.operation {
            Operations::Assignment(source_dest) => operands.push(&source_dest.destination),
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                operands.push(&arithmetic_operands.destination);
            }
            Operations::Jump(_) => {}
        }

        operands
    }

    pub fn write_operands_mut(&mut self) -> Vec<&mut Operand> {
        let mut operands: Vec<&mut Operand> = Vec::new();
        match &mut self.operation {
            Operations::Assignment(source_dest) => operands.push(&mut source_dest.destination),
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands_mut();
                operands.push(&mut arithmetic_operands.destination);
            }
            Operations::Jump(_) => {}
        }

        operands
    }
}

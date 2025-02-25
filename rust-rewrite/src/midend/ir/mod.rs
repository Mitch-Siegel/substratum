pub mod operands;
pub mod operations;

use std::fmt::Display;

use crate::frontend::sourceloc::SourceLoc;
use crate::midend::ir;
use operands::*;
use operations::*;
use serde::Serialize;

pub use operands::BasicOperand;
pub use operands::GenericOperand;
pub use operands::SsaOperand;

use super::program_point::ProgramPoint;

#[derive(Debug, Serialize)]
pub struct IrBase<T>
where
    T: std::fmt::Display,
{
    pub loc: SourceLoc,
    pub program_point: ProgramPoint,
    pub operation: IROperations<T>,
}

impl<T> Display for IrBase<T>
where
    T: std::fmt::Display,
{
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

pub type IrLine = IrBase<String>;
pub type SsaLine = IrBase<SsaOperand>;

pub type BasicOperations = IROperations<String>;
pub type SsaOperations = IROperations<SsaOperand>;

impl<T> IrBase<T>
where
    T: std::fmt::Display,
{
    fn new(loc: SourceLoc, operation: IROperations<T>) -> Self {
        IrBase::<T> {
            loc: loc,
            program_point: ProgramPoint::default(),
            operation: operation,
        }
    }

    pub fn new_assignment(
        loc: SourceLoc,
        destination: ir::operands::GenericOperand<T>,
        source: ir::operands::GenericOperand<T>,
    ) -> Self {
        Self::new(
            loc,
            IROperations::Assignment(SourceDestOperands::<T> {
                destination,
                source,
            }),
        )
    }

    pub fn new_binary_op(loc: SourceLoc, op: BinaryOperations<T>) -> Self {
        Self::new(loc, IROperations::BinaryOperation(op))
    }

    pub fn new_jump(
        loc: SourceLoc,
        destination_block: usize,
        condition: ir::operands::JumpCondition<T>,
    ) -> Self {
        Self::new(
            loc,
            ir::IROperations::<T>::Jump(ir::operations::JumpOperation {
                destination_block,
                condition,
            }),
        )
    }

    pub fn program_point(&self) -> &ProgramPoint {
        &self.program_point
    }

    pub fn read_operands(&self) -> Vec<&ir::operands::GenericOperand<T>> {
        let mut operands: Vec<&ir::operands::GenericOperand<T>> = Vec::new();
        match &self.operation {
            IROperations::Assignment(source_dest) => operands.push(&source_dest.source),
            IROperations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                operands.push(&arithmetic_operands.sources.a);
                operands.push(&arithmetic_operands.sources.b);
            }
            IROperations::Jump(jump_operands) => match &jump_operands.condition {
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

    pub fn write_operands(&self) -> Vec<&ir::operands::GenericOperand<T>> {
        let mut operands: Vec<&ir::operands::GenericOperand<T>> = Vec::new();
        match &self.operation {
            IROperations::Assignment(source_dest) => operands.push(&source_dest.destination),
            IROperations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                operands.push(&arithmetic_operands.destination);
            }
            IROperations::Jump(_) => {}
        }

        operands
    }
}

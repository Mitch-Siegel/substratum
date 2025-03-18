pub mod control_flow;
pub mod operands;
pub mod operations;

use std::collections::{BTreeSet, HashMap};
use std::fmt::Display;

use crate::frontend::sourceloc::SourceLoc;
use crate::midend::ir;
use serde::Serialize;

pub use control_flow::ControlFlow;
pub use operands::*;
pub use operations::*;

use super::program_point::ProgramPoint;

#[derive(Debug, Serialize, Clone)]
pub struct IrLine {
    pub loc: SourceLoc,
    pub program_point: ProgramPoint,
    pub operation: Operations,
}

impl Display for IrLine {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.operation)
    }
}

#[derive(Clone, Debug, Serialize)]
pub struct BasicBlock {
    pub label: usize,
    pub statements: Vec<ir::IrLine>,
    pub arguments: BTreeSet<ir::OperandName>,
}

impl BasicBlock {
    pub fn new(label: usize) -> Self {
        BasicBlock {
            statements: Vec::new(),
            label: label,
            arguments: BTreeSet::new(),
        }
    }

    pub fn append_statement(&mut self, statement: ir::IrLine) {
        self.statements.push(statement);
    }

    pub fn label(&self) -> usize {
        self.label
    }

    pub fn statements(&self) -> &Vec<ir::IrLine> {
        &self.statements
    }

    pub fn statements_mut(&mut self) -> &mut Vec<ir::IrLine> {
        &mut self.statements
    }
}

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
                block_args: HashMap::new(),
                condition,
            }),
        )
    }

    pub fn read_operand_names(&self) -> Vec<&OperandName> {
        let mut operand_names: Vec<&OperandName> = Vec::new();
        match &self.operation {
            Operations::Assignment(source_dest) => match &source_dest.source.get_name() {
                Some(name) => operand_names.push(name),
                None => {}
            },
            Operations::BinaryOperation(operation) => {
                let sources = &operation.raw_operands().sources;
                match sources.a.get_name() {
                    Some(name) => operand_names.push(name),
                    None => {}
                }
                match sources.b.get_name() {
                    Some(name) => operand_names.push(name),
                    None => {}
                }
            }
            Operations::Jump(jump_operands) => {
                match &jump_operands.condition {
                    JumpCondition::Unconditional => {}
                    JumpCondition::Eq(condition_operands)
                    | JumpCondition::NE(condition_operands)
                    | JumpCondition::G(condition_operands)
                    | JumpCondition::L(condition_operands)
                    | JumpCondition::GE(condition_operands)
                    | JumpCondition::LE(condition_operands) => {
                        match condition_operands.a.get_name() {
                            Some(name) => operand_names.push(name),
                            None => {}
                        }
                        match condition_operands.b.get_name() {
                            Some(name) => operand_names.push(name),
                            None => {}
                        }
                    }
                };
                for arg in jump_operands.block_args.values() {
                    operand_names.push(arg);
                }
            }
        }
        operand_names
    }

    pub fn read_operand_names_mut(&mut self) -> Vec<&mut OperandName> {
        let mut operand_names: Vec<&mut OperandName> = Vec::new();
        match &mut self.operation {
            Operations::Assignment(source_dest) => match source_dest.source.get_name_mut() {
                Some(name) => operand_names.push(name),
                None => {}
            },
            Operations::BinaryOperation(operation) => {
                let sources = &mut operation.raw_operands_mut().sources;
                match sources.a.get_name_mut() {
                    Some(name) => operand_names.push(name),
                    None => {}
                }
                match sources.b.get_name_mut() {
                    Some(name) => operand_names.push(name),
                    None => {}
                }
            }
            Operations::Jump(jump_operands) => {
                match &mut jump_operands.condition {
                    JumpCondition::Unconditional => {}
                    JumpCondition::Eq(condition_operands)
                    | JumpCondition::NE(condition_operands)
                    | JumpCondition::G(condition_operands)
                    | JumpCondition::L(condition_operands)
                    | JumpCondition::GE(condition_operands)
                    | JumpCondition::LE(condition_operands) => {
                        match condition_operands.a.get_name_mut() {
                            Some(name) => operand_names.push(name),
                            None => {}
                        }
                        match condition_operands.b.get_name_mut() {
                            Some(name) => operand_names.push(name),
                            None => {}
                        }
                    }
                };
                for arg in jump_operands.block_args.values_mut() {
                    operand_names.push(arg);
                }
            }
        }

        operand_names
    }

    pub fn write_operand_names(&self) -> Vec<&OperandName> {
        let mut operand_names: Vec<&OperandName> = Vec::new();
        match &self.operation {
            Operations::Assignment(source_dest) => {
                operand_names.push(&source_dest.destination.get_name().unwrap())
            }
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                operand_names.push(&arithmetic_operands.destination.get_name().unwrap());
            }
            Operations::Jump(_) => {}
        }

        operand_names
    }

    pub fn write_operand_names_mut(&mut self) -> Vec<&mut OperandName> {
        let mut operands: Vec<&mut OperandName> = Vec::new();
        match &mut self.operation {
            Operations::Assignment(source_dest) => {
                operands.push(source_dest.destination.get_name_mut().unwrap())
            }
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands_mut();
                operands.push(arithmetic_operands.destination.get_name_mut().unwrap());
            }
            Operations::Jump(_) => {}
        }

        operands
    }
}

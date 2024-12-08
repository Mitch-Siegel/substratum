use std::collections::HashMap;
use std::collections::HashSet;

use crate::lexer::SourceLoc;
use serde::Serialize;

use super::Type;
use super::WalkContext;

#[derive(Clone, Debug, Serialize)]
pub enum IROperand {
    Variable(String),
    Temporary(String),
    UnsignedDecimalConstant(usize),
}

impl IROperand {
    pub fn new_as_variable(identifier: String) -> Self {
        IROperand::Variable(identifier)
    }

    pub fn new_as_temporary(identifier: String) -> Self {
        IROperand::Temporary(identifier)
    }

    pub fn new_as_unsigned_decimal_constant(constant: usize) -> Self {
        IROperand::UnsignedDecimalConstant(constant)
    }

    pub fn type_(&self, context: &WalkContext) -> Type {
        match self {
            IROperand::Variable(name) => context
                .lookup_variable_by_name(name)
                .expect(format!("Use of undeclared variable {}", name).as_str())
                .type_(),
            IROperand::Temporary(name) => context
                .lookup_variable_by_name(name)
                .expect(format!("Use of undeclared variable {}", name).as_str())
                .type_()
                .clone(),
            IROperand::UnsignedDecimalConstant(value) => {
                if *value > (u32::MAX as usize) {
                    Type::new_u64(0)
                } else if *value > (u16::MAX as usize) {
                    Type::new_u32(0)
                } else if *value > (u8::MAX as usize) {
                    Type::new_u16(0)
                } else {
                    Type::new_u8(0)
                }
            }
        }
    }
}

#[derive(Debug, Serialize)]
pub enum BinaryOperations {
    Add(BinaryArithmeticOperands),
    Subtract(BinaryArithmeticOperands),
    Multiply(BinaryArithmeticOperands),
    Divide(BinaryArithmeticOperands),
}

impl BinaryOperations {
    pub fn new_add(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::Add(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_subtract(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::Subtract(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_multiply(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::Multiply(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_divide(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::Divide(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }
}

#[derive(Debug, Serialize)]
pub struct SourceDestOperands {
    destination: IROperand,
    source: IROperand,
}

#[derive(Debug, Serialize)]
pub struct DualSourceOperands {
    pub a: IROperand,
    pub b: IROperand,
}

impl DualSourceOperands {
    pub fn from(a: IROperand, b: IROperand) -> Self {
        DualSourceOperands { a, b }
    }
}

#[derive(Debug, Serialize)]
pub struct BinaryArithmeticOperands {
    pub destination: IROperand,
    pub sources: DualSourceOperands,
}

impl BinaryArithmeticOperands {
    pub fn from(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryArithmeticOperands {
            destination,
            sources: DualSourceOperands::from(source_a, source_b),
        }
    }
}

#[derive(Debug, Serialize)]
pub enum JumpCondition {
    Unconditional,
    Eq(DualSourceOperands),
    NE(DualSourceOperands),
    G(DualSourceOperands),
    L(DualSourceOperands),
    GE(DualSourceOperands),
    LE(DualSourceOperands),
}

#[derive(Debug, Serialize)]
pub struct JumpOperands {
    pub destination_block: usize,
    pub condition: JumpCondition,
}

#[derive(Debug, Serialize)]
pub enum IROperations {
    Assignment(SourceDestOperands),
    BinaryOperation(BinaryOperations),
    Jump(JumpOperands),
}

#[derive(Debug, Serialize)]
pub struct IR {
    loc: SourceLoc,
    operation: IROperations,
}

impl IR {
    pub fn new_assignment(loc: SourceLoc, destination: IROperand, source: IROperand) -> Self {
        IR {
            loc: loc,
            operation: IROperations::Assignment(SourceDestOperands {
                destination,
                source,
            }),
        }
    }

    pub fn new_binary_op(loc: SourceLoc, op: BinaryOperations) -> Self {
        IR {
            loc,
            operation: IROperations::BinaryOperation(op),
        }
    }

    pub fn new_jump(loc: SourceLoc, destination_block: usize, condition: JumpCondition) -> Self {
        IR {
            loc,
            operation: IROperations::Jump(JumpOperands {
                destination_block,
                condition,
            }),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct BasicBlock {
    label: usize,
    statements: Vec<IR>,
    targets: HashSet<usize>,
}

impl BasicBlock {
    pub fn new(label: usize) -> Self {
        BasicBlock {
            label,
            statements: Vec::new(),
            targets: HashSet::new(),
        }
    }

    pub fn append_statement(&mut self, statement: IR) {
        match &statement.operation {
            IROperations::Jump(operands) => {
                self.targets.insert(operands.destination_block);
            }
            _ => {}
        }

        self.statements.push(statement);
    }
}

#[derive(Debug, Serialize)]
pub struct ControlFlow {
    // indexed by numerical block number
    blocks: Vec<BasicBlock>,
    // mapping from source block to set of destination blocks
    branches: HashMap<usize, HashSet<usize>>,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
}

impl ControlFlow {
    pub fn new() -> Self {
        ControlFlow {
            blocks: Vec::new(),
            branches: HashMap::new(),
            temp_num: 0,
        }
    }

    pub fn new_block(&mut self) -> usize {
        self.blocks.push(BasicBlock::new(self.blocks.len()));
        self.blocks.last_mut().unwrap().label
    }

    pub fn append_to_block(&mut self, label: usize, statement: IR) {
        self.blocks[label].append_statement(statement);
    }

    pub fn next_temp(&mut self) -> String {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        temp_name
    }
}

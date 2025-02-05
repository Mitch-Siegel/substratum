use std::collections::HashSet;
use std::fmt::Display;

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
    LThan(BinaryArithmeticOperands),
    GThan(BinaryArithmeticOperands),
    LThanE(BinaryArithmeticOperands),
    GThanE(BinaryArithmeticOperands),
    Equals(BinaryArithmeticOperands),
    NotEquals(BinaryArithmeticOperands),
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

    pub fn new_lthan(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::LThan(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_gthan(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::GThan(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_lthan_e(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::LThanE(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_gthan_e(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::GThanE(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_equals(destination: IROperand, source_a: IROperand, source_b: IROperand) -> Self {
        BinaryOperations::Equals(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_not_equals(
        destination: IROperand,
        source_a: IROperand,
        source_b: IROperand,
    ) -> Self {
        BinaryOperations::NotEquals(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn raw_operands(&self) -> &BinaryArithmeticOperands {
        match self {
            Self::Add(ops)
            | Self::Subtract(ops)
            | Self::Multiply(ops)
            | Self::Divide(ops)
            | Self::LThan(ops)
            | Self::GThan(ops)
            | Self::LThanE(ops)
            | Self::GThanE(ops)
            | Self::Equals(ops)
            | Self::NotEquals(ops) => ops,
        }
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

impl Display for IR {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "@{} {:?}", self.loc.to_string(), self.operation)
    }
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

    pub fn read_operands(&self) -> Vec<&IROperand> {
        let mut operands: Vec<&IROperand> = Vec::new();
        match &self.operation {
            IROperations::Assignment(source_dest) => operands.push(&source_dest.source),
            IROperations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                operands.push(&arithmetic_operands.sources.a);
                operands.push(&arithmetic_operands.sources.b);
            }
            IROperations::Jump(_) => {},
        }

        operands
    }

    pub fn write_operands(&self) -> Vec<&IROperand> {
        let mut operands: Vec<&IROperand> = Vec::new();
        match &self.operation {
            IROperations::Assignment(source_dest) => operands.push(&source_dest.destination),
            IROperations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                operands.push(&arithmetic_operands.destination);
            }
            IROperations::Jump(_) => {},
        }

        operands
    }
}

#[derive(Debug, Serialize)]
pub struct BasicBlock {
    label: usize,
    statements: Vec<IR>,
    targets: HashSet<usize>,
}

impl Display for BasicBlock {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "Basic Block {}", self.label)?;
        write!(f, "\tTargets: [")?;
        let mut first_target = true;
        for target in &self.targets {
            if !first_target {
                write!(f, ", ")?;
            } else {
                first_target = false;
            }
            write!(f, "{}", target)?;
        }
        writeln!(f, "]")?;

        for statement in &self.statements {
            writeln!(f, "\t{}", statement)?;
        }
        std::fmt::Result::Ok(())
    }
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

    pub fn label(&self) -> usize {
        self.label
    }

    pub fn statements(&self) -> &Vec<IR> {
        &self.statements
    }
}

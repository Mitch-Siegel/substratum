use serde::Serialize;
use std::{collections::HashMap, fmt::Display};

use super::operands::*;

/// ## Binary Operations
#[derive(Debug, Serialize, Clone)]
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

impl Display for BinaryOperations {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Add(operands) => {
                write!(
                    f,
                    "{} = {} + {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::Subtract(operands) => {
                write!(
                    f,
                    "{} = {} - {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::Multiply(operands) => {
                write!(
                    f,
                    "{} = {} * {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::Divide(operands) => {
                write!(
                    f,
                    "{} = {} / {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::LThan(operands) => {
                write!(
                    f,
                    "{} = {} < {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::GThan(operands) => {
                write!(
                    f,
                    "{} = {} > {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::LThanE(operands) => {
                write!(
                    f,
                    "{} = {} <= {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::GThanE(operands) => {
                write!(
                    f,
                    "{} = {} >= {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::Equals(operands) => {
                write!(
                    f,
                    "{} = {} == {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::NotEquals(operands) => {
                write!(
                    f,
                    "{} = {} != {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
        }
    }
}

impl BinaryOperations {
    pub fn new_add(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::Add(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_subtract(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::Subtract(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_multiply(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::Multiply(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_divide(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::Divide(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_lthan(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::LThan(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_gthan(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::GThan(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_lthan_e(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::LThanE(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_gthan_e(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::GThanE(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_equals(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryOperations::Equals(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_not_equals(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
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

    pub fn raw_operands_mut(&mut self) -> &mut BinaryArithmeticOperands {
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

/// ## Jump
#[derive(Debug, Serialize, Clone)]
pub struct JumpOperation {
    pub destination_block: usize,
    pub block_args: HashMap<OperandName, OperandName>,
    pub condition: JumpCondition,
}

impl Display for JumpOperation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut block_args_string = String::new();
        for (arg, operand) in &self.block_args {
            block_args_string += &format!("{}:{} ", arg, operand);
        }
        write!(
            f,
            "{} Block{}({})",
            self.condition, self.destination_block, block_args_string
        )
    }
}

impl JumpOperation {
    pub fn new(destination_block: usize, condition: JumpCondition) -> Self {
        Self {
            destination_block,
            block_args: HashMap::new(),
            condition,
        }
    }
}

/// ## Enum of all operations
#[derive(Debug, Serialize, Clone)]
pub enum Operations {
    Assignment(SourceDestOperands),
    BinaryOperation(BinaryOperations),
    Jump(JumpOperation),
    FunctionCall(FunctionCallOperands),
    MethodCall(MethodCallOperands),
    FieldRead(FieldReadOperands),
    FieldWrite(FieldWriteOperands),
}

impl Display for Operations {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Assignment(assignment) => {
                write!(f, "{} = {}", assignment.destination, assignment.source)
            }
            Self::BinaryOperation(binary_operation) => write!(f, "{}", binary_operation),
            Self::Jump(jump) => write!(f, "{}", jump),
            Self::FunctionCall(function_call) => write!(f, "{}", function_call),
            Self::MethodCall(method_call) => write!(f, "{}", method_call),
            Self::FieldRead(field_read) => write!(
                f,
                "{} = {}.{}",
                field_read.destination, field_read.receiver, field_read.field_name
            ),
            Self::FieldWrite(field_write) => write!(
                f,
                "{}.{} = {}",
                field_write.receiver, field_write.field_name, field_write.source
            ),
        }
    }
}

impl Operations {
    pub fn new_assignment(destination: Operand, source: Operand) -> Self {
        Self::Assignment(SourceDestOperands {
            destination,
            source,
        })
    }

    pub fn new_jump(destination_block: usize, condition: JumpCondition) -> Self {
        Self::Jump(JumpOperation::new(destination_block, condition))
    }

    pub fn new_function_call(
        name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<Operand>,
    ) -> Self {
        Self::FunctionCall(FunctionCallOperands::new(name, arguments, return_value_to))
    }

    pub fn new_method_call(
        receiver: Operand,
        name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<Operand>,
    ) -> Self {
        Self::MethodCall(MethodCallOperands::new(
            receiver,
            name,
            arguments,
            return_value_to,
        ))
    }

    pub fn new_field_read(receiver: Operand, field_name: String, destination: Operand) -> Self {
        Self::FieldRead(FieldReadOperands {
            receiver,
            field_name,
            destination,
        })
    }

    pub fn new_field_write(source: Operand, receiver: Operand, field_name: String) -> Self {
        Self::FieldWrite(FieldWriteOperands {
            receiver,
            field_name,
            source,
        })
    }
}

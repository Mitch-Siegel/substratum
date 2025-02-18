use serde::Serialize;
use std::fmt::Display;

use super::operands::{self, *};

/// ## Binary Operations
#[derive(Debug, Serialize)]
pub enum BinaryOperations<T>
where
    T: std::fmt::Display,
{
    Add(BinaryArithmeticOperands<T>),
    Subtract(BinaryArithmeticOperands<T>),
    Multiply(BinaryArithmeticOperands<T>),
    Divide(BinaryArithmeticOperands<T>),
    LThan(BinaryArithmeticOperands<T>),
    GThan(BinaryArithmeticOperands<T>),
    LThanE(BinaryArithmeticOperands<T>),
    GThanE(BinaryArithmeticOperands<T>),
    Equals(BinaryArithmeticOperands<T>),
    NotEquals(BinaryArithmeticOperands<T>),
}

impl<T> Display for BinaryOperations<T>
where
    T: std::fmt::Display,
{
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

impl<T> BinaryOperations<T>
where
    T: std::fmt::Display,
{
    pub fn new_add(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::Add(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_subtract(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::Subtract(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_multiply(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::Multiply(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_divide(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::Divide(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_lthan(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::LThan(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_gthan(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::GThan(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_lthan_e(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::LThanE(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_gthan_e(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::GThanE(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_equals(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::Equals(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_not_equals(
        destination: operands::GenericOperand<T>,
        source_a: operands::GenericOperand<T>,
        source_b: operands::GenericOperand<T>,
    ) -> Self {
        BinaryOperations::NotEquals(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn raw_operands(&self) -> &BinaryArithmeticOperands<T> {
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
#[derive(Debug, Serialize)]
pub struct JumpOperation<T>
where
    T: std::fmt::Display,
{
    pub destination_block: usize,
    pub condition: JumpCondition<T>,
}

impl<T> Display for JumpOperation<T>
where
    T: std::fmt::Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}", self.condition, self.destination_block)
    }
}

/// ## Enum of all operations
#[derive(Debug, Serialize)]
pub enum IROperations<T>
where
    T: std::fmt::Display,
{
    Assignment(SourceDestOperands<T>),
    BinaryOperation(BinaryOperations<T>),
    Jump(JumpOperation<T>),
}

impl<T> Display for IROperations<T>
where
    T: std::fmt::Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Assignment(assignment) => {
                write!(f, "{} = {}", assignment.destination, assignment.source)
            }
            Self::BinaryOperation(binary_operation) => {
                write!(f, "{}", binary_operation)
            }
            Self::Jump(jump) => {
                write!(f, "{}", jump)
            }
        }
    }
}

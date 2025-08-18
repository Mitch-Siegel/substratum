use serde::Serialize;
use std::{collections::HashMap, fmt::Display};

use crate::midend::ir::{lowered::*, *};

///
/// ## Binary Operations
///

/// ### Binary Arithmetic

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum BinaryArithmeticKind {
    Add,
    Sub,
    Mul,
    Div,
}

impl Display for BinaryArithmeticKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::Add => "+",
                Self::Sub => "-",
                Self::Mul => "*",
                Self::Div => "/",
            }
        )
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct BinaryArithmeticOperands {
    pub destination: ValueId,
    pub sources: DualSourceOperands,
    pub kind: BinaryArithmeticKind,
}

impl Display for BinaryArithmeticOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{} = {} {} {}",
            self.destination, self.sources.a, self.kind, self.sources.b
        )
    }
}

impl BinaryArithmeticOperands {
    fn new(
        destination: ValueId,
        source_a: ValueId,
        source_b: ValueId,
        kind: BinaryArithmeticKind,
    ) -> Self {
        Self {
            destination,
            sources: DualSourceOperands::new(source_a, source_b),
            kind,
        }
    }

    pub fn new_add(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryArithmeticKind::Add)
    }

    pub fn new_sub(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryArithmeticKind::Sub)
    }

    pub fn new_mul(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryArithmeticKind::Mul)
    }

    pub fn new_div(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryArithmeticKind::Div)
    }
}

/// ### Binary Comparison

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum BinaryComparisonKind {
    LT,
    GT,
    LE,
    GE,
    EQ,
    NE,
}

impl Display for BinaryComparisonKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::LT => "<",
                Self::GT => ">",
                Self::LE => "<=",
                Self::GE => ">=",
                Self::EQ => "==",
                Self::NE => "!=",
            }
        )
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct BinaryComparisonOperands {
    pub destination: ValueId,
    pub sources: DualSourceOperands,
    pub kind: BinaryComparisonKind,
}

impl Display for BinaryComparisonOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{} = {} {} {}",
            self.destination, self.sources.a, self.kind, self.sources.b
        )
    }
}

impl BinaryComparisonOperands {
    fn new(
        destination: ValueId,
        source_a: ValueId,
        source_b: ValueId,
        kind: BinaryComparisonKind,
    ) -> Self {
        Self {
            destination,
            sources: DualSourceOperands::new(source_a, source_b),
            kind,
        }
    }

    pub fn new_lt(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryComparisonKind::LT)
    }

    pub fn new_gt(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryComparisonKind::GT)
    }

    pub fn new_le(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryComparisonKind::LE)
    }

    pub fn new_ge(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryComparisonKind::GE)
    }

    pub fn new_eq(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryComparisonKind::EQ)
    }

    pub fn new_ne(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        Self::new(destination, source_a, source_b, BinaryComparisonKind::NE)
    }
}

/// ## Jump
#[derive(Debug, Serialize, Clone, PartialEq, Eq)]
pub struct JumpOperation {
    pub destination_block: usize,
    pub block_args: HashMap<ValueId, ValueId>,
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

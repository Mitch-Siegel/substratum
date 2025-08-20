use serde::Serialize;
use std::{collections::HashMap, fmt::Display};

use crate::midend::ir::{lowered::*, *};

///
/// ## Binary Operations
///

/// ### Binary Arithmetic

/// ### Binary Comparison

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

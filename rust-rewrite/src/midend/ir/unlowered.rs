use crate::midend::ir::*;
use serde::Serialize;

pub mod operands;
use operands::*;

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum Operation {
    Match(MatchOperands),
}

impl Operation {
    pub fn read_value_ids(&self) -> Vec<ValueId> {
        vec![]
    }

    pub fn write_value_ids(&self) -> Vec<ValueId> {
        vec![]
    }
}

impl std::fmt::Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Match(_m) => write!(f, "match"),
        }
    }
}

pub fn new_match(scrutinee: ValueId, arms: Vec<MatchArm>) -> Operation {
    Operation::Match(MatchOperands { scrutinee, arms })
}

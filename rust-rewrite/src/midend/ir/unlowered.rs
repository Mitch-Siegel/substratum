use crate::midend::ir::*;
use serde::Serialize;

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum Operation {}

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
        unimplemented!();
    }
}

use crate::midend::ir::*;
use std::collections::{HashMap, HashSet};

#[derive(Debug, Clone, Serialize)]
pub struct ControlFlow {
    blocks: HashMap<usize, BasicBlock>,
    successors: HashMap<usize, HashSet<usize>>,
    predecessors: HashMap<usize, HashSet<usize>>,
}

impl From<HashMap<usize, BasicBlock>> for ControlFlow {
    fn from(blocks: HashMap<usize, BasicBlock>) -> Self {
        Self {
            blocks: HashMap::new(),
            successors: HashMap::new(),
            predecessors: HashMap::new(),
        }
    }
}

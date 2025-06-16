use crate::midend::ir::*;
use std::collections::{HashMap, HashSet};

#[derive(Debug, Clone, Serialize)]
pub struct ControlFlow {
    pub blocks: HashMap<usize, BasicBlock>,
    pub successors: HashMap<usize, HashSet<usize>>,
    pub predecessors: HashMap<usize, HashSet<usize>>,
}

impl From<HashMap<usize, BasicBlock>> for ControlFlow {
    fn from(blocks: HashMap<usize, BasicBlock>) -> Self {
        let mut successors = HashMap::<usize, HashSet<usize>>::new();
        let mut predecessors = HashMap::<usize, HashSet<usize>>::new();

        for block in blocks.values() {
            for statement in block {
                match &statement.operation {
                    ir::Operations::Jump(jump) => {
                        successors
                            .entry(block.label)
                            .or_default()
                            .insert(jump.destination_block);

                        if !blocks.contains_key(&jump.destination_block) {
                            panic!(
                                "Invalid jump target to nonexistent block {}",
                                jump.destination_block
                            );
                        }
                    }
                    _ => (),
                }
            }
        }

        Self {
            blocks,
            successors,
            predecessors,
        }
    }
}

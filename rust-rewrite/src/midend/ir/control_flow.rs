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

        for label in blocks.keys() {
            predecessors.entry(*label).or_default();
            successors.entry(*label).or_default();
        }

        for from_block in blocks.values() {
            for statement in from_block {
                match &statement.operation {
                    ir::Operations::Jump(jump) => {
                        successors
                            .get_mut(&from_block.label)
                            .unwrap()
                            .insert(jump.destination_block);

                        predecessors
                            .get_mut(&jump.destination_block)
                            .unwrap()
                            .insert(from_block.label);

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

impl<'a> IntoIterator for &'a ControlFlow {
    type Item = &'a BasicBlock;
    type IntoIter = std::collections::hash_map::Values<'a, usize, BasicBlock>;
    fn into_iter(self) -> Self::IntoIter {
        self.blocks.values()
    }
}

impl<'a> IntoIterator for &'a mut ControlFlow {
    type Item = &'a mut BasicBlock;
    type IntoIter = std::collections::hash_map::ValuesMut<'a, usize, BasicBlock>;
    fn into_iter(self) -> Self::IntoIter {
        self.blocks.values_mut()
    }
}

use crate::{hashmap_ooo_iter::*, midend::ir::*};
use std::collections::{HashMap, HashSet, VecDeque};

#[derive(Debug, Clone, Serialize)]
pub struct ControlFlow {
    blocks: HashMap<usize, BasicBlock>,
    successors: HashMap<usize, HashSet<usize>>,
    predecessors: HashMap<usize, HashSet<usize>>,
}

pub struct ControlFlowIntoIter<T> {
    postorder_stack: VecDeque<T>,
}

impl<T> Iterator for ControlFlowIntoIter<T> {
    type Item = T;

    fn next(&mut self) -> Option<Self::Item> {
        self.postorder_stack.pop_back()
    }
}

// TODO: are the postorder and reverse postorder named opposite right now? Need to actually check this...
impl ControlFlow {
    pub fn successors(&self, label: &usize) -> &HashSet<usize> {
        self.successors.get(label).unwrap()
    }

    pub fn predecessors(&self, label: &usize) -> &HashSet<usize> {
        self.predecessors.get(label).unwrap()
    }

    fn generate_postorder_stack(&self) -> Vec<usize> {
        let mut postorder_stack = Vec::<usize>::new();
        postorder_stack.clear();
        let mut visited = HashSet::<usize>::new();

        let mut dfs_stack = Vec::<usize>::new();
        dfs_stack.push(0);

        // go until done
        while dfs_stack.len() > 0 {
            match dfs_stack.pop() {
                Some(label) => {
                    // only visit once
                    if !visited.contains(&label) {
                        visited.insert(label);

                        postorder_stack.push(label);

                        for successor in self.successors(&label) {
                            dfs_stack.push(*successor);
                        }
                    }
                }
                None => {}
            }
        }
        postorder_stack
    }

    pub fn blocks_postorder(&self) -> HashMapOOOIter<usize, ir::BasicBlock> {
        let postorder_stack = self.generate_postorder_stack();

        HashMapOOOIter::new(&self.blocks, postorder_stack)
    }

    pub fn blocks_postorder_mut(&mut self) -> HashMapOOOIterMut<usize, ir::BasicBlock> {
        let postorder_stack = self.generate_postorder_stack();

        HashMapOOOIterMut::new(&mut self.blocks, postorder_stack)
    }

    pub fn blocks_reverse_postorder(&self) -> HashMapOOOIter<usize, ir::BasicBlock> {
        let reverse_postorder_stack = self.generate_postorder_stack().into_iter().rev().collect();

        HashMapOOOIter::new(&self.blocks, reverse_postorder_stack)
    }

    pub fn blocks_reverse_postorder_mut(&mut self) -> HashMapOOOIterMut<usize, ir::BasicBlock> {
        let reverse_postorder_stack = self.generate_postorder_stack().into_iter().rev().collect();

        HashMapOOOIterMut::new(&mut self.blocks, reverse_postorder_stack)
    }
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

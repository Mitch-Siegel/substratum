use std::{
    collections::{BTreeMap, VecDeque},
    usize,
};

use crate::midend::ir;

#[derive(Debug)]
struct BlockDepthMetadata<'a> {
    depths: BTreeMap<usize, usize>,
    control_flow: &'a ir::ControlFlow,
    unknown_predecessors_worklist: VecDeque<usize>,
}
impl<'a> BlockDepthMetadata<'a> {
    fn new(control_flow: &'a ir::ControlFlow) -> Self {
        Self {
            depths: BTreeMap::new(),
            control_flow,
            unknown_predecessors_worklist: VecDeque::new(),
        }
    }

    fn set_block_depth(&mut self, block_label: usize, depth: usize) {
        self.depths.insert(block_label, depth);
    }

    fn max_of_predecessors(&mut self, block_label: usize) -> usize {
        let mut max = usize::MIN;

        for predecessor_label in &self.control_flow.block_for_label(&block_label).predecessors {
            let predecessor_depth = match self.depths.get(predecessor_label) {
                Some(depth) => *depth,
                None => {
                    self.unknown_predecessors_worklist
                        .push_back(*predecessor_label);
                    usize::MIN
                }
            };

            max = usize::max(predecessor_depth, max);
        }

        max
    }

    fn visit(&mut self, block_label: usize) {
        let max_predecessor_depth = self.max_of_predecessors(block_label);
        self.set_block_depth(block_label, max_predecessor_depth + 1);
    }
}

pub fn find_block_depths(control_flow: &ir::ControlFlow) -> BTreeMap<usize, usize> {
    let mut metadata = BlockDepthMetadata::new(control_flow);

    for (label, _) in control_flow.blocks_postorder() {
        metadata.visit(label);
    }

    while let Some(unvisited) = metadata.unknown_predecessors_worklist.pop_front() {
        metadata.visit(unvisited);
    }

    metadata.depths
}

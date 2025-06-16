use crate::{frontend::sourceloc::SourceLoc, midend::ir, trace};

use serde::Serialize;
use std::{
    collections::{HashMap, VecDeque},
    fmt::Debug,
    usize,
};

#[derive(Debug, Clone, Serialize)]
pub struct BlockManager {
    // map from branch origin to (true_target, Option<false_target>)
    branch_points: HashMap<usize, (usize, Option<usize>)>,
    convergence_points: HashMap<usize, usize>, // map of label -> label that block should jump to when done
    max_block: usize,
    temp_num: usize,
}

impl BlockManager {
    // returns (Self, start_block, end_block)
    // where start_block is the first basic block in the function and end_block is the last
    pub fn new() -> (Self, ir::BasicBlock, ir::BasicBlock) {
        let mut convergence_points = HashMap::<usize, usize>::new();
        convergence_points.insert(0, 1);

        let start_block = ir::BasicBlock::new(0);
        let end_block = ir::BasicBlock::new(1);
        (
            Self {
                branch_points: HashMap::new(),
                convergence_points,
                max_block: 1,
                temp_num: 0,
            },
            start_block,
            end_block,
        )
    }

    // returns (branch_target, after_branch)
    pub fn create_unconditional_branch(
        &mut self,
        from_block: &mut ir::BasicBlock,
        loc: SourceLoc,
    ) -> (ir::BasicBlock, ir::BasicBlock) {
        self.max_block += 2;
        let true_block = ir::BasicBlock::new(self.max_block - 1);
        let after_branch = ir::BasicBlock::new(self.max_block);

        trace::trace!(
            "unconditional branch from block {} - true block: {}, after branch: {}",
            from_block.label,
            true_block.label,
            after_branch.label
        );

        let unconditional_jump = ir::IrLine::new_jump(
            loc,
            true_block.label,
            ir::operands::JumpCondition::Unconditional,
        );
        from_block.statements.push(unconditional_jump);

        (true_block, after_branch)
    }

    // returns (condition_true_target, condition_false_target, convergence)
    pub fn create_conditional_branch(
        &mut self,
        from_block: &mut ir::BasicBlock,
        loc: SourceLoc,
        jump_condition: ir::operands::JumpCondition,
    ) -> (ir::BasicBlock, ir::BasicBlock, ir::BasicBlock) {
        self.max_block += 3;
        let true_block = ir::BasicBlock::new(self.max_block - 2);
        let false_block = ir::BasicBlock::new(self.max_block - 1);
        let convergence_block = ir::BasicBlock::new(self.max_block);

        trace::trace!(
            "create conditional branch from block {} - true block: {}, false block: {}, after branch: {}",
            from_block.label,
            true_block.label,
            false_block.label,
            convergence_block.label
        );

        let conditional_jump = ir::IrLine::new_jump(loc, true_block.label, jump_condition);
        from_block.statements.push(conditional_jump);
        let unconditional_jump = ir::IrLine::new_jump(
            loc,
            false_block.label,
            ir::operands::JumpCondition::Unconditional,
        );
        from_block.statements.push(unconditional_jump);

        (true_block, false_block, convergence_block)
    }

    // returns (loop_top, loop_bottom, after_loop)
    pub fn create_loop(
        &mut self,
        from_block: &mut ir::BasicBlock,
        loc: SourceLoc,
    ) -> (ir::BasicBlock, ir::BasicBlock, ir::BasicBlock) {
        self.max_block += 3;
        let loop_top = ir::BasicBlock::new(self.max_block - 2);
        let mut loop_bottom = ir::BasicBlock::new(self.max_block - 1);
        let after_loop = ir::BasicBlock::new(self.max_block);

        trace::trace!(
            "create loop from block {} - loop top: {}, loop bottom: {}, after loop: {}",
            from_block.label,
            loop_top.label,
            loop_bottom.label,
            after_loop.label,
        );

        let loop_entry = ir::IrLine::new_jump(
            loc,
            loop_top.label,
            ir::operands::JumpCondition::Unconditional,
        );
        from_block.statements.push(loop_entry);

        let loop_jump = ir::IrLine::new_jump(
            loc,
            loop_top.label,
            ir::operands::JumpCondition::Unconditional,
        );

        loop_bottom.statements.push(loop_jump);

        (loop_top, loop_bottom, after_loop)
    }
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
/*
// TODO: are the postorder and reverse postorder named opposite right now? Need to actually check this...
impl ControlFlow {
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

                        for successor in &self.block_for_label(&label).successors {
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
}*/

#[cfg(test)]
mod tests {}

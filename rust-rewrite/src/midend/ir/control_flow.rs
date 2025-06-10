use crate::frontend::sourceloc::SourceLoc;

use super::ir;
use serde::Serialize;
use std::{
    collections::{HashMap, VecDeque},
    fmt::Debug,
    usize,
};

/*
    A ControlFlow represents the notion of ownership over basic blocks. At the end of linearization of a section of code,
    all relevant basic blocks will be owned by a single control flow. During linearization control flows can branch
    correspondingly with actual branches in the code, as they are linarized. Each branch gets its own control flow,
    which owns only the blocks relevant to the contents of branch. When the linearization of each branch is complete,
    its control flow is merged back to the one from which it was branched. The original brancher then takes ownership
    of all blocks of the branchee.
*/

#[derive(Debug, Serialize)]
pub struct ControlFlow {
    // map from branch origin to (true_target, Option<false_target>)
    branch_points: HashMap<usize, (usize, Option<usize>)>,
    convergence_points: HashMap<usize, usize>, // map of label -> label that block should jump to when done
    max_block: usize,
    temp_num: usize,
}

impl ControlFlow {
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
        let mut true_block = ir::BasicBlock::new(self.max_block - 1);
        let after_branch = ir::BasicBlock::new(self.max_block);

        from_block.successors.insert(true_block.label);
        true_block.predecessors.insert(from_block.label);

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
        let mut true_block = ir::BasicBlock::new(self.max_block - 2);
        let mut false_block = ir::BasicBlock::new(self.max_block - 1);
        let convergence_block = ir::BasicBlock::new(self.max_block);

        from_block.successors.insert(true_block.label);
        from_block.successors.insert(false_block.label);

        true_block.predecessors.insert(from_block.label);
        false_block.predecessors.insert(from_block.label);

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
        let loop_bottom = ir::BasicBlock::new(self.max_block - 1);
        let after_loop = ir::BasicBlock::new(self.max_block);

        /*let loop_jump = ir::new_jump(
            loc,
            loop_top.label,
            ir::operands::JumpCondition::Unconditional,
        );

        from_block.successors.insert(loop_top.label);
        // don't include the loop bottom as a predecessor of the loop top (TODO: for now?)
        loop_top.predecessors.insert(from_block.label);
        from_block.statements.append(loop_jump.clone());

        loop_bottom.successors.insert(loop_top);
        loop_bottom.append(loop_jump);*/

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

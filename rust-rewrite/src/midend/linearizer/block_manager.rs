use crate::{frontend::sourceloc::SourceLocWithMod, midend::ir, trace};

use std::{collections::HashMap, fmt::Debug};

pub mod block_convergences;
mod branch_error;
mod convergence_error;

use block_convergences::BlockConvergences;
pub use block_convergences::ConvergenceResult;
pub use branch_error::BranchError;
pub use convergence_error::ConvergenceError;

#[derive(Debug, PartialEq, Eq)]
pub enum LoopError {
    NotLooping,
    Convergence(ConvergenceError),
    LoopBottomNotDone(usize), // if the loop bottom convergence returns as "not done", the label
    // which the BlockConvergences says we expect to converge to
    LoopInsideNotDone(usize), // same as LoopBottomNotDone but for the loop body itself
}
impl From<ConvergenceError> for LoopError {
    fn from(e: ConvergenceError) -> Self {
        Self::Convergence(e)
    }
}
#[derive(Debug)]
pub struct BlockManager {
    // map from branch origin to (true_target, Option<false_target>)
    branch_points: HashMap<usize, (usize, Option<usize>)>,
    convergences: BlockConvergences,
    max_block: usize,
    temp_num: usize,
    open_loop_beginnings: Vec<usize>,
    // branch path of basic block labels targeted by the branches which got us to current_block
    open_branch_path: Vec<usize>,
    // conditional branches from source block to the branch_false block.
    // creating a branch replaces current_block with the branch_true block,
    // but we also need to track the branch_false block if it exists
    open_branch_false_blocks: HashMap<usize, ir::BasicBlock>,
}

impl BlockManager {
    // returns (Self, start_block)
    // where start_block is the first basic block in the function
    pub fn new() -> (Self, ir::BasicBlock) {
        // set up the initlal convergence - must always end up at the end_block
        let start_block = ir::BasicBlock::new(0);
        let end_block = ir::BasicBlock::new(1);
        let mut convergences = BlockConvergences::new();
        convergences.add(&[start_block.label], end_block).unwrap();

        (
            Self {
                branch_points: HashMap::new(),
                convergences,
                max_block: 1,
                temp_num: 0,
                open_loop_beginnings: Vec::new(),
                open_branch_path: Vec::new(),
                open_branch_false_blocks: HashMap::new(),
            },
            start_block,
        )
    }

    // returns (branch_target, after_branch)
    pub fn create_unconditional_branch(
        &mut self,
        from_block: &mut ir::BasicBlock,
        loc: SourceLocWithMod,
    ) -> Result<ir::BasicBlock, BranchError> {
        self.max_block += 2;
        let true_block = ir::BasicBlock::new(self.max_block - 1);
        let after_branch = ir::BasicBlock::new(self.max_block);
        let after_branch_label = after_branch.label;

        self.convergences
            .rename_source(from_block.label, after_branch.label);
        self.convergences.add(&[true_block.label], after_branch)?;

        self.open_branch_path.push(from_block.label);

        trace::trace!(
            "unconditional branch from block {} - true block: {}, after branch: {}",
            from_block.label,
            true_block.label,
            after_branch_label
        );

        let unconditional_jump = ir::IrLine::new_jump(
            loc,
            true_block.label,
            ir::operands::JumpCondition::Unconditional,
        );
        from_block.statements.push(unconditional_jump);

        Ok(true_block)
    }

    // returns (condition_true_target, condition_false_target, convergence)
    pub fn create_conditional_branch(
        &mut self,
        from_block: &mut ir::BasicBlock,
        loc: SourceLocWithMod,
        jump_condition: ir::operands::JumpCondition,
    ) -> Result<ir::BasicBlock, BranchError> {
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

        let conditional_jump = ir::IrLine::new_jump(loc.clone(), true_block.label, jump_condition);
        from_block.statements.push(conditional_jump);
        let unconditional_jump = ir::IrLine::new_jump(
            loc,
            false_block.label,
            ir::operands::JumpCondition::Unconditional,
        );
        from_block.statements.push(unconditional_jump);

        self.open_branch_path.push(from_block.label);

        self.convergences
            .rename_source(from_block.label, convergence_block.label);
        self.convergences
            .add(&[true_block.label, false_block.label], convergence_block)?;

        match self
            .open_branch_false_blocks
            .insert(from_block.label, false_block)
        {
            Some(existing_false_block) => {
                return Err(BranchError::ExistingFalseBlock(
                    from_block.label,
                    existing_false_block.label,
                ))
            }
            None => (),
        };

        Ok(true_block)
    }

    pub fn finish_true_branch_switch_to_false(
        &mut self,
        current_block: &mut ir::BasicBlock,
    ) -> Result<ir::BasicBlock, BranchError> {
        let branched_from = match self.open_branch_path.last() {
            Some(branched_from) => branched_from,
            None => return Err(BranchError::NotBranched),
        };

        match self.convergences.converge(current_block.label)? {
            ConvergenceResult::NotDone(converge_to_label) => {
                let false_block = match self.open_branch_false_blocks.remove(branched_from) {
                    Some(false_block) => false_block,
                    None => return Err(BranchError::MissingFalseBlock(*branched_from)),
                };

                let convergence_jump = ir::IrLine::new_jump(
                    SourceLocWithMod::none(),
                    converge_to_label,
                    ir::JumpCondition::Unconditional,
                );

                current_block.statements.push(convergence_jump);

                Ok(false_block)
            }
            ConvergenceResult::Done(_block) => Err(BranchError::MissingFalseBlock(*branched_from)),
        }
    }

    pub fn finish_branch(
        &mut self,
        current_block: &mut ir::BasicBlock,
    ) -> Result<ir::BasicBlock, BranchError> {
        let branched_from = self.open_branch_path.last().unwrap();
        trace::debug!("finish branch from block {}", branched_from);

        match self.convergences.converge(current_block.label)? {
            ConvergenceResult::Done(converge_to_block) => {
                let convergence_jump = ir::IrLine::new_jump(
                    SourceLocWithMod::none(),
                    converge_to_block.label,
                    ir::JumpCondition::Unconditional,
                );

                current_block.statements.push(convergence_jump);
                Ok(converge_to_block)
            }
            ConvergenceResult::NotDone(label) => Err(BranchError::NotDone(label)),
        }
    }

    // returns (loop_top, loop_bottom, after_loop)
    pub fn create_loop(
        &mut self,
        before_loop_block: &mut ir::BasicBlock,
        loc: SourceLocWithMod,
    ) -> Result<(ir::BasicBlock, usize), LoopError> {
        self.max_block += 3;
        let loop_top = ir::BasicBlock::new(self.max_block - 2);
        let mut loop_bottom = ir::BasicBlock::new(self.max_block - 1);
        let after_loop = ir::BasicBlock::new(self.max_block);

        trace::trace!(
            "create loop from block {} - loop top: {}, loop bottom: {}, after loop: {}",
            before_loop_block.label,
            loop_top.label,
            loop_bottom.label,
            after_loop.label,
        );

        let loop_entry = ir::IrLine::new_jump(
            loc.clone(),
            loop_top.label,
            ir::operands::JumpCondition::Unconditional,
        );
        before_loop_block.statements.push(loop_entry);

        let loop_jump = ir::IrLine::new_jump(
            loc.clone(),
            loop_top.label,
            ir::operands::JumpCondition::Unconditional,
        );

        loop_bottom.statements.push(loop_jump);

        // track the top of the loop we are opening
        self.open_loop_beginnings.push(loop_top.label);

        // transfer control flow unconditionally to the top of the loop
        let loop_entry_jump =
            ir::IrLine::new_jump(loc, loop_top.label, ir::JumpCondition::Unconditional);
        before_loop_block.statements.push(loop_entry_jump);

        // now the current block should be the loop's top
        self.convergences
            .rename_source(before_loop_block.label, after_loop.label);

        let after_loop_label = after_loop.label;

        // NB the convergence here is handled a bit differently for loops, since we might want a
        // condition check either at the top or at the bottom of the loop. In any case, the
        // finish_loop() handling of the convergence mirrors this scheme so the handling is valid.
        // the bottom of the loop should converge to after the loop
        self.convergences.add(&[loop_bottom.label], after_loop)?;

        // the top of the loop should converge to the bottom of the loop
        self.convergences.add(&[loop_top.label], loop_bottom)?;

        Ok((loop_top, after_loop_label))
    }

    pub fn finish_loop_1(
        &mut self,
        current_block: &mut ir::BasicBlock,
        loc: SourceLocWithMod,
    ) -> Result<ir::BasicBlock, LoopError> {
        // wherever the current block ends up, it should have convergence as Done to loop_bottom
        // per create_loop() as each loop convergence is singly-associated
        match self.convergences.converge(current_block.label)? {
            ConvergenceResult::Done(loop_bottom) => {
                // transfer control flow from the current block to loop_bottom
                let loop_bottom_jump =
                    ir::IrLine::new_jump(loc, loop_bottom.label, ir::JumpCondition::Unconditional);
                current_block.statements.push(loop_bottom_jump);

                Ok(loop_bottom)
            }

            ConvergenceResult::NotDone(label) => Err(LoopError::LoopInsideNotDone(label)),
        }
    }

    pub fn finish_loop_2(
        &mut self,
        current_block: &mut ir::BasicBlock,
        loc: SourceLocWithMod,
        loop_bottom_actions: Vec<ir::IrLine>,
    ) -> Result<ir::BasicBlock, LoopError> {
        // insert any IRs that need to be at the bottom of the loop but before the looping jump itself
        for loop_bottom_ir in loop_bottom_actions {
            current_block.statements.push(loop_bottom_ir);
        }

        // figure out where the top of our loop is to jump back to
        let loop_top = match self.open_loop_beginnings.pop() {
            Some(top) => top,
            None => return Err(LoopError::NotLooping),
        };

        let loop_jump = ir::IrLine::new_jump(loc, loop_top, ir::JumpCondition::Unconditional);
        current_block.statements.push(loop_jump);

        // now that we are in loop_bottom, create_loop() should have a convergence for us
        // which will give us the after_loop block
        match self.convergences.converge(current_block.label)? {
            ConvergenceResult::Done(after_loop) => {
                // transition to after_loop, assuming that the correct IR was inserted to
                // break out of the loop at some point within the loop or by
                // loop_bottom_actions
                Ok(after_loop)
            }
            ConvergenceResult::NotDone(label) => Err(LoopError::LoopBottomNotDone(label)),
        }
    }
}

#[cfg(test)]
mod tests {}

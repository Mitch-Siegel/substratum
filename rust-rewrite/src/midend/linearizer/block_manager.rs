use crate::{frontend::sourceloc::SourceLoc, midend::ir, trace};

use std::{collections::HashMap, fmt::Debug};

pub mod block_convergences;
mod branch_error;
mod convergence_error;

use block_convergences::BlockConvergences;
pub use block_convergences::ConvergenceResult;
pub use branch_error::BranchError;
pub use convergence_error::ConvergenceError;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BranchKind {
    Unconditional,
    ConditionalTrue(ir::BasicBlock), // currently on the true branch of a conditional. Owns the
    // block targeted by the false branch
    ConditionalFalse, // currently on the false branch of a conditional
    Switch(usize),    // within a switch but not one of its cases - owns the label of the switch
    // block
    SwitchCase(usize), // within a switch and inside one of its cases - owns the
    // label of the switch block
    Loop,
    BlockSplit(Vec<ir::IrLine>), // a block has been split into two. The statements after the split
                                 // (not including the statement which was split on) are owned by
                                 // the BlockSplit.
}

#[derive(Debug)]
struct Branch {
    from_label: usize,
    kind: BranchKind,
}

impl Branch {
    pub fn new(from_label: usize, kind: BranchKind) -> Self {
        Self { from_label, kind }
    }
}

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
    // branch path of basic block labels targeted by the branches which got us to current_block
    open_branch_path: Vec<Branch>,
    blocks: HashMap<usize, ir::BasicBlock>,
}

impl BlockManager {
    // returns (Self, start_block)
    // where start_block is the first basic block in the function
    pub fn new() -> (Self, usize) {
        // set up the initlal convergence - must always end up at the end_block
        let start_block = ir::BasicBlock::new(0);
        let end_block = ir::BasicBlock::new(1);
        let mut convergences = BlockConvergences::new();
        convergences.add(&[start_block.label], end_block).unwrap();

        let start_label = start_block.label;
        (
            Self {
                branch_points: HashMap::new(),
                convergences,
                max_block: 1,
                open_branch_path: Vec::new(),
                blocks: vec![(start_block.label, start_block)].into_iter().collect(),
            },
            start_label,
        )
    }

    pub fn with_existing_blocks(mut from_blocks: impl Iterator<Item = ir::BasicBlock>) -> Self {
        let mut max_block = 0;
        let mut blocks = HashMap::<usize, ir::BasicBlock>::new();
        while let Some(block) = from_blocks.next() {
            max_block = max_block.max(block.label);

            match blocks.insert(block.label, block) {
                Some(existing) => panic!("Block label {} duplicated", existing.label),
                None => (),
            }
        }

        Self {
            branch_points: HashMap::new(),
            convergences: BlockConvergences::new(),
            max_block,
            open_branch_path: Vec::new(),
            blocks,
        }
    }

    pub fn get_mut(&mut self, label: &usize) -> Option<&mut ir::BasicBlock> {
        self.blocks.get_mut(label)
    }

    fn last_branch(&self) -> Result<&Branch, BranchError> {
        match self.open_branch_path.last() {
            Some(branch) => Ok(branch),
            None => Err(BranchError::NotBranched),
        }
    }

    fn pop_last_branch(&mut self) -> Result<Branch, BranchError> {
        match self.open_branch_path.pop() {
            Some(branch) => Ok(branch),
            None => Err(BranchError::NotBranched),
        }
    }

    // wrapper around calls to self.convergences.converge(from)
    // automatically appends an unconditional jump from 'from' to wherever it converges to
    fn converge_with_jump(
        &mut self,
        from: usize,
        loc: SourceLoc,
    ) -> Result<ConvergenceResult, ConvergenceError> {
        let result = self.convergences.converge(from)?;

        let converge_to = match &result {
            ConvergenceResult::Done(block) => block.label,
            ConvergenceResult::NotDone(label) => *label,
        };

        let convergence_jump =
            ir::IrLine::new_jump(loc, converge_to, ir::lowered::JumpCondition::Unconditional);
        self.get_mut(&from)
            .unwrap()
            .statements
            .push(convergence_jump);

        Ok(result)
    }

    // returns (branch_target, after_branch)
    pub fn create_unconditional_branch(
        &mut self,
        from: usize,
        loc: SourceLoc,
    ) -> Result<usize, BranchError> {
        self.max_block += 2;
        let true_block = ir::BasicBlock::new(self.max_block - 1);
        let after_branch = ir::BasicBlock::new(self.max_block);
        let after_branch_label = after_branch.label;

        self.convergences.rename_source(from, after_branch.label);
        self.convergences.add(&[true_block.label], after_branch)?;

        self.open_branch_path
            .push(Branch::new(from, BranchKind::Unconditional));

        trace::trace!(
            "unconditional branch from block {} - true block: {}, after branch: {}",
            from,
            true_block.label,
            after_branch_label
        );

        let unconditional_jump = ir::IrLine::new_jump(
            loc,
            true_block.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        let from_block = self.get_mut(&from).unwrap();
        from_block.statements.push(unconditional_jump);

        let true_block_label = true_block.label;
        self.blocks.insert(true_block_label, true_block);
        Ok(true_block_label)
    }

    // returns the label of the true branch or BranchError
    pub fn create_conditional_branch(
        &mut self,
        from: usize,
        loc: SourceLoc,
        jump_condition: ir::lowered::operands::JumpCondition,
    ) -> Result<usize, BranchError> {
        self.max_block += 3;
        let true_block = ir::BasicBlock::new(self.max_block - 2);
        let false_block = ir::BasicBlock::new(self.max_block - 1);
        let convergence_block = ir::BasicBlock::new(self.max_block);

        trace::trace!(
            "create conditional branch from block {} - true block: {}, false block: {}, after branch: {}",
            from,
            true_block.label,
            false_block.label,
            convergence_block.label
        );

        let conditional_jump = ir::IrLine::new_jump(loc.clone(), true_block.label, jump_condition);
        let from_block = self.get_mut(&from).unwrap();
        from_block.statements.push(conditional_jump);
        let unconditional_jump = ir::IrLine::new_jump(
            loc,
            false_block.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        from_block.statements.push(unconditional_jump);

        self.convergences
            .rename_source(from, convergence_block.label);
        self.convergences
            .add(&[true_block.label, false_block.label], convergence_block)?;

        self.open_branch_path
            .push(Branch::new(from, BranchKind::ConditionalTrue(false_block)));

        let true_label = true_block.label;
        self.blocks.insert(true_label, true_block);
        Ok(true_label)
    }

    // returns the label of the false branch or BranchError
    pub fn finish_true_branch_switch_to_false(
        &mut self,
        true_end_label: usize,
        loc: SourceLoc,
    ) -> Result<usize, BranchError> {
        let finished_branch = self.pop_last_branch()?;
        let branched_from = finished_branch.from_label;
        let false_block = match finished_branch.kind {
            BranchKind::ConditionalTrue(false_block) => Ok(false_block),
            kind => Err(BranchError::WrongKind(
                kind,
                vec![BranchKind::ConditionalTrue(ir::BasicBlock::new(0))],
            )),
        }?;

        match self.converge_with_jump(true_end_label, loc)? {
            ConvergenceResult::NotDone(_) => Ok(()),
            ConvergenceResult::Done(_) => Err(BranchError::MissingFalseBlock(branched_from)),
        }?;

        self.open_branch_path
            .push(Branch::new(branched_from, BranchKind::ConditionalFalse));

        let false_label = false_block.label;
        self.blocks.insert(false_label, false_block);
        Ok(false_label)
    }

    pub fn finish_branch(
        &mut self,
        branch_end_label: usize,
        loc: SourceLoc,
    ) -> Result<usize, BranchError> {
        let branched_from = self.last_branch()?.from_label;
        trace::debug!("finish branch from block {}", branched_from);

        match self.open_branch_path.pop() {
            Some(branch) => match branch.kind {
                BranchKind::Unconditional | BranchKind::ConditionalFalse => Ok(()),
                kind => Err(BranchError::WrongKind(
                    kind,
                    vec![BranchKind::Unconditional, BranchKind::ConditionalFalse],
                )),
            },
            None => Err(BranchError::NotBranched),
        }?;

        match self.converge_with_jump(branch_end_label, loc)? {
            ConvergenceResult::Done(converge_to_block) => {
                let converge_to_label = converge_to_block.label;
                self.blocks.insert(converge_to_label, converge_to_block);
                Ok(converge_to_label)
            }
            ConvergenceResult::NotDone(label) => Err(BranchError::NotDone(label)),
        }
    }

    // returns (loop_top, loop_bottom, after_loop)
    pub fn create_loop(
        &mut self,
        before_loop: usize,
        loc: SourceLoc,
    ) -> Result<(usize, usize), LoopError> {
        self.max_block += 3;
        let loop_top = ir::BasicBlock::new(self.max_block - 2);
        let mut loop_bottom = ir::BasicBlock::new(self.max_block - 1);
        let after_loop = ir::BasicBlock::new(self.max_block);

        trace::trace!(
            "create loop from block {} - loop top: {}, loop bottom: {}, after loop: {}",
            before_loop,
            loop_top.label,
            loop_bottom.label,
            after_loop.label,
        );

        // track the top of the loop we are opening
        self.open_branch_path.push(Branch {
            from_label: loop_top.label,
            kind: BranchKind::Loop,
        });

        let loop_entry = ir::IrLine::new_jump(
            loc.clone(),
            loop_top.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        let before_loop_block = self.get_mut(&before_loop).unwrap();
        before_loop_block.statements.push(loop_entry);

        let loop_jump = ir::IrLine::new_jump(
            loc.clone(),
            loop_top.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );

        loop_bottom.statements.push(loop_jump);

        // transfer control flow unconditionally to the top of the loop
        let loop_entry_jump = ir::IrLine::new_jump(
            loc,
            loop_top.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        before_loop_block.statements.push(loop_entry_jump);

        // now the current block should be the loop's top
        self.convergences
            .rename_source(before_loop, after_loop.label);

        let after_loop_label = after_loop.label;

        // NB the convergence here is handled a bit differently for loops, since we might want a
        // condition check either at the top or at the bottom of the loop. In any case, the
        // finish_loop() handling of the convergence mirrors this scheme so the handling is valid.
        // the bottom of the loop should converge to after the loop
        self.convergences.add(&[loop_bottom.label], after_loop)?;

        // the top of the loop should converge to the bottom of the loop
        self.convergences.add(&[loop_top.label], loop_bottom)?;

        let loop_top_label = loop_top.label;
        self.blocks.insert(loop_top_label, loop_top);
        Ok((loop_top_label, after_loop_label))
    }

    pub fn finish_loop_1(
        &mut self,
        loop_end_pre_bottom: usize,
        loc: SourceLoc,
    ) -> Result<usize, LoopError> {
        // wherever the current block ends up, it should have convergence as Done to loop_bottom
        // per create_loop() as each loop convergence is singly-associated
        match self.converge_with_jump(loop_end_pre_bottom, loc)? {
            ConvergenceResult::Done(loop_bottom) => {
                // transfer control flow from the current block to loop_bottom
                let loop_bottom_label = loop_bottom.label;
                self.blocks.insert(loop_bottom_label, loop_bottom);
                Ok(loop_bottom_label)
            }

            ConvergenceResult::NotDone(label) => Err(LoopError::LoopInsideNotDone(label)),
        }
    }

    pub fn finish_loop_2(
        &mut self,
        loop_bottom: usize,
        loc: SourceLoc,
        loop_bottom_actions: Vec<ir::IrLine>,
    ) -> Result<usize, BranchError> {
        let loop_bottom_block = self.get_mut(&loop_bottom).unwrap();
        // insert any IRs that need to be at the bottom of the loop but before the looping jump itself
        for loop_bottom_ir in loop_bottom_actions {
            loop_bottom_block.statements.push(loop_bottom_ir);
        }

        // figure out where the top of our loop is to jump back to
        let loop_top = match self.open_branch_path.pop() {
            Some(branch) => match branch.kind {
                BranchKind::Loop => Ok(branch.from_label),
                kind => Err(BranchError::WrongKind(kind, vec![BranchKind::Loop])),
            },
            None => return Err(BranchError::NotBranched),
        }?;

        let loop_jump = ir::IrLine::new_jump(
            loc.clone(),
            loop_top,
            ir::lowered::operands::JumpCondition::Unconditional,
        );

        let loop_bottom_block = self.get_mut(&loop_bottom).unwrap();
        loop_bottom_block.statements.push(loop_jump);

        // now that we are in loop_bottom, create_loop() should have a convergence for us
        // which will give us the after_loop block
        match self.converge_with_jump(loop_bottom, loc)? {
            ConvergenceResult::Done(after_loop) => {
                // transition to after_loop, assuming that the correct IR was inserted to
                // break out of the loop at some point within the loop or by
                // loop_bottom_actions

                let after_loop_label = after_loop.label;
                self.blocks.insert(after_loop_label, after_loop);
                Ok(after_loop_label)
            }
            ConvergenceResult::NotDone(label) => Err(BranchError::ConvergenceNotDone(label)),
        }
    }

    pub fn create_switch(
        &mut self,
        before_switch: usize,
        loc: SourceLoc,
    ) -> Result<usize, BranchError> {
        self.max_block += 2;
        let switch_block = ir::BasicBlock::new(self.max_block - 1);
        let convergence_block = ir::BasicBlock::new(self.max_block);

        trace::trace!(
            "create switch block {} - after switch: {}",
            switch_block.label,
            convergence_block.label,
        );

        let unconditional_jump = ir::IrLine::new_jump(
            loc,
            switch_block.label,
            ir::lowered::operands::JumpCondition::Unconditional,
        );
        let before_switch_block = self.get_mut(&before_switch).unwrap();
        before_switch_block.statements.push(unconditional_jump);

        self.convergences
            .rename_source(before_switch, convergence_block.label);
        self.convergences
            .add(&[switch_block.label], convergence_block)?;

        self.open_branch_path.push(Branch::new(
            before_switch,
            BranchKind::Switch(switch_block.label),
        ));

        let switch_label = switch_block.label;
        self.blocks.insert(switch_label, switch_block);
        Ok(switch_label)
    }

    pub fn create_switch_case(&mut self, switch_label: usize) -> Result<usize, BranchError> {
        // verify that we are in the correct state to create a new arm
        match &self.last_branch()?.kind {
            BranchKind::Switch(expected_label) => {
                if switch_label == *expected_label {
                    Ok(()) // branched AND branch kind is a switch AND labels match
                } else {
                    Err(BranchError::SwitchBlockMismatch(
                        *expected_label,
                        switch_label,
                    ))
                }
            }
            kind => Err(BranchError::WrongKind(
                kind.clone(),
                vec![BranchKind::Switch(0)],
            )),
        }?;

        self.max_block += 1;
        let case_block = ir::BasicBlock::new(self.max_block);
        let after_switch_label = self
            .convergences
            .convergence_label_of_block(&switch_label)
            .unwrap();
        self.convergences
            .supplement(&[case_block.label], *after_switch_label)?;

        self.open_branch_path.push(Branch::new(
            switch_label,
            BranchKind::SwitchCase(switch_label),
        ));

        let case_label = case_block.label;
        self.blocks.insert(case_label, case_block);
        Ok(case_label)
    }

    // returns the label of the switch block
    pub fn finish_switch_case(
        &mut self,
        last_block_label_in_case: usize,
        loc: SourceLoc,
    ) -> Result<usize, BranchError> {
        let switch_label = match self.pop_last_branch()?.kind {
            BranchKind::SwitchCase(label) => Ok(label),
            kind => Err(BranchError::WrongKind(
                kind,
                vec![BranchKind::SwitchCase(0)],
            )),
        }?;

        match self.converge_with_jump(last_block_label_in_case, loc)? {
            ConvergenceResult::NotDone(_) => Ok(()),
            ConvergenceResult::Done(block) => Err(BranchError::ConvergenceDone(block)),
        }?;

        Ok(switch_label)
    }

    // returns the label of the block after the switch
    pub fn finish_switch(
        &mut self,
        switch_label: usize,
        loc: SourceLoc,
    ) -> Result<usize, BranchError> {
        let last_branch = self.pop_last_branch()?;

        match last_branch.kind {
            BranchKind::Switch(expected_label) => {
                if switch_label == expected_label {
                    Ok(())
                } else {
                    Err(BranchError::SwitchBlockMismatch(
                        expected_label,
                        switch_label,
                    ))
                }
            }
            kind => Err(BranchError::WrongKind(kind, vec![BranchKind::Switch(0)])),
        }?;

        match self.converge_with_jump(switch_label, loc)? {
            ConvergenceResult::Done(after_switch_block) => {
                let after_switch_label = after_switch_block.label;

                self.blocks.insert(after_switch_label, after_switch_block);
                Ok(after_switch_label)
            }
            ConvergenceResult::NotDone(after_switch_label) => {
                Err(BranchError::ConvergenceNotDone(after_switch_label))
            }
        }
    }

    pub fn finish(&mut self, before_final_block: usize) -> Result<(), BranchError> {
        match self.converge_with_jump(before_final_block, SourceLoc::none())? {
            ConvergenceResult::Done(block) => {
                self.blocks.insert(block.label, block);
                Ok(())
            }
            ConvergenceResult::NotDone(e) => Err(BranchError::NotDone(e)),
        }
    }
}

/// implementation of manipulation functions such as splitting
impl BlockManager {
    pub fn split_block_at_statement(
        &mut self,
        block: usize,
        stmt_idx: usize,
    ) -> Result<(usize, ir::IrLine), BranchError> {
        let split_block = self.get_mut(&block).unwrap();
        let mut after_split = split_block.split_at(stmt_idx);
        let at_split = after_split.remove(0);

        self.open_branch_path
            .push(Branch::new(block, BranchKind::BlockSplit(after_split)));

        let split_to_block = self
            .create_unconditional_branch(block, at_split.loc.clone())
            .unwrap();
        Ok((split_to_block, at_split))
    }

    pub fn finish_block_split(
        &mut self,
        split_end_label: usize,
        loc: SourceLoc,
    ) -> Result<usize, BranchError> {
        let after_split_label = self.finish_branch(split_end_label, loc).unwrap();

        let last_branch = self.pop_last_branch()?;
        match last_branch.kind {
            BranchKind::BlockSplit(mut after_split_stmts) => {
                let after_split_block = self.get_mut(&after_split_label).unwrap();
                after_split_block.statements.append(&mut after_split_stmts);
                Ok(after_split_label)
            }
            wrong => Err(BranchError::WrongKind(
                wrong,
                vec![BranchKind::BlockSplit(Vec::new())],
            )),
        }
    }
}

impl TryInto<ir::ControlFlow> for BlockManager {
    type Error = ();
    fn try_into(self) -> Result<ir::ControlFlow, Self::Error> {
        if self.open_branch_path.len() > 0 {
            trace::error!("Failing due to open branch path length > 0");
            return Err(());
        }

        if !self.convergences.is_empty() {
            trace::error!("Failing due to unresolved convergences");
            return Err(());
        }

        let cf = ir::ControlFlow::from(self.blocks);
        Ok(cf)
    }
}

impl From<ir::ControlFlow> for BlockManager {
    fn from(cf: ir::ControlFlow) -> Self {
        Self::with_existing_blocks(cf.into_iter().map(|(_, block)| block))
    }
}

#[cfg(test)]
mod tests {}

use crate::{frontend::sourceloc::SourceLoc, midend::ir, trace};

use std::{collections::HashMap, fmt::Debug};

pub mod block_convergences;
mod branch;
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

#[derive(Debug)]
pub struct BlockManager {
    // map from branch origin to (true_target, Option<false_target>)
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

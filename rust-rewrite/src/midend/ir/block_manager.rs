use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{ir::*, *},
    trace,
};

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
    ConditionalTrue(BasicBlock), // currently on the true branch of a conditional. Owns the
    // block targeted by the false branch
    ConditionalFalse, // currently on the false branch of a conditional
    Switch(usize),    // within a switch but not one of its cases - owns the label of the switch
    // block
    SwitchCase(usize), // within a switch and inside one of its cases - owns the
    // label of the switch block
    Loop,
    BlockSplit(Vec<IrLine>), // a block has been split into two. The statements after the split
                             // (not including the statement which was split on) are owned by
                             // the BlockSplit.
}

#[derive(Debug, Clone)]
struct Branch {
    from_label: usize,
    kind: BranchKind,
}

impl Branch {
    pub fn new(from_label: usize, kind: BranchKind) -> Self {
        Self { from_label, kind }
    }
}

#[derive(Debug, Clone)]
pub struct BlockManager {
    // map from branch origin to (true_target, Option<false_target>)
    convergences: BlockConvergences,
    max_block: usize,
    // branch path of basic block labels targeted by the branches which got us to current_block
    open_branch_path: Vec<Branch>,
    blocks: HashMap<usize, BasicBlock>,
    values: ValueInterner,
}

impl BlockManager {
    // returns (Self, start_block)
    // where start_block is the first basic block in the function
    pub fn new(unit_type: types::Semantic, def_path: symtab::DefPath) -> (Self, usize) {
        // set up the initlal convergence - must always end up at the end_block
        let start_block = BasicBlock::new(0, def_path.clone());
        let end_block = BasicBlock::new(1, def_path);
        let mut convergences = BlockConvergences::new();
        convergences.add(&[start_block.label], end_block).unwrap();

        let start_label = start_block.label;
        (
            Self {
                convergences,
                max_block: 1,
                open_branch_path: Vec::new(),
                blocks: vec![(start_block.label, start_block)].into_iter().collect(),
                values: ValueInterner::new(unit_type),
            },
            start_label,
        )
    }

    pub fn try_take(self) -> Result<(HashMap<usize, BasicBlock>, ValueInterner), &'static str> {
        if self.open_branch_path.len() > 0 {
            let msg = "Failing due to open branch path length > 0";
            trace::error!("{}", msg);
            return Err(msg);
        }

        if !self.convergences.is_empty() {
            let msg = "Failing due to unresolved convergences";
            trace::error!("{}", msg);
            return Err(msg);
        }

        Ok((self.blocks, self.values))
    }

    pub fn with_existing_blocks(
        blocks: HashMap<usize, BasicBlock>,
        existing_values: ValueInterner,
    ) -> Self {
        let mut max_block = 0;
        for label in blocks.keys() {
            max_block = max_block.max(*label);
        }

        Self {
            convergences: BlockConvergences::new(),
            max_block,
            open_branch_path: Vec::new(),
            blocks,
            values: existing_values,
        }
    }

    pub fn values(&self) -> &ValueInterner {
        &self.values
    }

    pub fn values_mut(&mut self) -> &mut ValueInterner {
        &mut self.values
    }

    pub fn get(&self, label: &usize) -> Option<&BasicBlock> {
        self.blocks.get(label)
    }

    pub fn get_mut(&mut self, label: &usize) -> Option<&mut BasicBlock> {
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
            IrLine::new_jump(loc, converge_to, ir::lowered::JumpCondition::Unconditional);
        self.get_mut(&from).unwrap().push(convergence_jump);

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
    ) -> Result<(usize, IrLine), BranchError> {
        let split_block = self.get_mut(&block).unwrap();
        let def_path = split_block.def_path().clone();
        let mut after_split = split_block.split_at(stmt_idx);
        let at_split = after_split.remove(0);

        self.open_branch_path
            .push(Branch::new(block, BranchKind::BlockSplit(after_split)));

        let split_to_block = self
            .create_unconditional_branch(block, at_split.loc.clone(), def_path.clone(), def_path)
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
                after_split_block.append(&mut after_split_stmts);
                Ok(after_split_label)
            }
            wrong => Err(BranchError::WrongKind(
                wrong,
                vec![BranchKind::BlockSplit(Vec::new())],
            )),
        }
    }
}

/// Implementation of type inference machinery
impl BlockManager {
    pub fn infer_types(&mut self, symtab: &mut symtab::SymbolTable) {
        let (values, blocks) = (&mut self.values, &mut self.blocks);

        let ctx = TypeInferenceContext::new(symtab, values);
        let mut require_reanalysis: BTreeSet<usize> = blocks.keys().cloned().collect();

        while require_reanalysis.len() > 0 {
            require_reanalysis = require_reanalysis
                .into_iter()
                .map(
                    |label| match blocks.get_mut(&label).unwrap().infer_types(&ctx) {
                        true => None,
                        false => Some(label),
                    },
                )
                .flatten()
                .collect();
        }
    }
}

impl From<ControlFlow> for BlockManager {
    fn from(cf: ControlFlow) -> Self {
        let (blocks, values) = cf.take();
        Self::with_existing_blocks(blocks, values)
    }
}

#[cfg(test)]
mod tests {}

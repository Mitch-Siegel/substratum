use std::collections::BTreeSet;

use frontend::sourceloc;

use crate::{
    ir,
    ir::{
        BasicBlock, ControlFlow, IrLine, OperandTypeInference, TypeInferenceContext, ValueInterner,
    },
    symtab,
    symtab::ValueOwner,
    types,
};

use std::{collections::HashMap, fmt::Debug};

pub(crate) mod block_convergences;
mod branch;
mod branch_error;
mod convergence_error;

use block_convergences::BlockConvergences;
pub(crate) use block_convergences::ConvergenceResult;
pub(crate) use branch_error::BranchError;
pub(crate) use convergence_error::ConvergenceError;

#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) enum BranchKind {
    Unconditional,
    ConditionalTrue(Box<BasicBlock>), // currently on the true branch of a conditional. Owns the
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
    pub(crate) fn new(from_label: usize, kind: BranchKind) -> Self {
        Self { from_label, kind }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct BlockManager {
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
    pub(crate) fn new(
        unit_type: types::Semantic,
        parent_def_path: &symtab::ValuePath,
    ) -> (Self, usize) {
        // set up the initlal convergence - must always end up at the end_block
        let start_block_path = parent_def_path.clone().with_scope(symtab::ScopeId(0));
        let end_block_path = parent_def_path.clone().with_scope(symtab::ScopeId(1));
        let start_block = BasicBlock::new(0, start_block_path);
        let end_block = BasicBlock::new(1, end_block_path);
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

    pub(crate) fn take(mut self, before_final_block: usize) -> ControlFlow {
        match self.converge_with_jump(before_final_block, sourceloc::SourceLoc::none()) {
            Ok(ConvergenceResult::Done(block)) => {
                self.blocks.insert(block.label, block);
            }
            Ok(ConvergenceResult::NotDone(e)) => {
                panic!("open branches exist (convergence not done for {e})")
            }
            Err(e) => panic!("convergence error: {e:?}"),
        }

        assert!(
            self.open_branch_path.is_empty(),
            "open branch path length > 0"
        );

        assert!(self.convergences.is_empty(), "unresolved convergences");

        ControlFlow::new(self.blocks, self.values)
    }

    pub(crate) fn with_existing_blocks(
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

    pub(crate) fn values(&self) -> &ValueInterner {
        &self.values
    }

    pub(crate) fn values_mut(&mut self) -> &mut ValueInterner {
        &mut self.values
    }

    pub(crate) fn get(&self, label: usize) -> Option<&BasicBlock> {
        self.blocks.get(&label)
    }

    pub(crate) fn get_mut(&mut self, label: usize) -> Option<&mut BasicBlock> {
        self.blocks.get_mut(&label)
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
        loc: sourceloc::SourceLoc,
    ) -> Result<ConvergenceResult, ConvergenceError> {
        let result = self.convergences.converge(from)?;

        let converge_to = match &result {
            ConvergenceResult::Done(block) => block.label,
            ConvergenceResult::NotDone(label) => *label,
        };

        let convergence_jump =
            IrLine::new_jump(loc, converge_to, ir::lowered::JumpCondition::Unconditional);
        self.get_mut(from).unwrap().push(convergence_jump);

        Ok(result)
    }

    pub(crate) fn ensure_finished(&self) -> Result<(), BranchError> {
        match self.open_branch_path.last() {
            None => Ok(()),
            Some(unfinished) => Err(BranchError::NotDone(unfinished.from_label)),
        }
    }
}

/// implementation of manipulation functions such as splitting
impl BlockManager {
    pub(crate) fn finish_block_split(
        &mut self,
        split_end_label: usize,
        loc: sourceloc::SourceLoc,
    ) -> Result<usize, BranchError> {
        let after_split_label = self.finish_branch(split_end_label, loc).unwrap();

        let last_branch = self.pop_last_branch()?;
        match last_branch.kind {
            BranchKind::BlockSplit(mut after_split_stmts) => {
                let after_split_block = self.get_mut(after_split_label).unwrap();
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
    pub(crate) fn infer_types(&mut self, symtab: &mut symtab::SymbolTable) {
        let (values, blocks) = (&mut self.values, &mut self.blocks);

        let ctx = TypeInferenceContext::new(symtab, values);
        let mut require_reanalysis: BTreeSet<usize> = blocks.keys().copied().collect();

        while !require_reanalysis.is_empty() {
            require_reanalysis.retain(|label| blocks.get_mut(label).unwrap().infer_types(&ctx));
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

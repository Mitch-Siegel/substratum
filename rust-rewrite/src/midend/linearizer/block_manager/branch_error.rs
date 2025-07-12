use crate::midend::linearizer::block_manager::*;

#[derive(Debug, PartialEq, Eq)]
pub enum BranchError {
    NotBranched, // not branched but expected a branch
    Convergence(ConvergenceError),
    NotDone(usize),
    ExistingFalseBlock(usize, usize), // branch already exists (from_block, false_block)
    MissingFalseBlock(usize),         // missing false block on branch (from_label)
    ScopeHandling,
}

impl From<ConvergenceError> for BranchError {
    fn from(e: ConvergenceError) -> Self {
        Self::Convergence(e)
    }
}

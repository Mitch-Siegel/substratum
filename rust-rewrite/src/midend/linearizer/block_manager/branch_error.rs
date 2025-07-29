use crate::midend::linearizer::block_manager::*;

#[derive(Debug, PartialEq, Eq)]
pub enum BranchError {
    NotBranched, // not branched but expected a branch
    Convergence(ConvergenceError),
    ConvergenceNotDone(usize), // convergence returned NotDone when expected Done
    ConvergenceDone(ir::BasicBlock), // convergence returned Done when expected NotDone
    NotDone(usize),
    WrongKind(BranchKind), // current branch doesn't match the expected kind
    ExistingFalseBlock(usize, usize), // branch already exists (from_block, false_block)
    MissingFalseBlock(usize), // missing false block on branch (from_label)
    SwitchBlockMismatch(usize, usize), // (expected, found) where expected switch block label didn't match the one at the end of the open branch path
    ScopeHandling,
}

impl From<ConvergenceError> for BranchError {
    fn from(e: ConvergenceError) -> Self {
        Self::Convergence(e)
    }
}

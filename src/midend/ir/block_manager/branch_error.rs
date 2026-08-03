use crate::midend::ir::block_manager::{ir, BranchKind, ConvergenceError, Debug};

#[derive(Debug, PartialEq, Eq)]
pub(crate) enum BranchError {
    NotBranched, // not branched but expected a branch
    Convergence(ConvergenceError),
    ConvergenceNotDone(usize), // convergence returned NotDone when expected Done
    ConvergenceDone(Box<ir::BasicBlock>), // convergence returned Done when expected NotDone
    NotDone(usize),
    WrongKind(BranchKind, Vec<BranchKind>), // (current, expected) where current branch doesn't match the expected kind
    MissingFalseBlock(usize),               // missing false block on branch (from_label)
    SwitchBlockMismatch(usize, usize), // (expected, found) where expected switch block label didn't match the one at the end of the open branch path
    LoopInsideNotDone(usize),
    ScopeHandling,
}

impl From<ConvergenceError> for BranchError {
    fn from(e: ConvergenceError) -> Self {
        Self::Convergence(e)
    }
}

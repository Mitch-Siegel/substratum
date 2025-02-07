use core::fmt;
use std::cmp::Ordering;

use serde::Serialize;

#[derive(Copy, Clone, Debug, Serialize)]
pub struct ProgramPoint {
    pub depth: usize,
    pub index: usize,
}

impl PartialOrd for ProgramPoint {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(
            self.depth
                .cmp(&other.depth)
                .then(self.index.cmp(&other.index)),
        )
    }
}

impl PartialEq for ProgramPoint {
    fn eq(&self, other: &Self) -> bool {
        (self.depth == other.depth) && (self.index == other.index)
    }
}

impl Eq for ProgramPoint {}

impl Ord for ProgramPoint {
    fn cmp(&self, other: &Self) -> Ordering {
        self.depth
            .cmp(&other.depth)
            .then(self.index.cmp(&other.index))
    }
}

impl ProgramPoint {
    pub fn default() -> Self {
        Self::new(0, 0)
    }

    pub fn new(depth: usize, index: usize) -> Self {
        ProgramPoint { depth, index }
    }
}

impl std::fmt::Display for ProgramPoint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:>2x}:{:<2x}", self.depth, self.index)
    }
}

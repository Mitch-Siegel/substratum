use std::fmt::Display;

use serde::{Deserialize, Serialize};

#[derive(Copy, Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct SourceLoc {
    line: usize,
    col: usize,
}

impl SourceLoc {
    pub fn none() -> Self {
        SourceLoc { line: 0, col: 0 }
    }

    pub fn new(line: usize, col: usize) -> Self {
        SourceLoc { line, col }
    }
}

impl Display for SourceLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.line, self.col)
    }
}

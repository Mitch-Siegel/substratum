use std::fmt::Display;

use serde::{Deserialize, Serialize};

#[derive(Copy, Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
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

    pub fn as_string(&self) -> String {
        String::from(format!("{}:{}", self.line, self.col))
    }

    pub fn valid(&self) -> bool {
        self.line != 0 && self.col != 0
    }
}

impl Display for SourceLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.line, self.col)
    }
}

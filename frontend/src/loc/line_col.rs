use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(
    Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize,
)]
pub(crate) struct LineCol {
    pub line: u32,
    pub col: u32,
}

impl LineCol {
    pub(crate) fn new(line: u32, col: u32) -> Self {
        Self { line, col }
    }
}

impl fmt::Display for LineCol {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.line, self.col)
    }
}

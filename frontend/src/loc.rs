use std::fmt;

use serde::{Deserialize, Serialize};

mod line_col;
mod span;

pub(crate) use line_col::LineCol;
pub use span::{LineColSpan, Span};

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Loc {
    file: String,
    line_col: LineCol,
}

impl Loc {
    pub(crate) fn new(file: String, line_col: LineCol) -> Self {
        Self { file, line_col }
    }
}

impl From<&'static std::panic::Location<'static>> for Loc {
    fn from(location: &'static std::panic::Location<'static>) -> Self {
        Self {
            file: location.file().to_string(),
            line_col: LineCol::new(location.line(), location.column()),
        }
    }
}

impl fmt::Display for Loc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.file, self.line_col)
    }
}

impl std::fmt::Debug for Loc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self}")
    }
}

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub struct StaticLoc {
    file: &'static str,
    point: LineCol,
}

impl StaticLoc {
    #[must_use]
    pub fn new(file: &'static str, line: u32, col: u32) -> Self {
        Self {
            file,
            point: LineCol::new(line, col),
        }
    }
}

impl fmt::Debug for StaticLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.file, self.point)
    }
}

#[macro_export]
macro_rules! here {
    () => {
        $crate::loc::StaticLoc::new(file!(), line!(), column!())
    };
}

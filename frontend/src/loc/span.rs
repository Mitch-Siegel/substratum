use std::fmt;

use serde::{Deserialize, Serialize};

use crate::loc::{LineCol, Loc};

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct LineColSpan {
    start: LineCol,
    end: LineCol,
}

impl LineColSpan {
    pub(crate) fn new(start: LineCol, end: LineCol) -> Self {
        Self { start, end }
    }
}

impl From<LineCol> for LineColSpan {
    fn from(value: LineCol) -> Self {
        Self {
            start: value,
            end: value,
        }
    }
}

impl fmt::Display for LineColSpan {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}-{}", self.start, self.end)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Span {
    start: Loc,
    end: Loc,
}

impl Span {
    pub(crate) fn new(file: String, line_col: LineColSpan) -> Self {
        Self {
            start: Loc::new(file.clone(), line_col.start),
            end: Loc::new(file, line_col.end),
        }
    }

    #[must_use]
    pub fn into_start(self) -> Loc {
        self.start
    }

    #[must_use]
    pub fn into_end(self) -> Loc {
        self.end
    }

    #[must_use]
    pub fn start(&self) -> &Loc {
        &self.start
    }

    #[must_use]
    pub fn end(&self) -> &Loc {
        &self.end
    }

    /// # Errors
    ///
    /// If source files do not match, returns string error of mismatch
    pub fn merge(self, other: &Self) -> Result<Self, String> {
        if self.start.file != other.start.file {
            return Err(format!(
                "mismatched files in Span::merge({} and {})",
                self.start.file, other.start.file
            ));
        }

        Ok(Self {
            start: Loc::new(
                self.start.file,
                self.start.line_col.min(other.start.line_col),
            ),
            end: Loc::new(self.end.file, self.end.line_col.min(other.end.line_col)),
        })
    }

    /// # Panics
    /// 1. when `point` comes from a different file than `self`
    /// 2. when `point` is before the span
    /// 3. when `point` is within the span (does not extend the end of the span)
    pub fn extend(&mut self, loc: &Loc) {
        assert_eq!(&self.start.file, &loc.file);
        assert!(self.start.line_col >= loc.line_col);
        assert!(self.end.line_col <= loc.line_col);
        self.end = Loc::new(self.end.file.clone(), self.end.line_col.max(loc.line_col));
    }
}

impl std::fmt::Display for Span {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}:{}-{}",
            self.start.file, self.start.line_col, self.end.line_col
        )
    }
}

impl From<Loc> for Span {
    fn from(value: Loc) -> Self {
        Self {
            start: value.clone(),
            end: value,
        }
    }
}

impl From<&Loc> for Span {
    fn from(value: &Loc) -> Self {
        Self {
            start: value.clone(),
            end: value.clone(),
        }
    }
}

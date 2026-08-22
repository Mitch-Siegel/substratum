use std::fmt::Display;

use serde::{Deserialize, Serialize};

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub(crate) struct SourcePoint {
    pub line: u32,
    pub col: u32,
}

impl SourcePoint {
    pub(crate) fn new(line: u32, col: u32) -> Self {
        Self { line, col }
    }

    pub(crate) fn valid(self) -> bool {
        self.line != 0 && self.col != 0
    }
}

impl Display for SourcePoint {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.line, self.col)
    }
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SourceLoc {
    file: String,
    point: SourcePoint,
}

impl SourceLoc {
    // TODO: this is a smell. just use options elsewhere instead of constructing none/invalid points
    #[must_use]
    pub fn none() -> Self {
        Self {
            file: String::new(),
            point: SourcePoint::default(),
        }
    }

    pub(crate) fn new(file: String, point: SourcePoint) -> Self {
        Self { file, point }
    }

    // TODO: this is a smell. just use options elsewhere instead of constructing none/invalid points
    #[must_use]
    pub fn valid(&self) -> bool {
        !self.file.is_empty() && self.point.valid()
    }
}

impl From<&'static std::panic::Location<'static>> for SourceLoc {
    fn from(location: &'static std::panic::Location<'static>) -> Self {
        Self {
            file: location.file().to_string(),
            point: SourcePoint::new(location.line(), location.column()),
        }
    }
}

impl Display for SourceLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.file, self.point)
    }
}

impl std::fmt::Debug for SourceLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self}")
    }
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct SourceSpan {
    file: String,
    start: SourcePoint,
    end: SourcePoint,
}

impl SourceSpan {
    pub(crate) fn new(file: String, start: SourcePoint, end: SourcePoint) -> Self {
        Self { file, start, end }
    }

    #[must_use]
    pub fn start(self) -> SourceLoc {
        SourceLoc::new(self.file, self.start)
    }

    #[must_use]
    pub fn end(self) -> SourceLoc {
        SourceLoc::new(self.file, self.end)
    }

    /// # Errors
    ///
    /// If source files do not match, returns string error of mismatch
    pub fn merge(mut self, other: &Self) -> Result<Self, String> {
        if self.file != other.file {
            return Err(format!(
                "mismatched files in SourceSpan::merge({} and {})",
                self.file, other.file
            ));
        }
        self.start = SourcePoint::min(self.start, other.start);
        self.end = SourcePoint::max(self.end, other.end);
        Ok(self)
    }
}

impl std::fmt::Display for SourceSpan {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}:{}", self.file, self.start, self.end)
    }
}

impl From<SourceLoc> for SourceSpan {
    fn from(value: SourceLoc) -> Self {
        Self {
            file: value.file,
            start: value.point,
            end: value.point,
        }
    }
}

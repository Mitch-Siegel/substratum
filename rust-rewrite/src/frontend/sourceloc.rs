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
pub(crate) struct SourceLoc {
    pub file: String,
    pub point: SourcePoint,
}

impl SourceLoc {
    pub(crate) fn none() -> Self {
        SourceLoc {
            file: String::new(),
            point: SourcePoint::default(),
        }
    }

    pub(crate) fn new(file: String, point: SourcePoint) -> Self {
        SourceLoc { file, point }
    }

    pub(crate) fn valid(&self) -> bool {
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
pub(crate) struct SourceSpan {
    file: String,
    start: SourcePoint,
    end: SourcePoint,
}

impl SourceSpan {
    pub(crate) fn new(file: String, start: SourcePoint, end: SourcePoint) -> Self {
        Self { file, start, end }
    }

    pub(crate) fn start(self) -> SourceLoc {
        SourceLoc::new(self.file, self.start)
    }

    pub(crate) fn end(self) -> SourceLoc {
        SourceLoc::new(self.file, self.end)
    }

    pub(crate) fn merge(mut self, other: &Self) -> Result<Self, String> {
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

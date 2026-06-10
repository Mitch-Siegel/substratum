use std::fmt::Display;

use serde::{Deserialize, Serialize};

#[derive(Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub struct SourcePoint {
    pub line: u32,
    pub col: u32,
}

impl SourcePoint {
    pub fn new(line: u32, col: u32) -> Self {
        Self { line, col }
    }

    pub fn valid(&self) -> bool {
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
    pub file: String,
    pub point: SourcePoint,
}

impl SourceLoc {
    pub fn none() -> Self {
        SourceLoc {
            file: "".into(),
            point: SourcePoint::default(),
        }
    }

    pub fn new(file: String, point: SourcePoint) -> Self {
        SourceLoc { file, point }
    }

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
        write!(f, "{}", self)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct SourceSpan {
    file: String,
    start: SourcePoint,
    end: SourcePoint,
}

impl SourceSpan {
    pub fn new(file: String, start: SourcePoint, end: SourcePoint) -> Self {
        Self { file, start, end }
    }

    pub fn start(self) -> SourceLoc {
        SourceLoc::new(self.file, self.start)
    }

    pub fn end(self) -> SourceLoc {
        SourceLoc::new(self.file, self.end)
    }

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

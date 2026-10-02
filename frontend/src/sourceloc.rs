use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(
    Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize,
)]
pub struct SourcePoint {
    line: u32,
    col: u32,
}

impl SourcePoint {
    #[must_use]
    pub fn new(line: u32, col: u32) -> Self {
        Self { line, col }
    }
}

impl fmt::Display for SourcePoint {
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
    #[must_use]
    pub fn new(file: String, point: SourcePoint) -> Self {
        Self { file, point }
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

impl fmt::Display for SourceLoc {
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
    pub fn start(&self) -> SourceLoc {
        SourceLoc::new(self.file.clone(), self.start)
    }

    #[must_use]
    pub fn end(&self) -> SourceLoc {
        SourceLoc::new(self.file.clone(), self.end)
    }

    #[must_use]
    pub fn into_start(self) -> SourceLoc {
        SourceLoc::new(self.file, self.start)
    }

    #[must_use]
    pub fn into_end(self) -> SourceLoc {
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

    /// # Panics
    /// 1. when `point` comes from a different file than `self`
    /// 2. when `point` is before the span
    /// 3. when `point` is within the span (does not extend the end of the span)
    pub fn extend(&mut self, loc: &SourceLoc) {
        assert_eq!(&self.file, &loc.file);
        assert!(self.start >= loc.point);
        assert!(self.end <= loc.point);
        self.end = SourcePoint::max(self.end, loc.point);
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

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub struct StaticSourceLoc {
    file: &'static str,
    point: SourcePoint,
}

impl StaticSourceLoc {
    #[must_use]
    pub fn new(file: &'static str, line: u32, col: u32) -> Self {
        Self {
            file,
            point: SourcePoint::new(line, col),
        }
    }
}

impl fmt::Debug for StaticSourceLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.file, self.point)
    }
}

#[macro_export]
macro_rules! here {
    () => {
        $crate::sourceloc::StaticSourceLoc::new(file!(), line!(), column!())
    };
}

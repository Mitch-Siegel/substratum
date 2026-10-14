use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(
    Copy, Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize,
)]
pub(crate) struct Point {
    pub line: u32,
    pub col: u32,
}

impl Point {
    pub(crate) fn new(line: u32, col: u32) -> Self {
        Self { line, col }
    }
}

impl fmt::Display for Point {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.line, self.col)
    }
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Loc {
    file: String,
    point: Point,
}

impl Loc {
    // TODO: this is a smell. just use options elsewhere instead of constructing none/invalid points
    #[must_use]
    pub fn none() -> Self {
        Self {
            file: String::new(),
            point: Point::default(),
        }
    }

    pub(crate) fn new(file: String, point: Point) -> Self {
        Self { file, point }
    }
}

impl From<&'static std::panic::Location<'static>> for Loc {
    fn from(location: &'static std::panic::Location<'static>) -> Self {
        Self {
            file: location.file().to_string(),
            point: Point::new(location.line(), location.column()),
        }
    }
}

impl fmt::Display for Loc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}:{}", self.file, self.point)
    }
}

impl std::fmt::Debug for Loc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self}")
    }
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct Span {
    file: String,
    points: PointSpan,
}

impl Span {
    pub(crate) fn new(file: String, points: PointSpan) -> Self {
        Self { file, points }
    }

    #[must_use]
    pub fn start(self) -> Loc {
        Loc::new(self.file, self.points.start)
    }

    #[must_use]
    pub fn end(self) -> Loc {
        Loc::new(self.file, self.points.end)
    }

    /// # Errors
    ///
    /// If source files do not match, returns string error of mismatch
    pub fn merge(self, other: &Self) -> Result<Self, String> {
        if self.file != other.file {
            return Err(format!(
                "mismatched files in Span::merge({} and {})",
                self.file, other.file
            ));
        }

        self.points.merge(other.points);
        Ok(self)
    }

    /// # Panics
    /// 1. when `point` comes from a different file than `self`
    /// 2. when `point` is before the span
    /// 3. when `point` is within the span (does not extend the end of the span)
    pub fn extend(&mut self, loc: &Loc) {
        assert_eq!(&self.file, &loc.file);
        assert!(self.points.start >= loc.point);
        assert!(self.points.end <= loc.point);
        self.points.end = Point::max(self.points.end, loc.point);
    }
}

impl std::fmt::Display for Span {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}", self.file, self.points)
    }
}

impl From<Loc> for Span {
    fn from(value: Loc) -> Self {
        Self {
            file: value.file,
            points: value.point.into(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct PointSpan {
    start: Point,
    end: Point,
}

impl PointSpan {
    pub(crate) fn new(start: Point, end: Point) -> Self {
        Self { start, end }
    }

    pub(crate) fn merge(self, other: Self) -> Self {
        Self {
            start: self.start.min(other.start),
            end: other.end.max(other.end),
        }
    }
}

impl From<Point> for PointSpan {
    fn from(value: Point) -> Self {
        Self {
            start: value,
            end: value,
        }
    }
}

impl fmt::Display for PointSpan {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}-{}", self.start, self.end)
    }
}

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub struct StaticLoc {
    file: &'static str,
    point: Point,
}

impl StaticLoc {
    #[must_use]
    pub fn new(file: &'static str, line: u32, col: u32) -> Self {
        Self {
            file,
            point: Point::new(line, col),
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

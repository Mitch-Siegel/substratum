use std::fmt::Display;

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct SourceLoc {
    pub file: String,
    pub line: usize,
    pub col: usize,
}

impl SourceLoc {
    pub fn none() -> Self {
        SourceLoc {
            file: "".into(),
            line: 0,
            col: 0,
        }
    }

    pub fn new(path: &std::path::Path, line: usize, col: usize) -> Self {
        SourceLoc {
            file: path.to_str().unwrap().into(),
            line,
            col,
        }
    }

    pub fn with_new_position(mut self, line: usize, col: usize) -> Self {
        self.line = line;
        self.col = col;
        self
    }

    pub fn as_string(&self) -> String {
        String::from(format!("{} {}:{}", self.file, self.line, self.col))
    }

    pub fn valid(&self) -> bool {
        self.line != 0 && self.col != 0
    }
}

impl From<&'static std::panic::Location<'static>> for SourceLoc {
    fn from(location: &'static std::panic::Location<'static>) -> Self {
        Self {
            file: location.file().to_string(),
            line: location.line() as usize,
            col: location.column() as usize,
        }
    }
}

impl Display for SourceLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_string())
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct SourceLocWithMod {
    pub raw_loc: SourceLoc,
    pub module: String,
}

impl SourceLocWithMod {
    pub fn new(raw_loc: SourceLoc, module: String) -> Self {
        Self { raw_loc, module }
    }

    pub fn none() -> Self {
        Self {
            raw_loc: SourceLoc::none(),
            module: String::new(),
        }
    }

    pub fn valid(&self) -> bool {
        self.raw_loc.valid() && self.module.len() > 0
    }
}

impl Display for SourceLocWithMod {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "module {}: {}", self.module, self.raw_loc)
    }
}

use std::fmt::Display;

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct SourceLoc {
    pub path: String,
    pub module: String,
    pub line: usize,
    pub col: usize,
}

impl SourceLoc {
    pub fn none() -> Self {
        SourceLoc {
            path: "".into(),
            module: "".into(),
            line: 0,
            col: 0,
        }
    }

    pub fn new(path: &std::path::Path, line: usize, col: usize) -> Self {
        SourceLoc {
            path: path.to_str().unwrap().into(),
            module: path.file_name().unwrap().to_str().unwrap().into(),
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
        String::from(format!("{} - {}:{}", self.module, self.line, self.col))
    }

    pub fn valid(&self) -> bool {
        self.line != 0 && self.col != 0
    }

    pub fn module(&self) -> &str {
        &self.module
    }
}

impl Display for SourceLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_string())
    }
}

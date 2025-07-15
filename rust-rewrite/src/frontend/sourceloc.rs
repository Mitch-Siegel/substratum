use std::fmt::Display;

use serde::{Deserialize, Serialize};

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct SourceLoc {
    file: std::rc::Rc<std::path::PathBuf>,
    line: usize,
    col: usize,
}

impl SourceLoc {
    pub fn none() -> Self {
        SourceLoc {
            file: std::rc::Rc::new(std::path::Path::new("").into()),
            line: 0,
            col: 0,
        }
    }

    pub fn new(file: std::rc::Rc<std::path::PathBuf>, line: usize, col: usize) -> Self {
        SourceLoc { file, line, col }
    }

    pub fn as_string(&self) -> String {
        String::from(format!(
            "{} - {}:{}",
            self.file.display(),
            self.line,
            self.col
        ))
    }

    pub fn valid(&self) -> bool {
        self.line != 0 && self.col != 0
    }

    pub fn module(&self) -> String {
        let file_name = self.file.file_name().unwrap();
        file_name.to_str().unwrap().into()
    }
}

impl Display for SourceLoc {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_string())
    }
}

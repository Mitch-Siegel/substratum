use std::fmt::Display;

use serde::Serialize;

use super::ir::IrLine;

#[derive(Debug, Serialize)]
pub struct BasicBlock {
    label: usize,
    statements: Vec<IrLine>,
}

impl Display for BasicBlock {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "Basic Block {}", self.label)?;
        for statement in &self.statements {
            writeln!(f, "{}", statement)?;
        }
        std::fmt::Result::Ok(())
    }
}

impl BasicBlock {
    pub fn new(label: usize) -> Self {
        BasicBlock {
            label,
            statements: Vec::new(),
        }
    }

    pub fn append_statement(&mut self, mut statement: IrLine) {
        statement.program_point.index = self.statements.len();
        self.statements.push(statement);
    }

    pub fn label(&self) -> usize {
        self.label
    }

    pub fn statements(&self) -> &Vec<IrLine> {
        &self.statements
    }

    pub fn assign_depth(&mut self, depth: usize) {
        for statement in &mut self.statements {
            statement.program_point.depth = depth;
        }
    }
}

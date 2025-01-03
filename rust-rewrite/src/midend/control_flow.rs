use std::collections::{HashMap, HashSet};

use crate::lexer::SourceLoc;

use super::ir::*;

use serde::Serialize;

#[derive(Debug, Serialize)]
pub struct ControlFlow {
    blocks: Vec<BasicBlock>,
    current_block: usize,
    control_convergences: Vec<usize>,
    // mapping from source block to set of destination blocks
    branches: HashMap<usize, HashSet<usize>>,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
}

impl ControlFlow {
    pub fn new() -> Self {
        ControlFlow {
            blocks: vec![BasicBlock::new(0), BasicBlock::new(1)],
            current_block: 0,
            control_convergences: vec![1],
            branches: HashMap::new(),
            temp_num: 0,
        }
    }

    pub fn print_ir(&self) {
        for block in &self.blocks {
            println!("Block {}:", block.label());
            println!("{}", serde_json::to_string_pretty(&block).unwrap());
        }
    }

    pub fn next_temp(&mut self) -> String {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        temp_name
    }

    pub fn append_statement_to_current_block(&mut self, statement: IR) {
        self.blocks[self.current_block].append_statement(statement);
    }

    pub fn converge_control(&mut self) {
        let converge_to = self.control_convergences.pop().expect("Need at least 1 label to converge control flow to");

        let end_block_jump = IR::new_jump(
            SourceLoc::none(),
            converge_to,
            JumpCondition::Unconditional,
        );

        self.append_statement_to_current_block(end_block_jump);

        self.current_block = converge_to;
    }
}

use super::ir::*;
use crate::lexer::SourceLoc;
use serde::Serialize;
use std::{cmp::max, collections::HashMap, usize};

/*
    A ControlFlow represents the notion of ownership over BasicBlocks. At the end of linearization of a section of code,
    all relevant basic blocks will be owned by a single control flow. During linearization control flows can branch
    correspondingly with actual branches in the code, as they are linarized. Each branch gets its own control flow,
    which owns only the blocks relevant to the contents of branch. When the linearization of each branch is complete,
    its control flow is merged back to the one from which it was branched. The original brancher then takes ownership
    of all blocks of the branchee.
*/

#[derive(Debug, Serialize)]
pub struct ControlFlow {
    blocks: Vec<BasicBlock>,
    current_block: usize,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
}

impl ControlFlow {
    pub fn new() -> Self {
        ControlFlow {
            blocks: Vec::new(),
            current_block: 0,
            temp_num: 0,
        }
    }

    pub fn print_ir(&self) {
        // print!("CFG: digraph{{");
        // for block in &self.blocks {
        //     for target in block.targets() {
        //         print!("{}->{}; ", block.label(), target);
        //     }
        // }
        // println!("}}");

        for block in &self.blocks {
            println!("Block {}:", block.label());
            println!("{}", block);
        }
    }

    pub fn next_temp(&mut self) -> String {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        temp_name
    }

    pub fn set_current_block(&mut self, label: usize) {
        assert!(label < self.blocks.len());
        self.current_block = label;
    }

    pub fn current_block(&self) -> usize {
        self.current_block
    }

    pub fn next_block(&mut self) -> &mut BasicBlock {
        self.blocks.push(BasicBlock::new(self.blocks.len()));
        self.blocks.last_mut().unwrap()
    }

    pub fn append_statement_to_current_block(&mut self, statement: IR) {
        self.blocks[self.current_block].append_statement(statement);
    }

    pub fn append_statement_to_block(&mut self, statement: IR, block: usize) {
        self.blocks[block].append_statement(statement);
    }

    pub fn blocks(&self) -> &Vec<BasicBlock> {
        &self.blocks
    }
}

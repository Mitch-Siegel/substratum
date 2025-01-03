use std::{
    cmp::max,
    collections::{HashMap},
    usize,
};

use crate::lexer::SourceLoc;

use super::ir::*;

use serde::Serialize;

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
    blocks: HashMap<usize, BasicBlock>,
    next_block_label: usize,
    current_block: usize,
    control_convergences: Vec<usize>,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
    // whether or not this control flow has been used to generate another CF which branches from it
    // in the case that it is, linearization must be completed on that CF
    // and it must be merged back to this one before this linearization to this one can continue
    is_branched: bool,
}

impl ControlFlow {
    pub fn new_starter() -> Self {
        let mut blocks = HashMap::<usize, BasicBlock>::new();
        blocks.insert(0, BasicBlock::new(0));
        blocks.insert(1, BasicBlock::new(1));
        ControlFlow {
            blocks: blocks,
            next_block_label: 2,
            current_block: 0,
            control_convergences: vec![1],
            temp_num: 0,
            is_branched: false,
        }
    }

    pub fn branch(&mut self) -> Self {
        if self.is_branched {
            panic!("ControlFlow.branch() called while already branched");
        }

        // create the set for blocks owned by the branch
        let mut branch_blocks = HashMap::<usize, BasicBlock>::new();

        let branched_to_label = self.next_block_label;
        self.next_block_label += 1;
        branch_blocks.insert(self.next_block_label, BasicBlock::new(branched_to_label));

        self.is_branched = true;

        ControlFlow {
            blocks: branch_blocks,
            next_block_label: self.next_block_label,
            current_block: branched_to_label,
            control_convergences: vec![*self
                .control_convergences
                .last()
                .expect("Control flow branch expects control to converge to")],
            temp_num: self.temp_num,
            is_branched: false,
        }
    }

    // merge a control flow branched from this one back to this one
    // other flow must have control fully converged
    // takes ownership of the other control flow and moves all its blocks to self
    pub fn merge(mut self, other: Self) {
        if !self.is_branched {
            panic!("ControlFlow.merge() called while not branched");
        }

        if other.control_convergences.len() > 0 {
            panic!("ControlFlow.merge(other) called with other not converged");
        }

        for (label, block) in other.blocks {
            if self.blocks.insert(label, block).is_some() {
                panic!(
                    "Duplicate basic block {} found during control flow merge",
                    label
                );
            }
        }

        self.next_block_label = max(self.next_block_label, other.next_block_label);
        self.temp_num = max(self.temp_num, other.temp_num);

        self.is_branched = false;
    }

    pub fn print_ir(&self) {
        for (_, block) in &self.blocks {
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
        self.blocks
            .get_mut(&self.current_block)
            .expect("Control flow's current blocks is not valid")
            .append_statement(statement);
    }

    pub fn converge_control(&mut self) {
        let converge_to = self
            .control_convergences
            .pop()
            .expect("Need at least 1 label to converge control flow to");

        let end_block_jump =
            IR::new_jump(SourceLoc::none(), converge_to, JumpCondition::Unconditional);

        self.append_statement_to_current_block(end_block_jump);

        self.current_block = converge_to;
    }
}

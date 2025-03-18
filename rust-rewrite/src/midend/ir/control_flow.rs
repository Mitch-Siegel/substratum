use crate::frontend::sourceloc::SourceLoc;

use super::ir;
use serde::Serialize;
use std::{
    collections::{HashMap, HashSet, VecDeque},
    fmt::Debug,
    usize,
};

/*
    A ControlFlow represents the notion of ownership over basic blocks. At the end of linearization of a section of code,
    all relevant basic blocks will be owned by a single control flow. During linearization control flows can branch
    correspondingly with actual branches in the code, as they are linarized. Each branch gets its own control flow,
    which owns only the blocks relevant to the contents of branch. When the linearization of each branch is complete,
    its control flow is merged back to the one from which it was branched. The original brancher then takes ownership
    of all blocks of the branchee.
*/

#[derive(Debug, Serialize)]
pub struct ControlFlow {
    pub blocks: HashMap<usize, ir::BasicBlock>,

    current_block: usize,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
    max_block: usize,
}

impl ControlFlow {
    pub fn new() -> Self {
        let mut starter_blocks = HashMap::<usize, ir::BasicBlock>::new();
        starter_blocks.insert(0, ir::BasicBlock::new(0));
        starter_blocks.insert(1, ir::BasicBlock::new(1));
        ControlFlow {
            blocks: starter_blocks,
            current_block: 0,
            temp_num: 0,
            max_block: 1,
        }
    }

    pub fn next_temp(&mut self) -> String {
        let temp_name = String::from(".T") + &self.temp_num.to_string();
        self.temp_num += 1;
        temp_name
    }

    pub fn set_current_block(&mut self, label: usize) {
        self.current_block = label;
    }

    pub fn current_block(&self) -> usize {
        self.current_block
    }

    pub fn next_block(&mut self) -> usize {
        self.max_block += 1;
        self.max_block - 1
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) -> Option<usize> {
        let new_current_block = self.append_statement_to_block(statement, self.current_block);
        match new_current_block {
            Some(block) => self.set_current_block(block),
            None => {}
        }
        new_current_block
    }

    pub fn block_for_label(&self, label: &usize) -> &ir::BasicBlock {
        self.blocks.get(label).unwrap()
    }

    fn block_mut_for_label(&mut self, label: usize) -> &mut ir::BasicBlock {
        self.blocks
            .entry(label)
            .or_insert(ir::BasicBlock::new(label))
    }

    fn append_statement_to_block(&mut self, statement: ir::IrLine, block: usize) -> Option<usize> {
        let jump_not_taken_block = match &statement.operation {
            ir::Operations::Jump(operands) => match &operands.condition {
                ir::JumpCondition::Unconditional => None,
                _ => Some(self.next_block()),
            },
            _ => None,
        };

        self.append_statement_to_block_raw(statement, block);

        match &jump_not_taken_block {
            Some(label) => {
                let block_exit = ir::IrLine::new_jump(
                    SourceLoc::none(),
                    *label,
                    ir::JumpCondition::Unconditional,
                );
                self.append_statement_to_block_raw(block_exit, block);
            }
            None => {}
        }

        jump_not_taken_block
    }

    fn append_statement_to_block_raw(&mut self, statement: ir::IrLine, label: usize) {
        match &statement.operation {
            ir::Operations::Jump(operands) => {
                let target_block = operands.destination_block;

                self.block_mut_for_label(target_block)
                    .predecessors
                    .insert(label);
                self.block_mut_for_label(label)
                    .successors
                    .insert(target_block);

                match &operands.condition {
                    ir::JumpCondition::Unconditional => None,
                    _ => Some(self.next_block()),
                }
            }
            _ => None,
        };

        self.block_mut_for_label(label).statements.push(statement);
    }

    pub fn to_graphviz(&self) {
        print!("digraph {{fontname=\"consolas\"; node[shape=box; fontname=\"consolas\"; nojustify=true]; splines=ortho;");
        for block in self.blocks.values() {
            let mut block_arg_string = String::new();
            for arg in &block.arguments {
                block_arg_string += &format!("{} ", arg);
            }

            let mut block_string =
                String::from(format!("Block {}({})\n", block.label, block_arg_string));
            for statement in &block.statements {
                let stmt_str = &String::from(format!("{}\\l", statement)).replace("\"", "\\\"");
                block_string += stmt_str;
            }

            println!("{}[label=\"{}\\l\"]; ", block.label, block_string);

            for successor in &block.successors {
                print!("{}->{};", block.label, successor);
            }
        }

        println!("}}");
    }

    fn generate_postorder_stack(&mut self, control_flow: &ControlFlow) -> VecDeque<usize> {
        let mut postorder_stack = VecDeque::<usize>::new();
        postorder_stack.clear();
        let mut visited = HashSet::<usize>::new();

        let mut dfs_stack = VecDeque::<usize>::new();
        dfs_stack.push_back(0);

        // go until done
        while dfs_stack.len() > 0 {
            match dfs_stack.pop_back() {
                Some(label) => {
                    // only visit once
                    if !visited.contains(&label) {
                        visited.insert(label);

                        postorder_stack.push_back(label);

                        for successor in &control_flow.block_for_label(&label).successors {
                            dfs_stack.push_back(*successor);
                        }
                    }
                }
                None => {}
            }
        }
        postorder_stack
    }
}

struct ControlFlowPostorder {}

impl ControlFlowPostorder {}

// TODO: are the postorder and reverse postorder named opposite right now? Need to actually check this...
impl ControlFlow {
    pub fn map_over_blocks_mut_postorder<MetadataType>(
        &mut self,
        operation: fn(&mut ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        metadata: MetadataType,
    ) -> MetadataType
    where
        MetadataType: Debug,
    {
        let boxed_metadata = Box::from(metadata);
        *ControlFlowPostorder::map_mut(self, operation, boxed_metadata)
    }

    pub fn map_over_blocks_postorder<MetadataType>(
        &self,
        operation: fn(&ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        metadata: MetadataType,
    ) -> MetadataType
    where
        MetadataType: Debug,
    {
        let boxed_metadata = Box::from(metadata);
        *ControlFlowPostorder::map(self, operation, boxed_metadata)
    }

    pub fn map_over_blocks_mut_reverse_postorder<MetadataType>(
        &mut self,
        operation: fn(&mut ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        metadata: MetadataType,
    ) -> MetadataType
    where
        MetadataType: Debug,
    {
        let boxed_metadata = Box::from(metadata);
        *ControlFlowPostorder::map_reverse_mut(self, operation, boxed_metadata)
    }

    pub fn map_over_blocks_reverse_postorder<MetadataType>(
        &self,
        operation: fn(&ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        metadata: MetadataType,
    ) -> MetadataType
    where
        MetadataType: Debug,
    {
        let boxed_metadata = Box::from(metadata);
        *ControlFlowPostorder::map_reverse(self, operation, boxed_metadata)
    }
}

use crate::frontend::sourceloc::SourceLoc;

use super::ir;
use serde::Serialize;
use std::{
    collections::{HashSet, VecDeque},
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
    pub blocks: Vec<ir::BasicBlock>,
    pub successors: Vec<HashSet<usize>>,
    pub predecessors: Vec<HashSet<usize>>,
    current_block: usize,
    // index of number of temporary variables used in this control flow (across all blocks)
    temp_num: usize,
}

impl ControlFlow {
    pub fn new() -> Self {
        ControlFlow {
            blocks: Vec::new(),
            successors: Vec::<HashSet<usize>>::new(),
            predecessors: Vec::<HashSet<usize>>::new(),
            current_block: 0,
            temp_num: 0,
        }
    }

    pub fn labels(&self) -> std::ops::Range<usize> {
        0..self.blocks.len()
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

    pub fn next_block(&mut self) -> usize {
        self.blocks.push(ir::BasicBlock::new(self.blocks.len()));
        self.successors.push(HashSet::new());
        self.predecessors.push(HashSet::new());
        self.blocks.len() - 1
    }

    pub fn append_statement_to_current_block(&mut self, statement: ir::IrLine) -> Option<usize> {
        let new_current_block = self.append_statement_to_block(statement, self.current_block);
        match new_current_block {
            Some(block) => self.set_current_block(block),
            None => {}
        }
        new_current_block
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

    fn append_statement_to_block_raw(&mut self, statement: ir::IrLine, block: usize) {
        match &statement.operation {
            ir::Operations::Jump(operands) => {
                let target_block = operands.destination_block;

                self.predecessors[target_block].insert(self.current_block);
                self.successors[self.current_block].insert(target_block);

                match &operands.condition {
                    ir::JumpCondition::Unconditional => None,
                    _ => Some(self.next_block()),
                }
            }
            _ => None,
        };

        self.blocks[block].append_statement(statement);
    }

    pub fn to_graphviz(&self) {
        print!("digraph {{fontname=\"consolas\"; node[shape=box; fontname=\"consolas\"; nojustify=true]; splines=ortho;");
        for block in &self.blocks {
            let mut block_arg_string = String::new();
            for arg in &block.arguments {
                block_arg_string += &format!("{} ", arg);
            }

            let mut block_string =
                String::from(format!("Block {}({})\n", block.label(), block_arg_string));
            for statement in block.statements() {
                let stmt_str = &String::from(format!("{}\\l", statement)).replace("\"", "\\\"");
                block_string += stmt_str;
            }

            println!("{}[label=\"{}\\l\"]; ", block.label(), block_string);
        }

        for label in 0..self.blocks.len() {
            for successor in self.successors.get(label).unwrap() {
                print!("{}->{}; ", label, successor)
            }
        }

        println!("}}");
    }
}

struct ControlFlowPostorder {
    pub seen: HashSet<usize>,
    pub dfs_stack: VecDeque<usize>,
    pub postorder_stack: VecDeque<usize>,
}

impl ControlFlowPostorder {
    pub fn map<MetadataType>(
        control_flow: &ControlFlow,
        operation: fn(&ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        metadata: Box<MetadataType>,
    ) -> Box<MetadataType> {
        let mut postorder = Self::new();
        postorder.visit_all(control_flow, operation, metadata)
    }

    pub fn map_mut<MetadataType>(
        control_flow: &mut ControlFlow,
        operation: fn(&mut ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        metadata: Box<MetadataType>,
    ) -> Box<MetadataType> {
        let mut postorder = Self::new();
        postorder.visit_all_mut(control_flow, operation, metadata)
    }

    pub fn map_reverse<MetadataType>(
        control_flow: &ControlFlow,
        operation: fn(&ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        metadata: Box<MetadataType>,
    ) -> Box<MetadataType> {
        let mut postorder = Self::new();
        postorder.visit_all_reverse(control_flow, operation, metadata)
    }

    pub fn map_reverse_mut<MetadataType>(
        control_flow: &mut ControlFlow,
        operation: fn(&mut ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        metadata: Box<MetadataType>,
    ) -> Box<MetadataType> {
        let mut postorder = Self::new();
        postorder.visit_all_reverse_mut(control_flow, operation, metadata)
    }

    fn new() -> Self {
        Self {
            seen: HashSet::<usize>::new(),
            dfs_stack: VecDeque::<usize>::new(),
            postorder_stack: VecDeque::<usize>::new(),
        }
    }

    fn visit_all<MetadataType>(
        &mut self,
        control_flow: &ControlFlow,
        on_visit: fn(&ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        mut metadata: Box<MetadataType>,
    ) -> Box<MetadataType> {
        self.generate_postorder_stack(control_flow);

        loop {
            match self.postorder_stack.pop_front() {
                Some(block_label) => {
                    metadata = on_visit(&control_flow.blocks[block_label], metadata)
                }
                None => {
                    break;
                }
            }
        }

        metadata
    }

    fn visit_all_mut<MetadataType>(
        &mut self,
        control_flow: &mut ControlFlow,
        on_visit: fn(&mut ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        mut metadata: Box<MetadataType>,
    ) -> Box<MetadataType> {
        self.generate_postorder_stack(control_flow);

        loop {
            match self.postorder_stack.pop_front() {
                Some(block_label) => {
                    metadata = on_visit(&mut control_flow.blocks[block_label], metadata)
                }
                None => {
                    break;
                }
            }
        }

        metadata
    }

    fn visit_all_reverse<MetadataType>(
        &mut self,
        control_flow: &ControlFlow,
        on_visit: fn(&ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        mut metadata: Box<MetadataType>,
    ) -> Box<MetadataType> {
        self.generate_postorder_stack(control_flow);

        loop {
            match self.postorder_stack.pop_back() {
                Some(block_label) => {
                    metadata = on_visit(&control_flow.blocks[block_label], metadata)
                }
                None => {
                    break;
                }
            }
        }

        metadata
    }

    fn visit_all_reverse_mut<MetadataType>(
        &mut self,
        control_flow: &mut ControlFlow,
        on_visit: fn(&mut ir::BasicBlock, Box<MetadataType>) -> Box<MetadataType>,
        mut metadata: Box<MetadataType>,
    ) -> Box<MetadataType> {
        self.generate_postorder_stack(control_flow);

        loop {
            match self.postorder_stack.pop_back() {
                Some(block_label) => {
                    metadata = on_visit(&mut control_flow.blocks[block_label], metadata)
                }
                None => {
                    break;
                }
            }
        }

        metadata
    }

    fn generate_postorder_stack(&mut self, control_flow: &ControlFlow) {
        self.postorder_stack.clear();
        self.dfs_stack.clear();

        self.dfs_stack.push_back(0);

        // go until done
        while self.dfs_stack.len() > 0 {
            match &self.dfs_stack.pop_back() {
                Some(label) => {
                    // only visit once
                    if !self.was_seen(label) {
                        self.mark_seen(label);

                        self.postorder_stack.push_back(*label);
                        self.push_all_successors(*label, control_flow);
                    }
                }
                None => {}
            }
        }
    }

    fn was_seen(&self, block_label: &usize) -> bool {
        self.seen.contains(block_label)
    }

    fn push_all_successors(&mut self, block_label: usize, control_flow: &ControlFlow) {
        for s in &control_flow.successors[block_label] {
            self.dfs_stack.push_back(*s);
        }
    }

    fn mark_seen(&mut self, block_label: &usize) {
        self.seen.insert(*block_label);
    }
}

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

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
    pub max_block: usize,
}

impl ControlFlow {
    pub fn new() -> Self {
        let mut starter_blocks = HashMap::<usize, ir::BasicBlock>::new();
        starter_blocks.insert(0, ir::BasicBlock::new(0));
        starter_blocks.insert(1, ir::BasicBlock::new(1));
        ControlFlow {
            blocks: starter_blocks,
            max_block: 1,
        }
    }

    pub fn block_for_label(&self, label: &usize) -> &ir::BasicBlock {
        self.blocks.get(label).unwrap()
    }

    fn block_mut_for_label(&mut self, label: usize) -> &mut ir::BasicBlock {
        self.blocks
            .entry(label)
            .or_insert(ir::BasicBlock::new(label))
    }

    // appends the given statement to the block with the label provided
    // retrurns an option to a reference to the field containing the destination label of the false jump
    // iff the statement was a conditional jump which forced the end of the block
    pub fn append_statement_to_block(
        &mut self,
        statement: ir::IrLine,
        label: usize,
    ) -> Option<&mut usize> {
        let jump_always = match &statement.operation {
            ir::Operations::Jump(operands) => match &operands.condition {
                ir::JumpCondition::Unconditional => false,
                _ => true,
            },
            _ => false,
        };

        self.append_statement_to_block_raw(statement, label);

        if !jump_always {
            let block_exit =
                ir::IrLine::new_jump(SourceLoc::none(), label, ir::JumpCondition::Unconditional);
            self.append_statement_to_block_raw(block_exit, label);

            match self.blocks.get_mut(&label).unwrap().statements.last_mut() {
                Some(statement) => match &mut statement.operation {
                    ir::Operations::Jump(jump_operation) => {
                        Some(&mut jump_operation.destination_block)
                    }
                    _ => panic!("Block exit jump not present"),
                },
                _ => panic!("Block exit jump not present"),
            }
        } else {
            None
        }
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
            }
            _ => {}
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

    pub fn replace_blocks<T: IntoIterator<Item = ir::BasicBlock>>(&mut self, iter: T) {
        let mut into = iter.into_iter();

        while let Some(block) = into.next() {
            self.max_block = usize::max(self.max_block, block.label);
            self.blocks.insert(block.label, block);
        }
    }
}

struct ControlFlowPostorder {
    postorder_stack: VecDeque<ir::BasicBlock>,
}

impl Iterator for ControlFlowPostorder {
    type Item = ir::BasicBlock;

    fn next(&mut self) -> Option<Self::Item> {
        self.postorder_stack.pop_front()
    }
}

struct ControlFlowIter<'a> {
    postorder_stack: VecDeque<&'a ir::BasicBlock>,
}

impl<'a> Iterator for ControlFlowIter<'a> {
    type Item = &'a ir::BasicBlock;

    fn next(&mut self) -> Option<Self::Item> {
        self.postorder_stack.pop_back()
    }
}

pub struct ControlFlowIntoIter<T> {
    postorder_stack: VecDeque<T>,
}

impl<T> Iterator for ControlFlowIntoIter<T> {
    type Item = T;

    fn next(&mut self) -> Option<Self::Item> {
        self.postorder_stack.pop_back()
    }
}

// TODO: are the postorder and reverse postorder named opposite right now? Need to actually check this...
impl ControlFlow {
    fn generate_postorder_stack(&self) -> VecDeque<usize> {
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

                        for successor in &self.block_for_label(&label).successors {
                            dfs_stack.push_back(*successor);
                        }
                    }
                }
                None => {}
            }
        }
        postorder_stack
    }

    pub fn iter(&self) -> ControlFlowIter<'_> {
        let postorder_stack = self.generate_postorder_stack();
        let mut postorder_blocks = VecDeque::new();
        for label in postorder_stack {
            postorder_blocks.push_back(&self.blocks[&label]);
        }
        ControlFlowIter {
            postorder_stack: postorder_blocks,
        }
    }
}

impl<'a> IntoIterator for &'a ControlFlow {
    type Item = &'a ir::BasicBlock;

    type IntoIter = ControlFlowIntoIter<&'a ir::BasicBlock>;

    fn into_iter(self) -> Self::IntoIter {
        let postorder_stack = self.generate_postorder_stack();
        let mut postorder_blocks = VecDeque::new();
        for label in postorder_stack {
            postorder_blocks.push_back(self.blocks.get(&label).unwrap());
        }
        ControlFlowIntoIter::<&'a ir::BasicBlock> {
            postorder_stack: postorder_blocks,
        }
    }
}

impl IntoIterator for ControlFlow {
    type Item = ir::BasicBlock;

    type IntoIter = ControlFlowIntoIter<ir::BasicBlock>;

    fn into_iter(self) -> Self::IntoIter {
        let postorder_stack = self.generate_postorder_stack();
        let mut postorder_blocks = VecDeque::new();
        for label in postorder_stack {
            postorder_blocks.push_back(self.blocks.get(&label).unwrap().to_owned());
        }
        ControlFlowIntoIter {
            postorder_stack: postorder_blocks,
        }
    }
}

impl FromIterator<ir::BasicBlock> for ControlFlow {
    fn from_iter<T: IntoIterator<Item = ir::BasicBlock>>(iter: T) -> Self {
        let mut max_block = usize::MIN;
        let mut blocks = HashMap::new();

        let mut into = iter.into_iter();

        while let Some(block) = into.next() {
            max_block = usize::max(max_block, block.label);
            blocks.insert(block.label, block);
        }

        ControlFlow { max_block, blocks }
    }
}

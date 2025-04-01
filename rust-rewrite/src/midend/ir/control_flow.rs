use crate::{
    frontend::sourceloc::SourceLoc,
    hashmap_ooo_iter::{HashMapOOOIter, HashMapOOOIterMut},
};

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

    pub fn next_block(&mut self) -> usize {
        self.max_block += 1;
        self.max_block
    }

    // appends the given statement to the block with the label provided
    // returns: (Option<usize>, Option<usize>) referring to (if the statement is a branch):
    // block targeted by branch
    // block control flow ends up in if the branch is not taken (conditional branches only)
    // retrurns an option to a reference to the field containing the destination label of the false jump
    // iff the statement was a conditional jump which forced the end of the block
    pub fn append_statement_to_block(
        &mut self,
        statement: ir::IrLine,
        label: usize,
    ) -> (Option<usize>, Option<usize>) {
        self.append_statement_to_block_raw(statement.clone(), label);

        match &statement.operation {
            ir::Operations::Jump(jump) => {
                let target = jump.destination_block;
                match &jump.condition {
                    ir::JumpCondition::Unconditional => (Some(target), None),
                    _ => {
                        let false_label = self.next_block();
                        let block_exit = ir::IrLine::new_jump(
                            SourceLoc::none(),
                            false_label,
                            ir::JumpCondition::Unconditional,
                        );
                        self.append_statement_to_block_raw(block_exit, label);
                        (Some(target), Some(false_label))
                    }
                }
            }
            _ => (None, None),
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
    fn generate_postorder_stack(&self) -> Vec<usize> {
        let mut postorder_stack = Vec::<usize>::new();
        postorder_stack.clear();
        let mut visited = HashSet::<usize>::new();

        let mut dfs_stack = Vec::<usize>::new();
        dfs_stack.push(0);

        // go until done
        while dfs_stack.len() > 0 {
            match dfs_stack.pop() {
                Some(label) => {
                    // only visit once
                    if !visited.contains(&label) {
                        visited.insert(label);

                        postorder_stack.push(label);

                        for successor in &self.block_for_label(&label).successors {
                            dfs_stack.push(*successor);
                        }
                    }
                }
                None => {}
            }
        }
        postorder_stack
    }

    pub fn blocks_postorder(&self) -> HashMapOOOIter<usize, ir::BasicBlock> {
        let postorder_stack = self.generate_postorder_stack();

        HashMapOOOIter::new(&self.blocks, postorder_stack)
    }

    pub fn blocks_postorder_mut(&mut self) -> HashMapOOOIterMut<usize, ir::BasicBlock> {
        let postorder_stack = self.generate_postorder_stack();

        HashMapOOOIterMut::new(&mut self.blocks, postorder_stack)
    }
}

mod tests {
    use crate::midend::ir::*;

    #[test]
    fn starter_blocks() {
        let cf = ControlFlow::new();

        assert!(cf.blocks.len() == 2);
        assert!(cf.max_block == 1);
    }

    #[test]
    fn get_block_for_label() {
        let mut cf = ControlFlow::new();

        assert_eq!(cf.block_for_label(&0).label, 0);

        assert_eq!(cf.block_mut_for_label(0).label, 0);
        assert_eq!(cf.block_mut_for_label(999).label, 999);
    }

    #[test]
    fn append_statement_to_block() {
        let mut cf = ControlFlow::new();

        let assignment = IrLine::new_assignment(
            SourceLoc::none(),
            Operand::new_as_variable("dest".into()),
            Operand::new_as_variable("source".into()),
        );
        assert_eq!(cf.append_statement_to_block(assignment, 0), (None, None));
    }

    #[test]
    fn append_unconditional_jump_to_block() {
        let mut cf = ControlFlow::new();
        let jump = IrLine::new_jump(SourceLoc::none(), 1, JumpCondition::Unconditional);
        assert_eq!(cf.append_statement_to_block(jump, 0), (Some(1), None));
    }

    #[test]
    fn append_conditional_jump_to_block() {
        let mut cf = ControlFlow::new();
        let jump = IrLine::new_jump(
            SourceLoc::none(),
            1,
            JumpCondition::Eq(ir::operands::DualSourceOperands {
                a: Operand::new_as_variable("eq_a".into()),
                b: Operand::new_as_variable("eq_b".into()),
            }),
        );
        assert_eq!(cf.append_statement_to_block(jump, 0), (Some(1), Some(2)));

        let second_jump = IrLine::new_jump(
            SourceLoc::none(),
            1,
            JumpCondition::Eq(ir::operands::DualSourceOperands {
                a: Operand::new_as_variable("eq_a2".into()),
                b: Operand::new_as_variable("eq_b2".into()),
            }),
        );
        assert_eq!(
            cf.append_statement_to_block(second_jump, 0),
            (Some(1), Some(3))
        );
    }
}

use crate::{
    map_ooo_iter::*,
    midend::{ir::*, symtab},
};
use std::collections::{BTreeMap, BTreeSet, VecDeque};

#[derive(Debug, Clone, Serialize)]
pub struct ControlFlow {
    blocks: BTreeMap<usize, BasicBlock>,
    successors: BTreeMap<usize, BTreeSet<usize>>,
    predecessors: BTreeMap<usize, BTreeSet<usize>>,
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
    pub fn successors(&self, label: &usize) -> Option<&BTreeSet<usize>> {
        self.successors.get(label)
    }

    pub fn predecessors(&self, label: &usize) -> Option<&BTreeSet<usize>> {
        self.predecessors.get(label)
    }

    fn generate_reverse_postorder_stack(&self) -> Vec<usize> {
        let mut postorder_stack = Vec::<usize>::new();
        postorder_stack.clear();
        let mut visited = BTreeSet::<usize>::new();

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

                        for successor in self.successors(&label).unwrap() {
                            dfs_stack.push(*successor);
                        }
                    }
                }
                None => {}
            }
        }
        postorder_stack
    }

    pub fn blocks_postorder(&self) -> BTreeMapOOOIter<usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        BTreeMapOOOIter::new(&self.blocks, rpo_stack.into_iter().rev())
    }

    pub fn blocks_postorder_mut(&mut self) -> BTreeMapOOOIterMut<usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        BTreeMapOOOIterMut::new(&mut self.blocks, rpo_stack.into_iter().rev())
    }

    pub fn blocks_reverse_postorder(&self) -> BTreeMapOOOIter<usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        BTreeMapOOOIter::new(&self.blocks, rpo_stack.into_iter())
    }

    pub fn blocks_reverse_postorder_mut(&mut self) -> BTreeMapOOOIterMut<usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        BTreeMapOOOIterMut::new(&mut self.blocks, rpo_stack.into_iter())
    }
}

impl symtab::BasicBlockOwner for ControlFlow {
    fn basic_blocks(&self) -> impl Iterator<Item = &ir::BasicBlock> {
        self.blocks.values()
    }

    fn lookup_basic_block(&self, label: usize) -> Option<&ir::BasicBlock> {
        self.blocks.get(&label)
    }
}

impl From<BTreeMap<usize, BasicBlock>> for ControlFlow {
    fn from(blocks: BTreeMap<usize, BasicBlock>) -> Self {
        let mut successors = BTreeMap::<usize, BTreeSet<usize>>::new();
        let mut predecessors = BTreeMap::<usize, BTreeSet<usize>>::new();

        for label in blocks.keys() {
            predecessors.entry(*label).or_default();
            successors.entry(*label).or_default();
        }

        for from_block in blocks.values() {
            for statement in from_block {
                match &statement.operation {
                    ir::Operations::Jump(jump) => {
                        successors
                            .get_mut(&from_block.label)
                            .unwrap()
                            .insert(jump.destination_block);

                        predecessors
                            .get_mut(&jump.destination_block)
                            .unwrap()
                            .insert(from_block.label);

                        if !blocks.contains_key(&jump.destination_block) {
                            panic!(
                                "Invalid jump target to nonexistent block {}",
                                jump.destination_block
                            );
                        }
                    }
                    _ => (),
                }
            }
        }

        Self {
            blocks,
            successors,
            predecessors,
        }
    }
}

impl<'a> IntoIterator for &'a ControlFlow {
    type Item = &'a BasicBlock;
    type IntoIter = std::collections::btree_map::Values<'a, usize, BasicBlock>;
    fn into_iter(self) -> Self::IntoIter {
        self.blocks.values()
    }
}

impl<'a> IntoIterator for &'a mut ControlFlow {
    type Item = &'a mut BasicBlock;
    type IntoIter = std::collections::btree_map::ValuesMut<'a, usize, BasicBlock>;
    fn into_iter(self) -> Self::IntoIter {
        self.blocks.values_mut()
    }
}

#[cfg(test)]
mod tests {
    use crate::midend::ir::*;
    use std::collections::{BTreeMap, BTreeSet};

    fn test_control_flow() -> ControlFlow {
        let mut b0 = ir::BasicBlock::new(0);
        let mut b1 = ir::BasicBlock::new(1);
        let mut b2 = ir::BasicBlock::new(2);
        let b3 = ir::BasicBlock::new(3);

        // 0->1
        let jump = ir::IrLine::new_jump(SourceLoc::none(), 1, JumpCondition::Unconditional);
        b0.statements.push(jump);

        // 1->2 (conditional)
        let jump = ir::IrLine::new_jump(
            SourceLoc::none(),
            2,
            JumpCondition::Eq(DualSourceOperands::new(
                ir::Operand::new_as_temporary("a".into()),
                ir::Operand::new_as_temporary("b".into()),
            )),
        );
        b1.statements.push(jump);
        // 1->3
        let jump = ir::IrLine::new_jump(SourceLoc::none(), 3, JumpCondition::Unconditional);
        b1.statements.push(jump);

        // 2->1
        let jump = ir::IrLine::new_jump(SourceLoc::none(), 1, JumpCondition::Unconditional);
        b2.statements.push(jump);

        let blocks = vec![b0, b1, b2, b3];

        ControlFlow::from(
            blocks
                .into_iter()
                .map(|block| (block.label, block))
                .collect::<BTreeMap<_, _>>(),
        )
    }

    #[test]
    fn successors_predecessors() {
        let cf = test_control_flow();

        assert_eq!(cf.predecessors(&0), Some(&BTreeSet::<usize>::new()));
        assert_eq!(
            cf.predecessors(&1),
            Some(&([0, 2].into_iter().collect::<BTreeSet::<usize>>()))
        );
        assert_eq!(
            cf.predecessors(&2),
            Some(&([1].into_iter().collect::<BTreeSet::<usize>>()))
        );
        assert_eq!(
            cf.predecessors(&3),
            Some(&([1].into_iter().collect::<BTreeSet::<usize>>()))
        );
        assert_eq!(cf.predecessors(&4), None);

        assert_eq!(
            cf.successors(&0),
            Some(&([1].into_iter().collect::<BTreeSet::<usize>>()))
        );
        assert_eq!(
            cf.successors(&1),
            Some(&([2, 3].into_iter().collect::<BTreeSet::<usize>>()))
        );
        assert_eq!(
            cf.successors(&2),
            Some(&([1].into_iter().collect::<BTreeSet::<usize>>()))
        );
        assert_eq!(cf.successors(&3), Some(&BTreeSet::<usize>::new()));
        assert_eq!(cf.successors(&4), None);
    }

    #[test]
    fn postorder() {
        let mut cf = test_control_flow();

        let expected_order: Vec<usize> = vec![2, 3, 1, 0];
        assert_eq!(
            cf.blocks_postorder()
                .map(|(label, _block)| { label })
                .collect::<Vec::<_>>(),
            expected_order
        );

        assert_eq!(
            cf.blocks_postorder_mut()
                .map(|(label, _block)| { label })
                .collect::<Vec::<_>>(),
            expected_order
        );

        let reverse_order: Vec<usize> = vec![0, 1, 3, 2];
        assert_eq!(
            cf.blocks_reverse_postorder()
                .map(|(label, _block)| { label })
                .collect::<Vec::<_>>(),
            reverse_order
        );

        assert_eq!(
            cf.blocks_reverse_postorder_mut()
                .map(|(label, _block)| { label })
                .collect::<Vec::<_>>(),
            reverse_order
        );
    }
}

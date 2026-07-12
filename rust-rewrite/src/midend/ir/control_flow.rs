use crate::{map_ooo_iter::*, midend::ir::*};
use std::collections::{BTreeSet, HashMap, VecDeque};

#[derive(Debug, Clone)]
pub(crate) struct ControlFlow {
    blocks: HashMap<usize, BasicBlock>,
    successors: HashMap<usize, BTreeSet<usize>>,
    predecessors: HashMap<usize, BTreeSet<usize>>,
    values: ValueInterner,
}

#[allow(unused)]
pub(crate) struct ControlFlowIntoIter<T> {
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
    pub(crate) fn new(blocks: HashMap<usize, BasicBlock>, values: ValueInterner) -> Self {
        let mut successors = HashMap::<usize, BTreeSet<usize>>::new();
        let mut predecessors = HashMap::<usize, BTreeSet<usize>>::new();

        for label in blocks.keys() {
            predecessors.entry(*label).or_default();
            successors.entry(*label).or_default();
        }

        for from_block in blocks.values() {
            for statement in from_block {
                match &statement.operation {
                    Operation::Lowered(lowered::Operation::Jump(jump)) => {
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
                    Operation::Unlowered(unlowered::Operation::Match(m)) => {
                        for arm in &m.arms {
                            successors
                                .get_mut(&from_block.label)
                                .unwrap()
                                .insert(arm.arm_label);
                            predecessors
                                .get_mut(&arm.arm_label)
                                .unwrap()
                                .insert(from_block.label);
                        }
                    }
                    Operation::Unlowered(_) => (),
                    _ => (),
                }
            }
        }

        Self {
            blocks,
            successors,
            predecessors,
            values,
        }
    }

    pub(crate) fn take(self) -> (HashMap<usize, BasicBlock>, ValueInterner) {
        (self.blocks, self.values)
    }

    pub(crate) fn successors(&self, label: &usize) -> Option<&BTreeSet<usize>> {
        self.successors.get(label)
    }

    pub(crate) fn predecessors(&self, label: &usize) -> Option<&BTreeSet<usize>> {
        self.predecessors.get(label)
    }

    pub(crate) fn blocks(&self) -> impl Iterator<Item = (&usize, &BasicBlock)> {
        self.blocks.iter()
    }

    fn generate_reverse_postorder_stack(&self) -> Vec<usize> {
        let mut postorder_stack = Vec::<usize>::new();
        postorder_stack.clear();
        let mut visited = BTreeSet::<usize>::new();

        let mut dfs_stack = Vec::<usize>::new();
        dfs_stack.push(0);

        while let Some(label) = dfs_stack.pop() {
            // only visit once
            if !visited.contains(&label) {
                visited.insert(label);

                postorder_stack.push(label);

                for successor in self.successors(&label).unwrap() {
                    dfs_stack.push(*successor);
                }
            }
        }
        postorder_stack
    }

    pub(crate) fn blocks_postorder(&self) -> HashMapOOOIter<'_, usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        HashMapOOOIter::new(&self.blocks, rpo_stack.into_iter().rev())
    }

    pub(crate) fn blocks_postorder_mut(&mut self) -> HashMapOOOIterMut<'_, usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        HashMapOOOIterMut::new(&mut self.blocks, rpo_stack.into_iter().rev())
    }

    pub(crate) fn blocks_reverse_postorder(&self) -> HashMapOOOIter<'_, usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        HashMapOOOIter::new(&self.blocks, rpo_stack.into_iter())
    }

    pub(crate) fn blocks_reverse_postorder_mut(
        &mut self,
    ) -> HashMapOOOIterMut<'_, usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        HashMapOOOIterMut::new(&mut self.blocks, rpo_stack.into_iter())
    }

    pub(crate) fn graphviz_string(&self) -> String {
        let mut graphviz_string = String::from("digraph {\n");

        for (label, block) in self.blocks() {
            let loc_none = SourceLoc::none();
            let block_loc = block
                .into_iter()
                .filter(|&statement| statement.loc.valid())
                .map(|statement| &statement.loc)
                .next()
                .unwrap_or(&loc_none);

            graphviz_string += &format!("{}[label=\"{}\n{}\n", label, label, block_loc);
            for statement in block {
                graphviz_string += &format!("{}\n", statement);
            }
            graphviz_string += "\"];\n";

            for successor in self.successors(label).unwrap() {
                graphviz_string += &format!("{}->{};", label, *successor);
            }
            graphviz_string += "\n";
        }

        graphviz_string += "}";
        graphviz_string
    }

    pub(crate) fn values(&self) -> &ValueInterner {
        &self.values
    }

    pub(crate) fn values_mut(&mut self) -> &mut ValueInterner {
        &mut self.values
    }
}

impl ControlFlow {
    pub(crate) fn infer_types(
        &mut self,
        mut symtab: Box<symtab::SymbolTable>,
    ) -> (bool, Box<symtab::SymbolTable>) {
        let mut block_order: BTreeSet<usize> = self
            .generate_reverse_postorder_stack()
            .into_iter()
            .collect();

        let (values, blocks) = (&mut self.values, &mut self.blocks);
        let ctx = TypeInferenceContext::new(&mut symtab, values);
        loop {
            let old_size = block_order.len();

            block_order.retain(|label| blocks.get_mut(label).unwrap().infer_types(&ctx));
            if old_size == block_order.len() {
                break;
            }
        }

        (block_order.is_empty(), symtab)
    }
}

impl IntoIterator for ControlFlow {
    type Item = (usize, BasicBlock);
    type IntoIter = std::collections::hash_map::IntoIter<usize, BasicBlock>;
    fn into_iter(self) -> Self::IntoIter {
        self.blocks.into_iter()
    }
}

impl<'a> IntoIterator for &'a ControlFlow {
    type Item = &'a BasicBlock;
    type IntoIter = std::collections::hash_map::Values<'a, usize, BasicBlock>;
    fn into_iter(self) -> Self::IntoIter {
        self.blocks.values()
    }
}

impl<'a> IntoIterator for &'a mut ControlFlow {
    type Item = &'a mut BasicBlock;
    type IntoIter = std::collections::hash_map::ValuesMut<'a, usize, BasicBlock>;
    fn into_iter(self) -> Self::IntoIter {
        self.blocks.values_mut()
    }
}

impl From<BlockManager> for ControlFlow {
    fn from(manager: BlockManager) -> Self {
        let (blocks, values) = manager.try_take().unwrap();
        Self::new(blocks, values)
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        frontend::sourceloc::SourceLoc,
        midend::{
            ir::{
                lowered::{BinaryComparisonKind, BinaryComparisonOperands, JumpCondition},
                BasicBlock, ControlFlow, IrLine, ValueId, ValueInterner,
            },
            symtab::RawPath,
            types::Semantic,
        },
    };
    use std::collections::{BTreeSet, HashMap};

    fn test_control_flow() -> ControlFlow {
        let mut b0 = BasicBlock::new(0, RawPath::empty());
        let mut b1 = BasicBlock::new(1, RawPath::empty());
        let mut b2 = BasicBlock::new(2, RawPath::empty());
        let b3 = BasicBlock::new(3, RawPath::empty());

        // 0->1
        let jump = IrLine::new_jump(SourceLoc::none(), 1, JumpCondition::Unconditional);
        b0.push(jump);

        // 1->2 (conditional)
        let jump = IrLine::new_jump(
            SourceLoc::none(),
            2,
            JumpCondition::Conditional(BinaryComparisonOperands::new(
                ValueId::new(0),
                ValueId::new(1),
                BinaryComparisonKind::EQ,
            )),
        );
        b1.push(jump);
        // 1->3
        let jump = IrLine::new_jump(SourceLoc::none(), 3, JumpCondition::Unconditional);
        b1.push(jump);

        // 2->1
        let jump = IrLine::new_jump(SourceLoc::none(), 1, JumpCondition::Unconditional);
        b2.push(jump);

        let blocks: HashMap<usize, BasicBlock> = vec![b0, b1, b2, b3]
            .into_iter()
            .map(|block| (block.label, block))
            .collect();

        ControlFlow::new(blocks, ValueInterner::new(Semantic { id: 0 }))
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

use std::{
    collections::{BTreeSet, HashMap, VecDeque},
    ops,
};

use frontend::sourceloc;
use ooo_iter::{HashMapOOOIter, HashMapOOOIterMut};

use crate::{
    ir::{self, BasicBlock, Operation, ValueInterner, lowered, unlowered},
    symtab,
    types::{self, Inference},
};

#[derive(Debug, Clone)]
pub(crate) struct ControlFlow {
    blocks: HashMap<usize, BasicBlock>,
    successors: HashMap<usize, BTreeSet<usize>>,
    #[allow(unused)]
    predecessors: HashMap<usize, BTreeSet<usize>>,
    values: ValueInterner<Option<types::Syntactic>>,
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
    pub(crate) fn new(
        blocks: HashMap<usize, BasicBlock>,
        values: ValueInterner<Option<types::Syntactic>>,
    ) -> Self {
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

                        assert!(
                            blocks.contains_key(&jump.destination_block),
                            "Invalid jump target to nonexistent block {}",
                            jump.destination_block
                        );
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

    pub(crate) fn take(
        self,
    ) -> (
        HashMap<usize, BasicBlock>,
        ValueInterner<Option<types::Syntactic>>,
    ) {
        (self.blocks, self.values)
    }

    pub(crate) fn successors(&self, label: usize) -> Option<&BTreeSet<usize>> {
        self.successors.get(&label)
    }

    #[allow(unused)]
    pub(crate) fn predecessors(&self, label: usize) -> Option<&BTreeSet<usize>> {
        self.predecessors.get(&label)
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

                for successor in self.successors(label).unwrap() {
                    dfs_stack.push(*successor);
                }
            }
        }
        postorder_stack
    }

    #[allow(unused)]
    pub(crate) fn blocks_postorder(&self) -> HashMapOOOIter<'_, usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        HashMapOOOIter::new(&self.blocks, rpo_stack.into_iter().rev())
    }

    #[allow(unused)]
    pub(crate) fn blocks_postorder_mut(&mut self) -> HashMapOOOIterMut<'_, usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        HashMapOOOIterMut::new(&mut self.blocks, rpo_stack.into_iter().rev())
    }

    #[allow(unused)]
    pub(crate) fn blocks_reverse_postorder(&self) -> HashMapOOOIter<'_, usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        HashMapOOOIter::new(&self.blocks, rpo_stack.into_iter())
    }

    #[allow(unused)]
    pub(crate) fn blocks_reverse_postorder_mut(
        &mut self,
    ) -> HashMapOOOIterMut<'_, usize, ir::BasicBlock> {
        let rpo_stack = self.generate_reverse_postorder_stack();

        HashMapOOOIterMut::new(&mut self.blocks, rpo_stack.into_iter())
    }

    #[allow(clippy::format_push_string)]
    pub(crate) fn graphviz_string(&self) -> String {
        let mut graphviz_string = String::from("digraph {\n");

        // WOW!
        let values_str = self
            .values
            .ids()
            .by_ref()
            .map(|(value, id)| format!("{id}:{value:?}"))
            .collect::<Vec<_>>()
            .join("\n");
        graphviz_string += &format!("values[label=\"{values_str}\"]");

        for (label, block) in self.blocks() {
            let loc_none = sourceloc::SourceLoc::none();
            let block_loc = block
                .into_iter()
                .filter(|&statement| statement.loc.valid())
                .map(|statement| &statement.loc)
                .next()
                .unwrap_or(&loc_none);

            graphviz_string += &format!("{label}[label=\"{label}\n{block_loc}\n");
            for statement in block {
                graphviz_string += &format!("{statement}\n");
            }
            graphviz_string += "\"];\n";

            for successor in self.successors(*label).unwrap() {
                graphviz_string += &format!("{}->{};", label, *successor);
            }
            graphviz_string += "\n";
        }

        graphviz_string += "}";
        graphviz_string
    }

    #[allow(unused)]
    pub(crate) fn values(&self) -> &ValueInterner<Option<types::Syntactic>> {
        &self.values
    }

    #[allow(unused)]
    pub(crate) fn values_mut(&mut self) -> &mut ValueInterner<Option<types::Syntactic>> {
        &mut self.values
    }
}

impl ControlFlow {
    pub(crate) fn infer_types(
        &mut self,
        symtab: &symtab::SymbolTable,
        types: &types::Interner,
    ) -> bool {
        let block_order: BTreeSet<usize> = self
            .generate_reverse_postorder_stack()
            .into_iter()
            .collect();

        let (values, blocks) = (&mut self.values, &mut self.blocks);
        let mut ctx = types::inference::Ctx::new(symtab, types, values);

        // map of block -> set of statement indices that need to be reinferred
        let mut reinfer_statements = HashMap::<usize, BTreeSet<usize>>::new();

        for label in &block_order {
            let block = blocks.get(label).unwrap();
            let reinfer_at_block = (0..block.len()).collect();
            reinfer_statements.insert(*label, reinfer_at_block);
        }

        while !reinfer_statements.is_empty() {
            let mut missing_ids = BTreeSet::new();

            let start_stmt_count: usize = reinfer_statements.values().map(BTreeSet::len).sum();
            for label in &block_order {
                let block = blocks.get_mut(label).unwrap();

                let mut reinfer_at_block = BTreeSet::new();

                for (idx, stmt) in block.into_iter().enumerate() {
                    match stmt.infer_types(&mut ctx) {
                        ops::ControlFlow::Continue(_) => {
                            // FUTURE: use this to build dependency graphs for type inference
                        }
                        ops::ControlFlow::Break(reason) => {
                            // FUTURE: use this to build dependency graphs for type inference
                            if let types::inference::BreakReason::Stalled(id) = reason {
                                missing_ids.insert(id);
                                reinfer_at_block.insert(idx);
                            }
                        }
                    }
                }

                if reinfer_at_block.is_empty() {
                    reinfer_statements.remove(label);
                } else {
                    reinfer_statements.insert(*label, reinfer_at_block);
                }
            }

            let end_stmt_count: usize = reinfer_statements.values().map(BTreeSet::len).sum();
            if start_stmt_count == end_stmt_count {
                dbg!(&reinfer_statements);
            }
            assert_ne!(
                start_stmt_count, end_stmt_count,
                "type inference reached unexpected fixed point"
            );
        }

        block_order.is_empty()
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

#[cfg(test)]
mod tests {
    use crate::{
        frontend::sourceloc::SourceLoc,
        midend::{
            ir::{
                BasicBlock, ControlFlow, IrLine, ValueId, ValueInterner,
                lowered::{BinaryComparisonKind, BinaryComparisonOperands, JumpCondition},
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

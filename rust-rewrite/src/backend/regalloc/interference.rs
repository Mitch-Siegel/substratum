use super::LifetimeSet;
use crate::midend;
use std::collections::{BTreeMap, BTreeSet};

pub struct InterferenceGraph {
    graph: BTreeMap<midend::ir::OperandName, BTreeSet<midend::ir::OperandName>>,
}

impl InterferenceGraph {
    pub fn new() -> Self {
        Self {
            graph: BTreeMap::new(),
        }
    }

    pub fn record_interference(
        &mut self,
        operand_a: &midend::ir::OperandName,
        operand_b: &midend::ir::OperandName,
    ) {
        self.graph
            .entry(operand_a.clone())
            .or_default()
            .insert(operand_b.clone());

        self.graph
            .entry(operand_b.clone())
            .or_default()
            .insert(operand_a.clone());
    }

    pub fn remove(&mut self, operand: &midend::ir::OperandName) {
        self.graph.remove(operand);

        for (_, interfered) in &mut self.graph {
            interfered.remove(operand);
        }
    }

    pub fn iter(
        &self,
    ) -> std::collections::btree_map::Iter<
        '_,
        midend::ir::OperandName,
        BTreeSet<midend::ir::OperandName>,
    > {
        self.graph.iter()
    }
}

impl From<&midend::ir::ControlFlow> for InterferenceGraph {
    fn from(control_flow: &midend::ir::ControlFlow) -> Self {
        let mut graph = Self::new();
        for (_, block) in &control_flow.blocks {
            let block_lifetimes = LifetimeSet::from_block(block);

            for index in 0..block.statements.len() {
                let mut first_iter = block_lifetimes.lifetimes.iter();

                while let Some((first_name, first_lt)) = first_iter.next() {
                    if first_lt.live_at(&index) {
                        let mut second_iter = first_iter.clone();
                        while let Some((second_name, second_lt)) = second_iter.next() {
                            if second_lt.live_at(&index) {
                                graph.record_interference(first_name, second_name);
                            }
                        }
                    }
                }
            }
        }
        graph
    }
}

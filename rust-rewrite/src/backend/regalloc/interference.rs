use super::LifetimeSet;
use crate::backend::regalloc::find_block_depths;
use crate::backend::regalloc::lifetime::Lifetime;
use crate::backend::regalloc::program_point::ProgramPoint;
use crate::midend;
use std::collections::{BTreeMap, BTreeSet};

pub struct InterferenceGraph {
    graph: BTreeMap<midend::ir::OperandName, BTreeSet<midend::ir::OperandName>>,
    max_degree: usize,
}

impl InterferenceGraph {
    pub fn new() -> Self {
        Self {
            graph: BTreeMap::new(),
            max_degree: usize::MIN,
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

        self.max_degree = self
            .max_degree
            .max(self.graph.get(operand_a).unwrap().len());
    }

    pub fn remove(
        &mut self,
        operand: &midend::ir::OperandName,
    ) -> Option<BTreeSet<midend::ir::OperandName>> {
        let removed = self.graph.remove(operand);

        self.max_degree = usize::MIN;
        for (_, interfered) in &mut self.graph {
            interfered.remove(operand);
            self.max_degree = self.max_degree.max(interfered.len());
        }

        removed
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

    pub fn keys(
        &self,
    ) -> std::collections::btree_map::Keys<
        '_,
        midend::ir::OperandName,
        BTreeSet<midend::ir::OperandName>,
    > {
        self.graph.keys()
    }
}

impl<'a> FromIterator<&'a Lifetime> for InterferenceGraph {
    fn from_iter<I: IntoIterator<Item = &'a Lifetime>>(iter: I) -> Self {
        let mut graph = Self::new();

        let all_lifetimes = iter
            .into_iter()
            .map(|lifetime| lifetime)
            .collect::<BTreeSet<_>>();

        let mut first_iter = all_lifetimes.iter();
        while let Some(lifetime) = first_iter.next() {
            for second_lt in first_iter.clone() {
                if lifetime.overlaps(&second_lt) {
                    graph.record_interference(&lifetime.name, &second_lt.name);
                }
            }
        }
        graph
    }
}

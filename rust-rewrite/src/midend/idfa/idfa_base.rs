use std::{
    collections::{BTreeSet, HashMap},
    fmt::Display,
};

use crate::{midend::ir, trace};

#[allow(dead_code)]
#[derive(Debug)]
pub(crate) enum IdfaAnalysisDirection {
    Forward,
    Backward,
}

#[derive(Debug, Clone, PartialEq)]
pub(crate) struct BlockFacts<T> {
    pub in_: BTreeSet<T>,
    pub out: BTreeSet<T>,
    pub gen: BTreeSet<T>,
    pub kill: BTreeSet<T>,
}

impl<T> Default for BlockFacts<T> {
    fn default() -> Self {
        BlockFacts {
            in_: BTreeSet::<T>::new(),
            out: BTreeSet::<T>::new(),
            gen: BTreeSet::<T>::new(),
            kill: BTreeSet::<T>::new(),
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub(crate) struct Facts<T>
where
    T: Display + PartialEq,
{
    facts: HashMap<usize, BlockFacts<T>>,
}

impl<T> Facts<T>
where
    T: Display + PartialEq,
{
    pub(crate) fn new(n_blocks: usize) -> Self {
        Self {
            facts: HashMap::with_capacity(n_blocks),
        }
    }

    // return facts for a given label
    // requires &mut self in case of missing entry needing or_default()
    pub(crate) fn for_label(&self, label: usize) -> Option<&BlockFacts<T>> {
        self.facts.get(&label)
    }

    pub(crate) fn for_label_mut(&mut self, label: usize) -> &mut BlockFacts<T> {
        self.facts.entry(label).or_default()
    }
}

#[allow(dead_code)]
pub(crate) trait IdfaImplementor<'a, T>
where
    T: Display + PartialEq,
{
    fn f_transfer(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>;
    fn f_find_gen_kills(control_flow: &'a ir::ControlFlow, facts: &mut Facts<T>);
    fn f_meet(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>;
    fn new(control_flow: &'a ir::ControlFlow) -> Self;
    fn reanalyze(&mut self);
    fn take_facts(self) -> Facts<T>;
    fn facts(&self) -> &Facts<T>;
    fn facts_mut(&mut self) -> &mut Facts<T>;
}

#[derive(Debug)]
pub(crate) struct Idfa<'a, T>
where
    T: Display + PartialEq,
{
    control_flow: &'a ir::ControlFlow,
    direction: IdfaAnalysisDirection,
    last_facts: Facts<T>,
    pub facts: Facts<T>,
    f_find_gen_kills: fn(control_flow: &'a ir::ControlFlow, facts: &mut Facts<T>),
    f_meet: fn(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>,
    f_transfer: fn(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>,
}

#[allow(dead_code)]
impl<'a, T> Idfa<'a, T>
where
    Facts<T>: PartialEq,
    T: std::fmt::Debug + Display + Clone + Ord,
{
    fn store_facts_as_last(&mut self) {
        self.last_facts = self.facts.clone();
    }

    fn reached_fixpoint(&mut self) -> bool {
        self.facts == self.last_facts
    }

    fn predecessors(&self, block: &ir::BasicBlock) -> impl Iterator<Item = &'a usize> {
        self.control_flow.predecessors(block.label).unwrap().iter()
    }

    fn successors(&self, block: &ir::BasicBlock) -> impl Iterator<Item = &'a usize> {
        self.control_flow.predecessors(block.label).unwrap().iter()
    }

    fn analyze_block_forwards(&mut self, block: &ir::BasicBlock) {
        let label = block.label;
        let mut new_in_facts = BTreeSet::<T>::new();

        for predecessor in self.predecessors(block).copied() {
            new_in_facts = (self.f_meet)(new_in_facts, &self.facts.for_label_mut(predecessor).out);
        }

        self.facts
            .for_label_mut(label)
            .in_
            .clone_from(&new_in_facts);
        let transferred = (self.f_transfer)(self.facts.for_label_mut(label), new_in_facts);
        self.facts.for_label_mut(label).out = transferred;
    }

    fn analyze_forward(&mut self) {
        let _ = trace::span_auto_trace!("Idfa::analyze_forward()");
        let mut iteration: usize = 0;
        while !self.reached_fixpoint() || (iteration == 0) {
            trace::trace!("Iteration {} of idfa", iteration);
            self.store_facts_as_last();

            for block in self.control_flow {
                self.analyze_block_forwards(block);
            }
            iteration += 1;
        }
    }

    fn analyze_backward(&mut self) {
        unimplemented!();
        /*
        let mut iteration: usize = 0;
        while !self.reached_fixpoint() || (iteration == 0) {
            self.store_facts_as_last();
            iteration += 1;
        }
        */
    }

    pub(crate) fn analyze(&mut self) {
        (self.f_find_gen_kills)(self.control_flow, &mut self.facts);
        match self.direction {
            IdfaAnalysisDirection::Forward => {
                self.analyze_forward();
            }
            IdfaAnalysisDirection::Backward => {
                self.analyze_backward();
            }
        }
    }

    pub(crate) fn new(
        control_flow: &'a ir::ControlFlow,
        direction: IdfaAnalysisDirection,
        f_find_gen_kills: fn(control_flow: &'a ir::ControlFlow, facts: &mut Facts<T>),
        f_meet: fn(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>,
        f_transfer: fn(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>,
    ) -> Self {
        let mut idfa = Self {
            control_flow,
            direction,
            last_facts: Facts::<T>::new(0 /*control_flow.blocks.len()*/),
            facts: Facts::<T>::new(0 /*control_flow.blocks.len()*/),
            f_find_gen_kills,
            f_meet,
            f_transfer,
        };

        idfa.analyze();

        idfa
    }

    pub(crate) fn blocks(&self) -> impl Iterator<Item = &ir::BasicBlock> {
        self.control_flow.blocks().map(|(_, block)| block)
    }
}

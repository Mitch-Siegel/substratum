use std::{
    collections::{BTreeSet, HashMap},
    fmt::Display,
};

use crate::midend::ir;

#[derive(Debug)]
pub enum IdfaAnalysisDirection {
    Forward,
    Backward,
}

#[derive(Debug, Clone, PartialEq)]
pub struct BlockFacts<T> {
    pub in_facts: BTreeSet<T>,
    pub out_facts: BTreeSet<T>,
    pub gen_facts: BTreeSet<T>,
    pub kill_facts: BTreeSet<T>,
}

impl<T> BlockFacts<T> {
    pub fn new() -> Self {
        BlockFacts {
            in_facts: BTreeSet::<T>::new(),
            out_facts: BTreeSet::<T>::new(),
            gen_facts: BTreeSet::<T>::new(),
            kill_facts: BTreeSet::<T>::new(),
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct Facts<T>
where
    T: Display + PartialEq,
{
    facts: HashMap<usize, BlockFacts<T>>,
}

impl<T> Facts<T>
where
    T: Display + PartialEq,
{
    pub fn new(n_blocks: usize) -> Self {
        Self {
            facts: HashMap::new(),
        }
    }
    pub fn for_label(&self, label: usize) -> &BlockFacts<T> {
        self.facts.get(&label).unwrap()
    }

    pub fn for_label_mut(&mut self, label: usize) -> &mut BlockFacts<T> {
        self.facts.entry(label).or_insert(BlockFacts::<T>::new())
    }
}

pub trait IdfaImplementor<'a, T>
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
pub struct Idfa<'a, T>
where
    T: Display + PartialEq,
{
    pub control_flow: &'a ir::ControlFlow,
    direction: IdfaAnalysisDirection,
    last_facts: Facts<T>,
    pub facts: Facts<T>,
    f_find_gen_kills: fn(control_flow: &'a ir::ControlFlow, facts: &mut Facts<T>),
    f_meet: fn(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>,
    f_transfer: fn(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>,
}

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

    fn analyze_block_forwards<'b>(&mut self, block: &ir::BasicBlock) {
        let label = block.label;
        let mut new_in_facts = BTreeSet::<T>::new();

        for predecessor in &block.predecessors {
            new_in_facts =
                (self.f_meet)(new_in_facts, &self.facts.for_label(*predecessor).out_facts);
        }

        self.facts.for_label_mut(label).in_facts = new_in_facts.clone();
        let transferred = (self.f_transfer)(self.facts.for_label_mut(label), new_in_facts);
        self.facts.for_label_mut(label).out_facts = transferred;
    }

    fn analyze_forward(&mut self) {
        let mut first_iteration = true;
        while !self.reached_fixpoint() || first_iteration {
            first_iteration = false;
            self.store_facts_as_last();

            for (_, block) in &self.control_flow.blocks {
                self.analyze_block_forwards(block);
            }
        }
    }

    fn analyze_backward(&mut self) {
        // let mut first_iteration = true;
        // while !self.reached_fixpoint() || first_iteration {
        unimplemented!();
        // first_iteration = false;
        // self.store_facts_as_last();
        // }
    }

    pub fn analyze(&mut self) {
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

    pub fn new(
        control_flow: &'a ir::ControlFlow,
        direction: IdfaAnalysisDirection,
        f_find_gen_kills: fn(control_flow: &'a ir::ControlFlow, facts: &mut Facts<T>),
        f_meet: fn(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>,
        f_transfer: fn(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>,
    ) -> Self {
        let mut idfa = Self {
            control_flow,
            direction,
            last_facts: Facts::<T>::new(control_flow.blocks.len()),
            facts: Facts::<T>::new(control_flow.blocks.len()),
            f_find_gen_kills,
            f_meet,
            f_transfer,
        };

        idfa.analyze();

        idfa
    }
}

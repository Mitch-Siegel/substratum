use std::{
    collections::{BTreeMap, BTreeSet},
    fmt::Display,
};

use crate::midend::{idfa, ir};

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
    facts: Vec<BlockFacts<T>>,
}

impl<T> Facts<T>
where
    T: Display + PartialEq,
{
    pub fn new(n_blocks: usize) -> Self {
        let mut facts = Vec::<BlockFacts<T>>::new();

        for _ in 0..=n_blocks {
            facts.push(BlockFacts::<T>::new());
        }

        Self { facts }
    }
    pub fn for_label(&self, label: usize) -> &BlockFacts<T> {
        self.facts.get(label).unwrap()
    }

    pub fn for_label_mut(&mut self, label: usize) -> &mut BlockFacts<T> {
        self.facts.get_mut(label).unwrap()
    }

    pub fn into_map(self) -> BTreeMap<usize, BlockFacts<T>> {
        let mut return_map = BTreeMap::<usize, BlockFacts<T>>::new();

        for (label, block_facts) in self.facts.into_iter().enumerate() {
            return_map.insert(label, block_facts);
        }

        return_map
    }

    pub fn from_map(map: BTreeMap<usize, BlockFacts<T>>) -> Self {
        let mut constructed = Self {
            facts: Vec::<BlockFacts<T>>::new(),
        };

        for (_label, block_facts) in map {
            constructed.facts.push(block_facts);
        }

        constructed
    }
}

pub trait IdfaImplementor<'a, T>
where
    T: Display + PartialEq,
{
    fn f_transfer(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>;
    fn f_find_gen_kills(control_flow: &'a ir::ControlFlow, facts: &mut Facts<T>);
    fn f_meet(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>;
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

    fn analyze_block_forwards<'b>(
        block: &ir::BasicBlock,
        idfa: Box<&'b mut Idfa<'a, T>>,
    ) -> Box<&'b mut Idfa<'a, T>> {
        let label = block.label();
        let mut new_in_facts = BTreeSet::<T>::new();

        for predecessor in &idfa.control_flow.predecessors[block.label()] {
            new_in_facts =
                (idfa.f_meet)(new_in_facts, &idfa.facts.for_label(*predecessor).out_facts);
        }

        idfa.facts.for_label_mut(label).in_facts = new_in_facts.clone();
        let transferred = (idfa.f_transfer)(idfa.facts.for_label_mut(label), new_in_facts);
        idfa.facts.for_label_mut(label).out_facts = transferred;

        idfa
    }

    fn analyze_forward(&mut self) {
        let mut first_iteration = true;
        while !self.reached_fixpoint() || first_iteration {
            first_iteration = false;
            self.store_facts_as_last();

            self.control_flow
                .map_over_blocks_by_bfs::<&mut Idfa<T>>(Self::analyze_block_forwards, self);
        }
    }

    fn analyze_backward(&mut self) {
        let mut first_iteration = true;
        while first_iteration || !self.reached_fixpoint() {
            first_iteration = false;

            self.store_facts_as_last();
        }
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
        Self {
            control_flow,
            direction,
            last_facts: Facts::<T>::new(control_flow.blocks.len()),
            facts: Facts::<T>::new(control_flow.blocks.len()),
            f_find_gen_kills,
            f_meet,
            f_transfer,
        }
    }
}

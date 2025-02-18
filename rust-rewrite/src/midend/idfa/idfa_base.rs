use std::collections::BTreeSet;

use crate::midend::control_flow::ControlFlow;

pub enum IdfaAnalysisDirection {
    Forward,
    Backward,
}

#[derive(Clone, PartialEq)]
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

#[derive(Clone, PartialEq)]
pub struct IdfaFacts<T>
where
    T: PartialEq,
{
    facts: Vec<BlockFacts<T>>,
}

impl<T> IdfaFacts<T>
where
    T: PartialEq,
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
}

pub trait IdfaImplementor<'a, T>
where
    T: PartialEq,
{
    fn f_transfer(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>;
    fn f_find_gen_kills(control_flow: &'a ControlFlow, facts: &mut IdfaFacts<T>);
    fn f_meet(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>;
}

pub struct Idfa<'a, T>
where
    T: PartialEq,
{
    pub control_flow: &'a ControlFlow,
    direction: IdfaAnalysisDirection,
    last_facts: IdfaFacts<T>,
    pub facts: IdfaFacts<T>,
    f_find_gen_kills: fn(control_flow: &'a ControlFlow, facts: &mut IdfaFacts<T>),
    f_meet: fn(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>,
    f_transfer: fn(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>,
}

impl<'a, T> Idfa<'a, T>
where
    IdfaFacts<T>: PartialEq,
    T: Clone,
    T: Ord,
{
    fn store_facts_as_last(&mut self) {
        self.last_facts = self.facts.clone();
    }

    fn reached_fixpoint(&mut self) -> bool {
        self.facts == self.last_facts
    }

    fn analyze_forward(&mut self) {
        let mut first_iteration = true;
        while !self.reached_fixpoint() || first_iteration {
            first_iteration = false;
            self.store_facts_as_last();

            for block in &self.control_flow.blocks {
                let label = block.label();

                let mut new_in_facts = BTreeSet::<T>::new();

                for predecessor in &self.control_flow.predecessors[block.label()] {
                    new_in_facts =
                        (self.f_meet)(new_in_facts, &self.facts.for_label(*predecessor).out_facts);
                }

                self.facts.for_label_mut(label).in_facts = new_in_facts.clone();
                let transferred =
                    (self.f_transfer)(&mut self.facts.for_label_mut(label), new_in_facts);
                self.facts.for_label_mut(label).out_facts = transferred;
            }
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
        control_flow: &'a ControlFlow,
        direction: IdfaAnalysisDirection,
        f_find_gen_kills: fn(control_flow: &'a ControlFlow, facts: &mut IdfaFacts<T>),
        f_meet: fn(a: BTreeSet<T>, b: &BTreeSet<T>) -> BTreeSet<T>,
        f_transfer: fn(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T>,
    ) -> Self {
        Self {
            control_flow,
            direction,
            last_facts: IdfaFacts::<T>::new(control_flow.blocks.len()),
            facts: IdfaFacts::<T>::new(control_flow.blocks.len()),
            f_find_gen_kills,
            f_meet,
            f_transfer,
        }
    }
}

use std::collections::BTreeSet;

use crate::midend::control_flow::ControlFlow;

use super::idfa_base::{BlockFacts, Idfa, IdfaAnalysisDirection, IdfaFacts, IdfaImplementor};

pub struct ReachingDefs<'a, T>
where
    T: PartialOrd,
{
    idfa: Idfa<'a, T>,
}

impl<'a, T> IdfaImplementor<'a, T> for ReachingDefs<'a, T>
where
    T: std::cmp::Ord,
    T: Clone,
    T: std::fmt::Display,
{
    fn f_transfer(facts: &mut BlockFacts<T>, to_transfer: BTreeSet<T>) -> BTreeSet<T> {
        let mut transferred = BTreeSet::<T>::new();

        for gen_fact in &facts.gen_facts {
            if !facts.kill_facts.contains(gen_fact) {
                transferred.insert(gen_fact.clone());
            }
        }

        for in_fact in &facts.in_facts {
            if !facts.kill_facts.contains(in_fact) {
                transferred.insert(in_fact.clone());
            }
        }

        for transfer_fact in to_transfer {
            if !facts.kill_facts.contains(&transfer_fact) {
                transferred.insert(transfer_fact);
            }
        }

        transferred
    }

    fn f_find_gen_kills(
        control_flow: &'a crate::midend::control_flow::ControlFlow,
        facts: &mut IdfaFacts<T>,
    ) {
        for block in &control_flow.blocks {
            let label = block.label();

            let mut block_facts = facts.for_label_mut(label);

            // TODO: re-enable once SSA implemented
            // for statement in block.statements() {
            //     for read in statement.read_operands() {
            //         match read {
            //             ir::GenericOperand::<T>::Variable(name) => {
            //                 block_facts.kill_facts.insert(name.clone());
            //             }
            //             ir::GenericOperand::<T>::Temporary(name) => {
            //                 block_facts.kill_facts.insert(name.clone());
            //             }
            //             ir::GenericOperand::<T>::UnsignedDecimalConstant(_) => {}
            //         }
            //     }
            // }

            // for statement in block.statements() {
            //     for write in statement.write_operands() {
            //         match write {
            //             ir::GenericOperand::<T>::Variable(name) => {
            //                 block_facts.gen_facts.insert(name.clone());
            //             }
            //             ir::GenericOperand::<T>::Temporary(name) => {
            //                 block_facts.gen_facts.insert(name.clone());
            //             }
            //             IROperand::UnsignedDecimalConstant(_) => {}
            //         }
            //     }
            // }
        }
    }

    fn f_meet(
        mut a: std::collections::BTreeSet<T>,
        b: &std::collections::BTreeSet<T>,
    ) -> std::collections::BTreeSet<T> {
        for fact in b {
            a.insert((*fact).clone());
        }

        a
    }
}

impl<'a, T> ReachingDefs<'a, T>
// TODO: supertrait?
where
    T: std::fmt::Display,
    T: Clone,
    T: Ord,
{
    pub fn new(control_flow: &'a ControlFlow) -> Self {
        Self {
            idfa: Idfa::<'a, T>::new(
                control_flow,
                IdfaAnalysisDirection::Forward,
                Self::f_find_gen_kills,
                Self::f_meet,
                Self::f_transfer,
            ),
        }
    }

    pub fn analyze(&mut self) {
        self.idfa.analyze();
    }

    pub fn print(&self) {
        for label in 0..self.idfa.control_flow.blocks.len() {
            let facts = self.idfa.facts.for_label(label);
            println!("{}:", label);

            print!("\tGEN:");
            for gen_fact in &facts.gen_facts {
                print!("{} ", gen_fact);
            }
            println!();

            print!("\tKILL:");
            for kill_fact in &facts.kill_facts {
                print!("{} ", kill_fact);
            }
            println!();

            print!("\tIN:");
            for in_fact in &facts.in_facts {
                print!("{} ", in_fact);
            }
            println!();

            print!("\tOUT:");
            for out_fact in &facts.out_facts {
                print!("{} ", out_fact);
            }
            println!();
        }
    }
}

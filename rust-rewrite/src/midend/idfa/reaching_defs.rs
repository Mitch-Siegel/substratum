use std::collections::BTreeSet;

use crate::midend::{control_flow::ControlFlow, ir::IROperand};

use super::idfa_base::{BlockFacts, Idfa, IdfaAnalysisDirection, IdfaFacts, IdfaImplementor};

pub struct ReachingDefs<'a> {
    idfa: Idfa<'a, String>,
}

impl<'a> IdfaImplementor<'a, String> for ReachingDefs<'a>
where
    String: std::cmp::Ord,
    String: Clone,
{
    fn f_transfer(
        facts: &mut BlockFacts<String>,
        to_transfer: BTreeSet<String>,
    ) -> BTreeSet<String> {
        let mut transferred = BTreeSet::<String>::new();

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

        transferred
    }

    fn f_find_gen_kills(
        control_flow: &'a crate::midend::control_flow::ControlFlow,
        facts: &mut IdfaFacts<String>,
    ) {
        for block in &control_flow.blocks {
            let label = block.label();

            let mut block_facts = facts.for_label_mut(label);

            for statement in block.statements() {
                for read in statement.read_operands() {
                    match read {
                        IROperand::Variable(name) => {
                            block_facts.kill_facts.insert(name.clone());
                        }
                        IROperand::Temporary(name) => {
                            block_facts.kill_facts.insert(name.clone());
                        }
                        IROperand::UnsignedDecimalConstant(_) => {}
                    }
                }
            }

            for statement in block.statements() {
                for write in statement.write_operands() {
                    match write {
                        IROperand::Variable(name) => {
                            block_facts.gen_facts.insert(name.clone());
                        }
                        IROperand::Temporary(name) => {
                            block_facts.gen_facts.insert(name.clone());
                        }
                        IROperand::UnsignedDecimalConstant(_) => {}
                    }
                }
            }
        }
    }

    fn f_meet(
        mut a: std::collections::BTreeSet<String>,
        b: &std::collections::BTreeSet<String>,
    ) -> std::collections::BTreeSet<String> {
        for fact in b {
            a.insert((*fact).clone());
        }

        a
    }
}

impl<'a> ReachingDefs<'a> {
    pub fn new(control_flow: &'a ControlFlow) -> Self {
        Self {
            idfa: Idfa::<'a, String>::new(control_flow, IdfaAnalysisDirection::Forward, Self::f_find_gen_kills, Self::f_meet, Self::f_transfer)
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

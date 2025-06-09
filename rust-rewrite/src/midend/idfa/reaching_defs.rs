use std::collections::BTreeSet;

use crate::midend::ir;

use super::idfa_base;
pub use super::idfa_base::IdfaImplementor;

pub type Fact = ir::OperandName;
pub type BlockFacts = idfa_base::BlockFacts<Fact>;
pub type Facts = idfa_base::Facts<Fact>;

pub struct ReachingDefs<'a> {
    idfa: idfa_base::Idfa<'a, Fact>,
}

impl<'a> IdfaImplementor<'a, Fact> for ReachingDefs<'a> {
    fn f_transfer(facts: &mut BlockFacts, to_transfer: BTreeSet<Fact>) -> BTreeSet<Fact> {
        let mut transferred = BTreeSet::<Fact>::new();

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

    fn f_find_gen_kills(control_flow: &'a ir::ControlFlow, facts: &mut Facts) {
        //TODO: need to be able to possibly act on function arguments for gen/kill
        // e.g. reaching defs on function arguments

        /*for (label, block) in &control_flow.blocks {
            let block_facts = facts.for_label_mut(*label);

            for statement in &block.statements {
                for read in statement.read_operand_names() {
                    block_facts.kill_facts.insert(read.clone());
                }
                for write in statement.write_operand_names() {
                    block_facts.gen_facts.insert(write.clone());
                }
            }
        }*/
    }

    fn f_meet(
        mut a: std::collections::BTreeSet<Fact>,
        b: &std::collections::BTreeSet<Fact>,
    ) -> std::collections::BTreeSet<Fact> {
        for fact in b {
            a.insert((*fact).clone());
        }

        a
    }

    fn new(control_flow: &'a ir::ControlFlow) -> Self {
        Self {
            idfa: idfa_base::Idfa::new(
                control_flow,
                idfa_base::IdfaAnalysisDirection::Forward,
                Self::f_find_gen_kills,
                Self::f_meet,
                Self::f_transfer,
            ),
        }
    }

    fn reanalyze(&mut self) {
        self.idfa.analyze();
    }

    fn take_facts(self) -> Facts {
        self.idfa.facts
    }

    fn facts(&self) -> &Facts {
        &self.idfa.facts
    }

    fn facts_mut(&mut self) -> &mut Facts {
        &mut self.idfa.facts
    }
}

impl<'a> ReachingDefs<'a>
// TODO: supertrait?
{
    pub fn print(&self) {
        /*for label in self.idfa.control_flow.blocks.keys() {
            let facts = self.idfa.facts.for_label(*label);
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
        }*/
    }
}

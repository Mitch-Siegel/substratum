use std::collections::BTreeSet;

use crate::midend::ir;

use super::idfa_base;
use super::idfa_base::IdfaImplementor;

pub type Fact = ir::Operand;
pub type BlockFacts = idfa_base::BlockFacts<Fact>;
pub type Facts = idfa_base::Facts<Fact>;

pub struct ReachingDefs<'a> {
    idfa: idfa_base::Idfa<'a, ir::Operand>,
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

    fn f_find_gen_kills(control_flow: &'a ir::ControlFlow, _facts: &mut Facts) {
        for _label in control_flow.labels() {
            unimplemented!();
            // let mut block_facts = facts.for_label_mut(label);

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
        mut a: std::collections::BTreeSet<ir::Operand>,
        b: &std::collections::BTreeSet<ir::Operand>,
    ) -> std::collections::BTreeSet<ir::Operand> {
        for fact in b {
            a.insert((*fact).clone());
        }

        a
    }
}

impl<'a> ReachingDefs<'a>
// TODO: supertrait?
{
    pub fn new(control_flow: &'a ir::ControlFlow) -> Self {
        Self {
            idfa: idfa_base::Idfa::<'a, Fact>::new(
                control_flow,
                idfa_base::IdfaAnalysisDirection::Forward,
                Self::f_find_gen_kills,
                Self::f_meet,
                Self::f_transfer,
            ),
        }
    }

    pub fn analyze(&mut self) {
        self.idfa.analyze();
    }

    pub fn take_facts(self) -> Facts {
        self.idfa.facts
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

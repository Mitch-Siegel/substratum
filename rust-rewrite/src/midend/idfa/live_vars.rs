use std::collections::BTreeSet;

use crate::midend::ir;

use super::idfa_base::{self, IdfaImplementor};

pub type Fact = ir::NamedOperand;
pub type BlockFacts = idfa_base::BlockFacts<Fact>;
pub type Facts = idfa_base::Facts<Fact>;

pub struct LiveVars<'a> {
    idfa: idfa_base::Idfa<'a, Fact>,
}

impl<'a> IdfaImplementor<'a, Fact> for LiveVars<'a> {
    fn f_transfer(facts: &mut BlockFacts, to_transfer: BTreeSet<Fact>) -> BTreeSet<Fact> {
        let mut transferred = facts.gen_facts.clone();

        for fact in &to_transfer {
            if !facts.kill_facts.contains(fact) {
                transferred.insert(fact.clone());
            }
        }

        transferred
    }

    fn f_find_gen_kills(control_flow: &'a ir::ControlFlow, facts: &mut Facts) {
        for label in control_flow.labels() {
            let mut block_facts = facts.for_label_mut(label);

            let block = &control_flow.blocks[label];

            for statement in block.statements() {
                for read in statement.read_operands() {
                    match read {
                        ir::Operand::Variable(name) => {
                            block_facts.kill_facts.insert(name.clone());
                        }
                        ir::Operand::Temporary(name) => {
                            block_facts.kill_facts.insert(name.clone());
                        }
                        ir::Operand::UnsignedDecimalConstant(_) => {}
                    }
                }
                for write in statement.write_operands() {
                    match write {
                        ir::Operand::Variable(name) => {
                            block_facts.gen_facts.insert(name.clone());
                        }
                        ir::Operand::Temporary(name) => {
                            block_facts.gen_facts.insert(name.clone());
                        }
                        ir::Operand::UnsignedDecimalConstant(_) => {}
                    }
                }
            }
        }
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
}

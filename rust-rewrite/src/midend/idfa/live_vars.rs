use std::collections::BTreeSet;

use crate::midend::{idfa::*, ir};

pub type Fact = ir::OperandName;
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
        /*
        for (label, block) in &control_flow.blocks {
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

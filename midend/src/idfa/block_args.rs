use std::collections::BTreeSet;

use crate::midend::{
    idfa::{idfa_base, IdfaImplementor},
    ir,
};

pub(crate) type Fact = ir::ValueId;
pub(crate) type BlockFacts = idfa_base::BlockFacts<Fact>;
pub(crate) type Facts = idfa_base::Facts<Fact>;

pub(crate) struct BlockArgs<'a> {
    idfa: idfa_base::Idfa<'a, Fact>,
}

impl<'a> IdfaImplementor<'a, Fact> for BlockArgs<'a> {
    fn f_transfer(facts: &mut BlockFacts, _to_transfer: BTreeSet<Fact>) -> BTreeSet<Fact> {
        facts.gen.clone()
    }

    fn f_find_gen_kills(control_flow: &'a ir::ControlFlow, facts: &mut super::Facts<Fact>) {
        for block in control_flow {
            let block_facts = facts.for_label_mut(block.label);

            for statement in block {
                for read in statement.read_value_ids() {
                    if !block_facts.kill.contains(&read) {
                        block_facts.gen.insert(read);
                    }
                }
                for write in statement.write_value_ids() {
                    block_facts.kill.insert(write);
                }
            }
        }
    }

    fn f_meet(mut a: BTreeSet<Fact>, b: &BTreeSet<Fact>) -> BTreeSet<Fact> {
        for fact in b {
            a.insert(*fact);
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

    fn take_facts(self) -> super::Facts<Fact> {
        self.idfa.facts
    }

    fn facts(&self) -> &Facts {
        &self.idfa.facts
    }

    fn facts_mut(&mut self) -> &mut Facts {
        &mut self.idfa.facts
    }
}

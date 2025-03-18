use std::collections::BTreeSet;

use crate::midend::ir;

use super::idfa_base;
pub use super::idfa_base::IdfaImplementor;

pub type Fact = ir::OperandName;
pub type BlockFacts = idfa_base::BlockFacts<Fact>;
pub type Facts = idfa_base::Facts<Fact>;

pub struct BlockArgs<'a> {
    idfa: idfa_base::Idfa<'a, Fact>,
}

impl<'a> IdfaImplementor<'a, Fact> for BlockArgs<'a> {
    fn f_transfer(
        facts: &mut idfa_base::BlockFacts<Fact>,
        _to_transfer: BTreeSet<Fact>,
    ) -> BTreeSet<Fact> {
        facts.gen_facts.clone()
    }

    fn f_find_gen_kills(control_flow: &'a ir::ControlFlow, facts: &mut super::Facts<Fact>) {
        for (label, block) in &control_flow.blocks {
            let block_facts = facts.for_label_mut(*label);

            for statement in block.statements() {
                for read in statement.read_operand_names() {
                    if !block_facts.kill_facts.contains(read) {
                        block_facts.gen_facts.insert(read.clone());
                    }
                }
                for write in statement.write_operand_names() {
                    block_facts.kill_facts.insert(write.clone());
                }
            }
        }
    }

    fn f_meet(mut a: BTreeSet<Fact>, b: &BTreeSet<Fact>) -> BTreeSet<Fact> {
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

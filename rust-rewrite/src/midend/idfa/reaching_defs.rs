use std::collections::BTreeSet;

use crate::midend::{
    idfa::{idfa_base, IdfaImplementor},
    ir,
};

pub(crate) type Fact = ir::ValueId;
pub(crate) type BlockFacts = idfa_base::BlockFacts<Fact>;
pub(crate) type Facts = idfa_base::Facts<Fact>;

pub(crate) struct ReachingDefs<'a> {
    idfa: idfa_base::Idfa<'a, Fact>,
}

impl<'a> IdfaImplementor<'a, Fact> for ReachingDefs<'a> {
    fn f_transfer(facts: &mut BlockFacts, to_transfer: BTreeSet<Fact>) -> BTreeSet<Fact> {
        let mut transferred = BTreeSet::<Fact>::new();

        for gen_fact in &facts.gen {
            if !facts.kill.contains(gen_fact) {
                transferred.insert(*gen_fact);
            }
        }

        for in_fact in &facts.in_ {
            if !facts.kill.contains(in_fact) {
                transferred.insert(*in_fact);
            }
        }

        for transfer_fact in to_transfer {
            if !facts.kill.contains(&transfer_fact) {
                transferred.insert(transfer_fact);
            }
        }

        transferred
    }

    fn f_find_gen_kills(control_flow: &'a ir::ControlFlow, facts: &mut Facts) {
        //TODO: need to be able to possibly act on function arguments for gen/kill
        // e.g. reaching defs on function arguments

        for block in control_flow {
            let block_facts = facts.for_label_mut(block.label);

            for statement in block {
                for read in statement.read_value_ids() {
                    block_facts.kill.insert(read);
                }
                for write in statement.write_value_ids() {
                    block_facts.gen.insert(write);
                }
            }
        }
    }

    fn f_meet(
        mut a: std::collections::BTreeSet<Fact>,
        b: &std::collections::BTreeSet<Fact>,
    ) -> std::collections::BTreeSet<Fact> {
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

impl std::fmt::Display for ReachingDefs<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for block in self.idfa.blocks() {
            let label = block.label;
            let facts = self.idfa.facts.for_label(label).unwrap();
            write!(f, "{label}:")?;

            write!(f, "\tGEN:")?;
            for gen_fact in &facts.gen {
                write!(f, "{gen_fact} ")?;
            }
            writeln!(f)?;

            write!(f, "\tKILL:")?;
            for kill_fact in &facts.kill {
                write!(f, "{kill_fact} ")?;
            }
            writeln!(f)?;

            write!(f, "\tIN:")?;
            for in_fact in &facts.in_ {
                write!(f, "{in_fact} ")?;
            }
            writeln!(f)?;

            write!(f, "\tOUT:")?;
            for out_fact in &facts.out {
                write!(f, "{out_fact} ")?;
            }
            writeln!(f)?;
        }

        Ok(())
    }
}

use std::{
    cmp::Ordering,
    collections::{BTreeMap, HashMap},
    fmt,
};

use crate::midend;

use super::program_point::ProgramPoint;

#[derive(Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
pub struct Lifetime {
    pub name: midend::ir::OperandName,
    pub start: usize,
    pub end: usize,
    pub n_reads: usize,
    pub n_writes: usize,
}

impl Lifetime {
    pub fn new(name: midend::ir::OperandName) -> Self {
        Lifetime {
            name: name,
            start: usize::MAX,
            end: usize::MIN,
            n_reads: 0,
            n_writes: 0,
        }
    }

    fn update_range(&mut self, potential_range_limit: &usize) {
        if *potential_range_limit < self.start {
            self.start = *potential_range_limit;
        }
        if self.end < *potential_range_limit {
            self.end = *potential_range_limit;
        }
    }

    pub fn record_read(&mut self, at_index: &usize) {
        self.n_reads += 1;
        self.update_range(at_index);
    }

    pub fn record_write(&mut self, at_index: &usize) {
        self.n_writes += 1;
        self.update_range(at_index);
    }

    pub fn live_at(&self, at_index: &usize) -> bool {
        (self.start <= *at_index) && (self.end >= *at_index)
    }
}

impl std::fmt::Display for Lifetime {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[{}: {}-{}]", self.name, self.start, self.end)
    }
}

pub struct LifetimeSet {
    pub lifetimes: HashMap<midend::ir::OperandName, Lifetime>,
}

impl LifetimeSet {
    fn new() -> Self {
        LifetimeSet {
            lifetimes: HashMap::<midend::ir::OperandName, Lifetime>::new(),
        }
    }

    pub fn from_block(block: &midend::ir::BasicBlock) -> Self {
        let mut lifetimes = Self::new();

        for (index, line) in block.statements.iter().enumerate() {
            for read_operand in line.read_operand_names() {
                lifetimes.record_read_at_index(read_operand, &index);
            }

            for write_operand in line.write_operand_names() {
                lifetimes.record_write_at_index(write_operand, &index);
            }
        }

        lifetimes
    }

    fn lookup_or_create_lifetime_by_name(
        &mut self,
        name: &midend::ir::OperandName,
    ) -> &mut Lifetime {
        if !self.lifetimes.contains_key(name) {
            self.lifetimes
                .insert(name.clone(), Lifetime::new(name.clone()));
        }

        self.lifetimes.get_mut(name).unwrap()
    }

    pub fn record_read_at_index(&mut self, operand: &midend::ir::OperandName, index: &usize) {
        self.lookup_or_create_lifetime_by_name(operand)
            .record_read(index);
    }

    pub fn record_write_at_index(&mut self, operand: &midend::ir::OperandName, index: &usize) {
        self.lookup_or_create_lifetime_by_name(operand)
            .record_write(index);
    }

    pub fn values(
        &self,
    ) -> std::collections::hash_map::Values<'_, midend::ir::OperandName, Lifetime> {
        self.lifetimes.values()
    }

    pub fn print_numerical(&self) {
        for lifetime in self.lifetimes.values() {
            println!(
                "{:>20}: [{}-{}]",
                lifetime.name, lifetime.start, lifetime.end
            );
        }
    }
}

#[cfg(test)]
mod tests {
    use std::cmp::Ordering;

    use crate::{backend::regalloc::program_point::ProgramPoint, midend};

    use super::Lifetime;

    #[test]
    fn test_lifetime_range() {
        let mut dummy_lifetime = Lifetime::new(midend::ir::OperandName::new_basic("dummy".into()));

        dummy_lifetime.record_read(&1);

        assert!(dummy_lifetime.start == 1);
        assert!(dummy_lifetime.start == dummy_lifetime.end);

        dummy_lifetime.record_write(&0);

        assert!(dummy_lifetime.start == 0);
        assert!(dummy_lifetime.end == 1);
    }

    #[test]
    fn test_lifetime_format() {
        let mut dummy_lifetime =
            Lifetime::new(midend::ir::OperandName::new_basic("my_variable".into()));

        let start_point = 2;
        let end_point = 4;

        dummy_lifetime.record_write(&start_point);
        dummy_lifetime.record_write(&end_point);

        assert_eq!(
            format!("{}", dummy_lifetime),
            format!("[my_variable: {}-{}]", start_point, end_point)
        );
    }

    #[test]
    fn test_lifetime_live_at() {
        let mut dummy_lifetime = Lifetime::new(midend::ir::OperandName::new_basic("dummy".into()));

        assert!(!dummy_lifetime.live_at(&0));
        dummy_lifetime.record_write(&0);
        dummy_lifetime.record_write(&5);

        for index in 0..5 {
            assert!(dummy_lifetime.live_at(&index));
        }
    }
}

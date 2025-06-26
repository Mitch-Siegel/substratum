use crate::backend::regalloc::program_point::ProgramPoint;
use std::{collections::HashMap, fmt};

use crate::midend::{
    self,
    symtab::{BasicBlockOwner, VariableSizingContext},
};

use super::block_depths::find_block_depths;

#[derive(Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
pub struct Lifetime {
    pub name: midend::ir::OperandName,
    pub type_: midend::types::Type,
    pub start: ProgramPoint,
    pub end: ProgramPoint,
    pub n_reads: usize,
    pub n_writes: usize,
}

impl Lifetime {
    pub fn new(name: midend::ir::OperandName, type_: midend::types::Type) -> Self {
        Lifetime {
            name,
            type_,
            start: ProgramPoint::new(usize::MAX, usize::MAX),
            end: ProgramPoint::new(usize::MIN, usize::MIN),
            n_reads: 0,
            n_writes: 0,
        }
    }

    fn update_range(&mut self, potential_range_limit: &ProgramPoint) {
        if *potential_range_limit < self.start {
            self.start = *potential_range_limit;
        }
        if self.end < *potential_range_limit {
            self.end = *potential_range_limit;
        }
    }

    pub fn record_read(&mut self, at_point: &ProgramPoint) {
        self.n_reads += 1;
        self.update_range(at_point);
    }

    pub fn record_write(&mut self, at_point: &ProgramPoint) {
        self.n_writes += 1;
        self.update_range(at_point);
    }

    pub fn live_at(&self, at_point: &ProgramPoint) -> bool {
        (self.start <= *at_point) && (self.end >= *at_point)
    }

    pub fn overlaps(&self, other: &Self) -> bool {
        self.live_at(&other.start)
            || self.live_at(&other.end)
            || other.live_at(&self.start)
            || other.live_at(&self.end)
    }
}

impl std::fmt::Display for Lifetime {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[{}: {}-{}]", self.name, self.start, self.end)
    }
}

pub struct LifetimeSet {
    lifetimes: HashMap<midend::ir::OperandName, Lifetime>,
}

impl LifetimeSet {
    pub fn new<C>(function: &midend::symtab::Function, context: &C) -> Self
    where
        C: midend::symtab::VariableSizingContext,
    {
        let mut set = Self {
            lifetimes: HashMap::new(),
        };

        let block_depths = find_block_depths(&function.control_flow);

        for block in function.basic_blocks() {
            set.add_from_block(block, context, *block_depths.get(&block.label).unwrap())
        }

        set
    }

    fn add_from_block<C>(&mut self, block: &midend::ir::BasicBlock, context: &C, block_depth: usize)
    where
        C: midend::symtab::VariableSizingContext,
    {
        for (index, line) in block.statements.iter().enumerate() {
            let current_point = ProgramPoint::new(block_depth, index);

            for read_operand in line.read_operand_names() {
                self.record_read_at_point(read_operand, context, &current_point);
            }

            for write_operand in line.write_operand_names() {
                self.record_write_at_point(write_operand, context, &current_point);
            }
        }
    }

    fn lookup_or_create_lifetime_by_name<C>(
        &mut self,
        name: &midend::ir::OperandName,
        context: &C,
    ) -> &mut Lifetime
    where
        C: midend::symtab::VariableSizingContext,
    {
        if !self.lifetimes.contains_key(name) {
            self.lifetimes.insert(
                name.clone(),
                Lifetime::new(
                    name.clone(),
                    context
                        .lookup_variable_by_name(&name.base_name)
                        .unwrap()
                        .type_()
                        .clone(),
                ),
            );
        }

        self.lifetimes.get_mut(name).unwrap()
    }

    pub fn record_read_at_point<C>(
        &mut self,
        operand: &midend::ir::OperandName,
        context: &C,
        point: &ProgramPoint,
    ) where
        C: midend::symtab::VariableSizingContext,
    {
        self.lookup_or_create_lifetime_by_name(operand, context)
            .record_read(point);
    }

    pub fn record_write_at_point<C>(
        &mut self,
        operand: &midend::ir::OperandName,
        context: &C,
        point: &ProgramPoint,
    ) where
        C: midend::symtab::VariableSizingContext,
    {
        self.lookup_or_create_lifetime_by_name(operand, context)
            .record_write(point);
    }

    pub fn lookup_by_variable(&self, variable: &midend::symtab::Variable) -> Option<&Lifetime> {
        self.lifetimes
            .get(&midend::ir::OperandName::new_basic(variable.name.clone()))
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

    use crate::midend;

    use super::Lifetime;
    use crate::backend::regalloc::program_point::ProgramPoint;

    #[test]
    fn test_lifetime_range() {
        let mut dummy_lifetime = Lifetime::new(
            midend::ir::OperandName::new_basic("dummy".into()),
            midend::types::Type::I64,
        );

        dummy_lifetime.record_read(&ProgramPoint::new(0, 1));

        assert!(dummy_lifetime.start == ProgramPoint::new(0, 1));
        assert!(dummy_lifetime.start == dummy_lifetime.end);

        dummy_lifetime.record_write(&ProgramPoint::new(0, 0));

        assert!(dummy_lifetime.start == ProgramPoint::new(0, 0));
        assert!(dummy_lifetime.end == ProgramPoint::new(0, 1));
    }

    #[test]
    fn test_lifetime_format() {
        let mut dummy_lifetime = Lifetime::new(
            midend::ir::OperandName::new_basic("my_variable".into()),
            midend::types::Type::U8,
        );

        let start_point = ProgramPoint::new(0, 2);
        let end_point = ProgramPoint::new(0, 4);

        dummy_lifetime.record_write(&start_point);
        dummy_lifetime.record_write(&end_point);

        assert_eq!(
            format!("{}", dummy_lifetime),
            format!("[my_variable: {}-{}]", start_point, end_point)
        );
    }

    #[test]
    fn test_lifetime_live_at() {
        let mut dummy_lifetime = Lifetime::new(
            midend::ir::OperandName::new_basic("dummy".into()),
            midend::types::Type::I32,
        );

        assert!(!dummy_lifetime.live_at(&ProgramPoint::new(0, 0)));
        dummy_lifetime.record_write(&ProgramPoint::new(0, 0));
        dummy_lifetime.record_write(&ProgramPoint::new(0, 5));

        for index in 0..5 {
            assert!(dummy_lifetime.live_at(&ProgramPoint::new(0, index)));
        }
    }
}

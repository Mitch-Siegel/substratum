use std::{cmp::Ordering, collections::HashMap, fmt};

use crate::midend::{control_flow::ControlFlow, ir::IROperand};

#[derive(Copy, Clone)]
pub struct ProgramPoint {
    depth: usize,
    index: usize,
}

impl PartialOrd for ProgramPoint {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(
            self.depth
                .cmp(&other.depth)
                .then(self.index.cmp(&other.index)),
        )
    }
}

impl PartialEq for ProgramPoint {
    fn eq(&self, other: &Self) -> bool {
        (self.depth == other.depth) && (self.index == other.index)
    }
}

impl Eq for ProgramPoint {}

impl Ord for ProgramPoint {
    fn cmp(&self, other: &Self) -> Ordering {
        self.depth
            .cmp(&other.depth)
            .then(self.index.cmp(&other.index))
    }
}

impl ProgramPoint {
    pub fn new(depth: usize, index: usize) -> Self {
        ProgramPoint { depth, index }
    }

    pub fn depth(&self) -> usize {
        self.depth
    }

    pub fn index(&self) -> usize {
        self.index
    }
}

impl std::fmt::Display for ProgramPoint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:>2x}:{:<2x}", self.depth, self.index)
    }
}

#[derive(Clone)]
pub struct Lifetime {
    name: String,
    start: ProgramPoint,
    end: ProgramPoint,
    n_reads: usize,
    n_writes: usize,
}

impl Lifetime {
    pub fn new(name: String) -> Self {
        Lifetime {
            name: name,
            start: ProgramPoint::new(usize::MAX, usize::MAX),
            end: ProgramPoint::new(0, 0),
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

    pub fn start(&self) -> ProgramPoint {
        self.start
    }

    pub fn end(&self) -> ProgramPoint {
        self.end
    }

    pub fn name(&self) -> &String {
        &self.name
    }
}

impl std::fmt::Display for Lifetime {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[{}: {}-{}]", self.name, self.start, self.end)
    }
}

impl PartialEq for Lifetime {
    fn eq(&self, other: &Self) -> bool {
        (self.start == other.start) && (self.end == other.end) && (self.name == other.name)
    }
}

impl Eq for Lifetime {}

impl PartialOrd for Lifetime {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(
            self.start
                .cmp(&other.start)
                .then(self.end.cmp(&other.end))
                .then(self.name.cmp(&other.name)),
        )
    }
}

impl Ord for Lifetime {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.start
            .cmp(&other.start)
            .then(self.end.cmp(&other.end))
            .then(self.name.cmp(&other.name))
    }
}

pub struct LifetimeSet {
    lifetimes: HashMap<String, Lifetime>,
}

impl LifetimeSet {
    fn new() -> Self {
        LifetimeSet {
            lifetimes: HashMap::<String, Lifetime>::new(),
        }
    }

    pub fn from_control_flow(control_flow: &ControlFlow) -> Self {
        let mut lifetimes = Self::new();

        for block in control_flow.blocks() {
            for index in 0..block.statements().len() {
                let ir = &block.statements()[index];
                let current_point = ProgramPoint::new(block.label(), index);
                for read_operand in ir.read_operands() {
                    lifetimes.record_read_at_point(read_operand, &current_point);
                }
    
                for write_operand in ir.write_operands() {
                    lifetimes.record_write_at_point(write_operand, &current_point);
                }
            }
        }

        lifetimes
    }

    fn lookup_or_create_lifetime_by_name(&mut self, name: &String) -> &mut Lifetime {
        if !self.lifetimes.contains_key(name) {
            self.lifetimes
                .insert(name.clone(), Lifetime::new(name.clone()));
        }

        self.lifetimes.get_mut(name).unwrap()
    }

    pub fn record_read_at_point(&mut self, operand: &IROperand, at_point: &ProgramPoint) {
        match &operand {
            IROperand::Temporary(temp) => self
                .lookup_or_create_lifetime_by_name(temp)
                .record_read(at_point),
            IROperand::Variable(var) => self
                .lookup_or_create_lifetime_by_name(var)
                .record_read(at_point),
            IROperand::UnsignedDecimalConstant(_) => {}
        }
    }

    pub fn record_write_at_point(&mut self, operand: &IROperand, at_point: &ProgramPoint) {
        match &operand {
            IROperand::Temporary(temp) => self
                .lookup_or_create_lifetime_by_name(temp)
                .record_write(at_point),
            IROperand::Variable(var) => self
                .lookup_or_create_lifetime_by_name(var)
                .record_write(at_point),
            IROperand::UnsignedDecimalConstant(_) => {}
        }
    }

    pub fn print_numerical(&self) {
        for lifetime in self.lifetimes.values() {
            println!("{:>20}: [{}-{}]", lifetime.name, lifetime.start, lifetime.end);
        }
    }
}

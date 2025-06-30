use crate::backend::*;
use std::collections::HashMap;

#[derive(Debug)]
pub enum Location {
    Register(arch::generic::Register),
    RegisterRange(Vec<arch::generic::Register>),
    StackArgument(isize),
}

#[derive(Debug)]
pub struct AllocatedLocations {
    locations: HashMap<midend::ir::ValueId, Location>,
}

impl AllocatedLocations {
    pub fn new() -> Self {
        Self {
            locations: HashMap::new(),
        }
    }

    pub fn assign_to_register(
        &mut self,
        name: midend::ir::ValueId,
        register: arch::generic::Register,
    ) {
        self.locations.insert(name, Location::Register(register));
    }

    pub fn assign_to_register_range(
        &mut self,
        name: midend::ir::ValueId,
        register_range: &[arch::generic::Register],
    ) {
        self.locations
            .insert(name, Location::RegisterRange(Vec::from(register_range)));
    }

    pub fn assign_to_stack_argument(&mut self, name: midend::ir::ValueId, offset: isize) {
        self.locations.insert(name, Location::StackArgument(offset));
    }
}

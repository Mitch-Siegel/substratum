use crate::backend::*;
use std::collections::HashMap;

#[derive(Debug)]
pub(crate) enum Location {
    Register(arch::generic::Register),
    RegisterRange(Vec<arch::generic::Register>),
    StackArgument(isize),
}

#[derive(Debug)]
pub(crate) struct AllocatedLocations {
    locations: HashMap<midend::ir::OperandName, Location>,
}

impl AllocatedLocations {
    pub(crate) fn new() -> Self {
        Self {
            locations: HashMap::new(),
        }
    }

    pub(crate) fn assign_to_register(
        &mut self,
        name: midend::ir::OperandName,
        register: arch::generic::Register,
    ) {
        self.locations.insert(name, Location::Register(register));
    }

    pub(crate) fn assign_to_register_range(
        &mut self,
        name: midend::ir::OperandName,
        register_range: &[arch::generic::Register],
    ) {
        self.locations
            .insert(name, Location::RegisterRange(Vec::from(register_range)));
    }

    pub(crate) fn assign_to_stack_argument(&mut self, name: midend::ir::OperandName, offset: isize) {
        self.locations.insert(name, Location::StackArgument(offset));
    }
}

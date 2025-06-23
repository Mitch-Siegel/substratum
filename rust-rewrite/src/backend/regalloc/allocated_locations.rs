use crate::backend::*;
use std::collections::HashMap;

pub enum Location {
    Register(arch::generic::Register),
    RegisterRange(Vec<arch::generic::Register>),
    Stack(isize),
}

pub struct AllocatedLocations {
    locations: HashMap<midend::ir::Operand, Location>,
}

impl AllocatedLocations {
    pub fn new() -> Self {
        Self {
            locations: HashMap::new(),
        }
    }
}

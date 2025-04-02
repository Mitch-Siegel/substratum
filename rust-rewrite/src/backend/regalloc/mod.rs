use std::collections::{BTreeMap, BTreeSet};

use block_depths::*;

use crate::midend::{ir::ControlFlow, symtab};

mod block_depths;

pub fn allocate_registers(_scope: &symtab::Scope, control_flow: &ControlFlow) {
    println!("Allocate registers for scope");

    let depths = find_block_depths(control_flow);
}

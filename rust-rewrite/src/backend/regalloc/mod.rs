use std::collections::{BTreeMap, BTreeSet};

use block_depths::*;

use crate::midend::{ir::ControlFlow, symtab};

mod block_depths;

pub fn allocate_registers(_scope: &symtab::Scope, control_flow: &ControlFlow) {
    println!("Allocate registers for scope");

    let mut blocks_by_depth = BTreeMap::<usize, BTreeSet<usize>>::new();

    for (block_label, block_depth) in find_block_depths(control_flow) {
        blocks_by_depth
            .entry(block_depth)
            .or_insert(BTreeSet::new())
            .insert(block_label);
    }

    for (depth, label_set) in blocks_by_depth {
        print!("{}:", depth);
        for label in label_set {
            print!("{} ", label);
        }
        println!();
    }
}

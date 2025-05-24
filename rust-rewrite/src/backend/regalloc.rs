use std::collections::{BTreeMap, BTreeSet};

use lifetime::LifetimeSet;

use crate::midend::{self, ir::ControlFlow, symtab};

mod block_depths;
mod interference;
mod lifetime;
mod program_point;

use interference::InterferenceGraph;

pub fn allocate_registers(_scope: &symtab::Scope, control_flow: &ControlFlow) {
    println!("Allocate registers for scope");

    // let depths = find_block_depths(control_flow);

    let mut graph = InterferenceGraph::new();

    // println!("interference graph: {:?}", graph);
    for (name, others) in graph.iter() {
        println!("{}:", name);
        for other in others {
            println!("\t{}", other);
        }
    }
}

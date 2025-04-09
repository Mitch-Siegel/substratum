use std::collections::{BTreeMap, BTreeSet};

use block_depths::*;
use lifetime::{Lifetime, LifetimeSet};
use program_point::ProgramPoint;

use crate::midend::{self, ir::ControlFlow, symtab};

mod block_depths;
mod lifetime;
mod program_point;

fn record_interference_graph(
    graph: &mut BTreeMap<midend::ir::OperandName, BTreeSet<midend::ir::OperandName>>,
    lifetime_a: &midend::ir::OperandName,
    lifetime_b: &midend::ir::OperandName,
) {
    graph
        .entry(lifetime_a.clone())
        .or_default()
        .insert(lifetime_b.clone());
}

pub fn allocate_registers(_scope: &symtab::Scope, control_flow: &ControlFlow) {
    println!("Allocate registers for scope");

    // let depths = find_block_depths(control_flow);

    let mut graph = BTreeMap::<midend::ir::OperandName, BTreeSet<midend::ir::OperandName>>::new();

    for (_, block) in &control_flow.blocks {
        let block_lifetimes = LifetimeSet::from_block(block);

        for index in 0..block.statements.len() {
            let mut first_iter = block_lifetimes.lifetimes.iter();

            while let Some((first_name, first_lt)) = first_iter.next() {
                if first_lt.live_at(&index) {
                    let mut second_iter = first_iter.clone();
                    while let Some((second_name, second_lt)) = second_iter.next() {
                        if second_lt.live_at(&index) {
                            record_interference_graph(&mut graph, first_name, second_name);
                        }
                    }
                }
            }
        }
    }
}

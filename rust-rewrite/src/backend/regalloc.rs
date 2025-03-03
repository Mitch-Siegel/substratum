use std::collections::{BTreeMap, HashMap, HashSet};

use crate::midend::{ir::ControlFlow, program_point::ProgramPoint, symtab};

use super::lifetime::{Lifetime, LifetimeSet};

struct RegallocMetadata {
    lifetimes: HashMap<String, Lifetime>,
    overlaps: BTreeMap<ProgramPoint, HashSet<String>>,
    conflicts: HashMap<String, HashSet<String>>,
}

fn find_max_indices_by_depth(control_flow: &ControlFlow) -> Vec<usize> {
    let mut max_indices_per_depth = Vec::<usize>::new();

    todo!("reimplement with new controlflow data structure");
    // for block in &control_flow.blocks {
    //     for ir_line in block.statements() {
    //         let depth = ir_line.program_point().depth;
    //         while depth >= max_indices_per_depth.len() {
    //             max_indices_per_depth.push(0);
    //         }

    //         max_indices_per_depth[depth] =
    //             usize::max(max_indices_per_depth[depth], ir_line.program_point().index);
    //     }
    // }

    // for depth in 0..max_indices_per_depth.len() {
    //     println!("{}:{}", depth, max_indices_per_depth[depth])
    // }

    max_indices_per_depth
}

impl RegallocMetadata {
    // figure out all program points at which there are overlaps between lifetimes
    fn find_overlaps(&mut self, max_indices_by_depth: &Vec<usize>) {
        // for all lifetimes
        for lifetime in self.lifetimes.values() {
            // for all depths this lifetime lives at
            for depth in lifetime.start().depth..=lifetime.end().depth {
                // for every index in each depth
                for index in 0..=max_indices_by_depth[depth] {
                    let current_point = ProgramPoint::new(depth, index);
                    // if the lifetime is live at the point (depth, index), insert it into the overlaps
                    if lifetime.live_at(&current_point) {
                        if !self.overlaps.contains_key(&current_point) {
                            self.overlaps
                                .insert(current_point, HashSet::<String>::new());
                        }

                        self.overlaps
                            .get_mut(&current_point)
                            .unwrap()
                            .insert(lifetime.name().clone());
                    }
                }
            }
        }
    }

    // once overlaps have been found, figure out which overlaps are actually conflicts
    // lifetimes which live only within the same set of depths as others may not actually conflict
    fn find_conflicts(&mut self) {
        unimplemented!();
    }

    pub fn new(lifetime_set: LifetimeSet, max_indices_by_depth: &Vec<usize>) -> Self {
        let mut metadata = RegallocMetadata {
            lifetimes: lifetime_set.lifetimes,
            overlaps: BTreeMap::<ProgramPoint, HashSet<String>>::new(),
            conflicts: HashMap::<String, HashSet<String>>::new(),
        };

        metadata.find_overlaps(max_indices_by_depth);
        metadata.find_conflicts();

        metadata
    }
}

pub fn allocate_registers(scope: &symtab::Scope, control_flow: &ControlFlow) {
    println!("Allocate registers for scope");

    // let lifetimes = LifetimeSet::from_control_flow(control_flow);

    // let max_indices_by_depth = find_max_indices_by_depth(control_flow);

    // let metadata = RegallocMetadata::new(lifetimes, &max_indices_by_depth);
}

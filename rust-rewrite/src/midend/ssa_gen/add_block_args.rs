use std::collections::{BTreeSet, HashMap, HashSet};

use crate::midend::{
    idfa::{self, reaching_defs::IdfaImplementor},
    ir, symtab,
};

use super::ModifiedBlocks;

#[derive(Debug)]
struct SsaBlockArgsMetadata {
    live_vars_facts: idfa::live_vars::Facts,
    block_args: HashMap<usize, HashSet<ir::NamedOperand>>,
    modified_blocks: ModifiedBlocks,
}

impl SsaBlockArgsMetadata {
    pub fn new(control_flow: &ir::ControlFlow) -> Self {
        let analysis = idfa::LiveVars::new(control_flow);

        let mut extra_kills_by_block = HashMap::<usize, BTreeSet<ir::NamedOperand>>::new();
        for block_label in 0..control_flow.blocks.len() {
            extra_kills_by_block.insert(block_label, BTreeSet::new());
        }

        let mut block_args = HashMap::<usize, HashSet<ir::NamedOperand>>::new();
        for block in &control_flow.blocks {
            block_args.insert(block.label(), block.arguments().clone());
        }

        Self {
            live_vars_facts: analysis.take_facts(),
            modified_blocks: ModifiedBlocks::new(),
            block_args,
        }
    }
}

fn add_block_arguments_for_block(
    block: &ir::BasicBlock,
    mut metadata: Box<SsaBlockArgsMetadata>,
) -> Box<SsaBlockArgsMetadata> {
    let label = block.label();

    let mut new_block = block.clone();

    for statement in new_block.statements_mut() {
        match &mut statement.operation {
            ir::Operations::Jump(jump) => {
                let destination = &jump.destination_block;
                let target_args = metadata
                    .block_args
                    .get(destination)
                    .expect(&format!("No block args found for block {}", destination));

                for liveout in &metadata.live_vars_facts.for_label(label).out_facts {
                    let mut non_ssa = liveout.clone();
                    non_ssa.ssa_number = None;

                    let replace = match jump.block_args.get(&non_ssa) {
                        Some(existing) => {
                            existing
                                .ssa_number
                                .expect("Block argument must have SSA number")
                                < liveout
                                    .ssa_number
                                    .expect("Liveout written variable must have SSA number")
                        }
                        None => target_args.contains(&non_ssa),
                    };

                    if replace {
                        println!("{}:{}", non_ssa, liveout);
                        jump.block_args.insert(non_ssa, liveout.clone());
                    }
                }
            }
            _ => {}
        }
    }

    metadata.modified_blocks.add_block(new_block);

    metadata
}

pub fn add_block_arguments(function: &mut symtab::Function) {
    let metadata = function.control_flow.map_over_blocks_reverse_postorder(
        add_block_arguments_for_block,
        SsaBlockArgsMetadata::new(&function.control_flow),
    );
    for (label, block) in metadata.modified_blocks.take() {
        function.control_flow.blocks[label] = block;
    }
}

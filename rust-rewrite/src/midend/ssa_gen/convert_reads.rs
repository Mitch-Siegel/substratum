use std::collections::{BTreeSet, HashMap};

use crate::midend::{
    idfa::{self, reaching_defs::IdfaImplementor},
    ir, symtab,
};

use super::ModifiedBlocks;

pub struct SsaReadConversionMetadata {
    reaching_defs_facts: idfa::reaching_defs::Facts,
    extra_kills_by_block: HashMap<usize, BTreeSet<ir::NamedOperand>>,
    modified_blocks: ModifiedBlocks,
    n_changed_reads: usize,
}

impl<'a> std::fmt::Debug for SsaReadConversionMetadata {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct("SsaReadConversionMetadata")
            .field("reaching_defs_facts", &self.reaching_defs_facts)
            .finish()
    }
}

impl SsaReadConversionMetadata {
    pub fn new(control_flow: &ir::ControlFlow) -> Self {
        let analysis = idfa::ReachingDefs::new(control_flow);

        let mut extra_kills_by_block = HashMap::<usize, BTreeSet<ir::NamedOperand>>::new();
        for block_label in 0..control_flow.blocks.len() {
            extra_kills_by_block.insert(block_label, BTreeSet::new());
        }

        Self {
            reaching_defs_facts: analysis.take_facts(),
            extra_kills_by_block,
            modified_blocks: ModifiedBlocks::new(),
            n_changed_reads: 0,
        }
    }

    fn get_read_number_for_variable(
        &mut self,
        name: &ir::NamedOperand,
        block_label: usize,
    ) -> Option<usize> {
        let mut highest_ssa_number = None;
        for out_fact in &self.reaching_defs_facts.for_label(block_label).out_facts {
            if (out_fact.base_name == name.base_name)
                && (!self
                    .extra_kills_by_block
                    .get(&block_label)
                    .unwrap()
                    .contains(out_fact))
            {
                highest_ssa_number = Some(highest_ssa_number.unwrap_or(0).max(
                    out_fact.ssa_number.expect(&format!(
                        "Variable {} doesn't have ssa number to read",
                        out_fact
                    )),
                ));
            }
        }
        highest_ssa_number
    }

    pub fn assign_read_number_to_operand(
        &mut self,
        name: &mut ir::NamedOperand,
        block_label: usize,
    ) {
        let number = self.get_read_number_for_variable(name, block_label);
        if number.is_some() {
            let number = number.unwrap();
            let new_name = ir::NamedOperand {
                base_name: name.base_name.clone(),
                ssa_number: Some(number),
            };
            let replace_result = match &mut name.ssa_number {
                Some(old_number) => {
                    if (number > *old_number)
                        && (!self.extra_kills_by_block[&block_label].contains(&new_name))
                    {
                        *old_number = number;
                        true
                    } else {
                        false
                    }
                }
                None => {
                    name.ssa_number.replace(number);
                    true
                }
            };

            if replace_result {
                self.n_changed_reads += 1;

                println!("Extra kill of {}", new_name);
                self.extra_kills_by_block
                    .get_mut(&block_label)
                    .expect(&format!(
                        "No extra_kills_by_block set for label {}",
                        block_label
                    ))
                    .insert(new_name);
            }
        }
    }
}

fn convert_block_reads_to_ssa<'a>(
    block: &ir::BasicBlock,
    mut metadata: Box<SsaReadConversionMetadata>,
) -> Box<SsaReadConversionMetadata> {
    let label = block.label();

    let mut new_block = block.clone();

    for statement in new_block.statements_mut() {
        for operand in statement.read_operands_mut() {
            match operand {
                ir::Operand::Variable(name) => {
                    metadata.assign_read_number_to_operand(name, label);
                }
                ir::Operand::Temporary(name) => {
                    metadata.assign_read_number_to_operand(name, label);
                }
                ir::Operand::UnsignedDecimalConstant(_) => {}
            }
        }
    }
    metadata.modified_blocks.add_block(new_block);

    metadata
}

pub fn convert_reads_to_ssa(function: &mut symtab::Function) {
    // let mut loop_count = 0;
    // loop {
    //     let mut reaching_defs = idfa::ReachingDefs::new(&function.control_flow);

    //     for block in &function.control_flow.blocks {
    //         print!("{}:", block.label());
    //         for fact in &reaching_defs.facts().for_label(block.label()).in_facts {
    //             print!("{} ", fact);
    //         }
    //         println!();
    //     }

    //     let read_conversion_metadata = function.control_flow.map_over_blocks_by_bfs(
    //         convert_block_reads_to_ssa,
    //         SsaReadConversionMetadata::new(&function.control_flow),
    //     );
    //     if read_conversion_metadata.n_changed_reads == 0 {
    //         break;
    //     } else {
    //         for (label, block) in read_conversion_metadata.modified_blocks.take() {
    //             function.control_flow.blocks[label] = block;
    //         }
    //     };

    //     loop_count += 1;
    // }
}

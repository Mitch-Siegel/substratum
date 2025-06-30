use crate::{
    midend::{ir, symtab},
    trace,
};
use std::collections::BTreeMap;

pub fn convert_reads_to_ssa(function: &mut symtab::Function) {
    for (_, block) in function.control_flow.blocks_postorder_mut() {
        let mut highest_ssa_numbers = BTreeMap::<ir::ValueId, ir::ValueId>::new();

        for arg in &block.arguments {
            highest_ssa_numbers.insert(arg.clone().into_non_ssa(), arg.clone());
        }

        for statement in &mut block.statements {
            for read in statement.read_operand_names_mut() {
                read.ssa_number = match highest_ssa_numbers.get(read) {
                    Some(operand) => operand.ssa_number,
                    None => {
                        trace::trace!("{} has no ssa number (yet)", read);
                        None
                    }
                };
            }

            for write in statement.write_operand_names() {
                match highest_ssa_numbers.get(&write.clone().into_non_ssa()) {
                    Some(existing_number) => {
                        assert!(existing_number.ssa_number.unwrap() < write.ssa_number.unwrap());
                    }
                    None => {}
                }
                highest_ssa_numbers.insert(write.clone().into_non_ssa(), write.clone());
            }
        }
    }
}

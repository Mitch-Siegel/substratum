use std::collections::BTreeMap;

use crate::midend::{idfa::reaching_defs::IdfaImplementor, ir, symtab};

pub fn convert_reads_to_ssa(function: &mut symtab::Function) {
    for block in function.control_flow.blocks.values_mut() {
        let mut highest_ssa_numbers = BTreeMap::<ir::OperandName, ir::OperandName>::new();

        for arg in &block.arguments {
            highest_ssa_numbers.insert(arg.clone().into_non_ssa(), arg.clone());
        }

        for statement in &mut block.statements {
            for read in statement.read_operand_names_mut() {
                read.ssa_number = match highest_ssa_numbers.get(read) {
                    Some(operand) => operand.ssa_number,
                    None => None,
                };
            }

            for write in statement.write_operand_names() {
                let existing_number = highest_ssa_numbers.get(&write.clone().into_non_ssa());
                if existing_number.is_some() {
                    // sanity check that SSA numbers only ever increase within a given block
                    assert!(
                        existing_number.unwrap().ssa_number.unwrap() < write.ssa_number.unwrap()
                    );
                    highest_ssa_numbers.insert(write.clone().into_non_ssa(), write.clone());
                }
            }
        }
    }
}

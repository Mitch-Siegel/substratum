use std::collections::BTreeMap;

use crate::ir;

pub(crate) fn convert_reads_to_ssa(cf: &mut ir::ControlFlow) {
    let (blocks, values) = cf.blocks_postorder_mut_with_values();

    for block in blocks {
        let mut highest_ssa_numbers = BTreeMap::<ir::ValueId, ir::ValueId>::new();

        for arg in &block.arguments {
            let base_id = values.ssa_base_id(*arg).unwrap();
            highest_ssa_numbers.insert(base_id, *arg);
        }

        for statement in block.statements_mut() {
            for read in statement.read_value_ids_mut() {
                let base_id = values.ssa_base_id(*read).unwrap();
                if let Some(highest) = highest_ssa_numbers.get(&base_id) {
                    *read = *highest;
                } else {
                    panic!("missing write for value {base_id}");
                }
            }

            for write in statement.write_value_ids() {
                let base_id = values.ssa_base_id(write).unwrap();
                highest_ssa_numbers.insert(base_id, write);
            }
        }
    }
}

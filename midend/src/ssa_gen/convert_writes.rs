use crate::{ir, types};

fn convert_block_writes_to_ssa(
    block: &mut ir::BasicBlock,
    values: &mut ir::ValueInterner<Option<types::Syntactic>>,
) {
    // let old_args = block.arguments.clone();
    // block.arguments.clear();

    // for argument in &old_args {
    //     let new_argument = values
    //         .make_unique_ssa_for(*argument, frontend::here!())
    //         .unwrap();
    //     block.arguments.insert(new_argument);
    // }

    for statement in block.statements_mut() {
        for write in statement.write_value_ids_mut() {
            match values.value_for_id(*write).unwrap().kind {
                ir::value::ValueKind::Variable(_) | ir::value::ValueKind::Argument(_) => {
                    *write = values
                        .make_unique_ssa_for(*write, frontend::here!())
                        .unwrap();
                }
                _ => (),
            }
        }
    }
}

pub(crate) fn convert_writes_to_ssa(cf: &mut ir::ControlFlow) {
    let (blocks, values) = cf.blocks_postorder_mut_with_values();

    for block in blocks {
        convert_block_writes_to_ssa(block, values);
    }
}

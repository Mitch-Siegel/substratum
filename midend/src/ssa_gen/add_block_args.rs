use std::collections::{BTreeSet, HashMap};

use crate::{
    idfa::{self, IdfaImplementor},
    ir,
};

pub(crate) fn add_block_arguments(cf: &mut ir::ControlFlow) {
    let mut block_args = idfa::BlockArgs::new(cf).take_facts();

    loop {
        let mut args_by_block = HashMap::<usize, BTreeSet<ir::ValueId>>::new();
        for block in &mut *cf {
            block.arguments = block_args.for_label(block.label).clone().out;
            args_by_block.insert(block.label, block.arguments.clone());
        }

        for block in &mut *cf {
            for statement in block.statements_mut() {
                if let ir::Operation::Lowered(ir::lowered::Operation::Jump(jump)) =
                    &mut statement.operation
                {
                    for target_arg in args_by_block.get(&jump.destination_block).unwrap() {
                        jump.block_args.insert(*target_arg, *target_arg);
                    }
                }
            }
        }

        let new_block_args = idfa::BlockArgs::new(cf).take_facts();
        if new_block_args == block_args {
            break;
        }
        block_args = new_block_args;
    }
}

use std::collections::{BTreeSet, HashMap};

use crate::midend::{
    idfa::{self, IdfaImplementor},
    ir, symtab,
};

pub fn add_block_arguments(function: &mut symtab::Function) {
    let mut block_args = idfa::BlockArgs::new(&function.control_flow).take_facts();

    loop {
        let mut args_by_block = HashMap::<usize, BTreeSet<ir::ValueId>>::new();
        for block in &mut function.control_flow {
            block.arguments = block_args.for_label(block.label).out_facts.clone();
            args_by_block.insert(block.label, block.arguments.clone());
        }

        for block in &mut function.control_flow {
            for statement in &mut block.statements {
                match &mut statement.operation {
                    ir::Operations::Jump(jump) => {
                        for target_arg in args_by_block.get(&jump.destination_block).unwrap() {
                            jump.block_args
                                .insert(target_arg.clone(), target_arg.clone());
                        }
                    }
                    _ => {}
                }
            }
        }

        let new_block_args = idfa::BlockArgs::new(&function.control_flow).take_facts();
        if new_block_args != block_args {
            block_args = new_block_args;
        } else {
            break;
        }
    }
}

use crate::midend::idfa::block_args::IdfaImplementor;
use crate::midend::{
    idfa::{self},
    ir, symtab,
};

pub fn add_block_arguments(function: &mut symtab::Function) {
    let mut block_args = idfa::BlockArgs::new(&function.control_flow).take_facts();

    loop {
        let mut args_by_block = Vec::new();
        for label in 0..function.control_flow.blocks.len() {
            let block = &mut function.control_flow.blocks[label];
            block.arguments = block_args.for_label(block.label()).out_facts.clone();
            args_by_block.push(block.arguments.clone());
        }

        for block in &mut function.control_flow.blocks {
            for statement in block.statements_mut() {
                match &mut statement.operation {
                    ir::Operations::Jump(jump) => {
                        for target_arg in &args_by_block[jump.destination_block] {
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

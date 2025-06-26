use std::collections::{BTreeMap, BTreeSet};

use crate::backend::arch::generic::registers;
use crate::backend::regalloc::block_depths::find_block_depths;
use crate::{
    backend::*,
    midend::{self, symtab::VariableOwner},
};
use lifetime::LifetimeSet;

mod allocated_locations;
mod block_depths;
mod interference;
mod lifetime;
mod program_point;
mod regalloc_context;

pub use allocated_locations::AllocatedLocations;
pub use lifetime::*;
pub use regalloc_context::*;

pub fn heuristic<C>(lifetime: &Lifetime, context: &C) -> isize
where
    C: midend::types::TypeSizingContext,
{
    let size = lifetime.type_.size::<arch::Target, C>(context).unwrap();

    let write_weight = 10;
    let read_weight = 4;
    let size_weight = 3;

    (lifetime.n_reads as isize * read_weight) + (lifetime.n_writes as isize * write_weight)
        - (size as isize * size_weight)
}

pub fn allocate_registers<Target: arch::TargetArchitecture, C>(
    context: RegallocContext<C>,
) -> AllocatedLocations
where
    C: midend::symtab::VariableSizingContext,
{
    trace::trace!("Allocate registers for {}", context.function.name());

    let lifetimes = LifetimeSet::new(context.function, &context);

    let mut locations = AllocatedLocations::new();
    // let depths = find_block_depths(control_flow);

    let _target_registers = Target::registers();

    let mut arguments = context
        .function
        .prototype
        .arguments
        .iter()
        .rev()
        .map(|argument| lifetimes.lookup_by_variable(argument).unwrap())
        .collect::<Vec<_>>();

    let mut remaining_argument_registers = _target_registers
        .for_purpose(&arch::generic::RegisterPurpose::Argument)
        .collect::<Vec<_>>()
        .into_iter()
        .rev()
        .collect::<Vec<_>>();
    let mut arg_register_index = 0;
    while let Some(argument) = arguments.pop() {
        // still have argument registers
        let argument_size = argument
            .type_
            .size::<Target, RegallocContext<C>>(&context)
            .unwrap();
        let registers_for_argument = argument_size.div_ceil(Target::word_size());

        let argument_to_register =
            if argument.type_.is_integral(&context).unwrap() || registers_for_argument <= 2 {
                if remaining_argument_registers.len() >= registers_for_argument {
                    true
                } else {
                    false
                }
            } else {
                false
            };

        trace::trace!(
            "Argument {}: size {} would require {} registers - to register? {}",
            argument.name,
            argument_size,
            registers_for_argument,
            argument_to_register
        );

        if argument_to_register {
            if registers_for_argument == 1 {
                locations.assign_to_register(
                    argument.name.clone(),
                    remaining_argument_registers.pop().unwrap().clone(),
                );
            } else {
                let range = remaining_argument_registers
                    .drain(0..registers_for_argument)
                    .map(|register| register.clone())
                    .collect::<Vec<_>>();

                locations.assign_to_register_range(argument.name.clone(), range.as_slice());
            }
        } else {
            locations.assign_to_stack_argument(argument.name.clone(), 123);
        }
    }

    let control_flow = &context.function.control_flow;

    // println!("interference graph: {:?}", graph);

    let depths = find_block_depths(control_flow);

    println!("{:?}", depths);
    println!("{:?}", locations);
    AllocatedLocations::new()
}

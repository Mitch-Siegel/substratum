use std::collections::{BTreeMap, BTreeSet};

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
    let _lookup_result = lifetime.type_.size::<arch::Target, C>(context);

    0
}

fn registers_required_for_argument<'a, C, Target: arch::TargetArchitecture>(
    argument_name: &midend::ir::OperandName,
    context: &RegallocContext<'a, C>,
) -> Option<usize>
where
    C: midend::symtab::VariableSizingContext,
{
    let lookup_result = context
        .lookup_variable_by_name(&argument_name.base_name)
        .expect("Function argument missing from scope stack");

    Target::registers_required_for_argument(context, lookup_result.type_())
}

pub fn allocate_registers<Target: arch::TargetArchitecture, C>(
    context: RegallocContext<C>,
) -> AllocatedLocations
where
    C: midend::symtab::VariableSizingContext,
{
    println!("Allocate registers for {}", context.function.name());

    let lifetimes = LifetimeSet::new(context.function, &context);
    // let depths = find_block_depths(control_flow);

    let _target_registers = Target::registers();

    let mut arguments = context
        .function
        .prototype
        .arguments
        .iter()
        .map(|argument| lifetimes.lookup_by_variable(argument).unwrap())
        .collect::<BTreeSet<_>>();

    let mut _stack_arguments: BTreeSet<&lifetime::Lifetime> = BTreeSet::new();

    let argument_heuristics = arguments
        .iter()
        .map(|argument| {
            let heuristic = heuristic(argument, &context);

            (*argument, heuristic)
        })
        .collect::<BTreeMap<&Lifetime, isize>>();

    if arguments.len()
        > *_target_registers
            .counts_by_purpose
            .get(&arch::generic::registers::RegisterPurpose::Argument)
            .unwrap()
    {
        let mut reverse_map = argument_heuristics
            .iter()
            .map(|(k, v)| (*v, *k))
            .collect::<BTreeMap<isize, &Lifetime>>();

        let to_spill = reverse_map.first_entry().unwrap().get().to_owned();

        arguments.remove(to_spill);
        _stack_arguments.insert(to_spill);

        println!("need to spill - selected lifetime {}", to_spill.name);
    }

    let control_flow = &context.function.control_flow;

    // println!("interference graph: {:?}", graph);

    let depths = find_block_depths(control_flow);

    println!("{:?}", depths);
    AllocatedLocations::new()
}

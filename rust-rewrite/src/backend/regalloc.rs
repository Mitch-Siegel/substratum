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
use regalloc_context::RegallocContext;

pub fn heuristic(lifetime: &midend::ir::OperandName, scope: &midend::symtab::Scope) -> isize {
    let _lookup_result = scope.lookup_variable_by_name(&lifetime.base_name);

    0
}

fn registers_required_for_argument<'a, C, Target: arch::TargetArchitecture>(
    argument_name: &midend::ir::OperandName,
    context: &RegallocContext<'a, C>,
) -> Option<usize>
where
    C: midend::types::TypeSizingContext,
{
    let lookup_result = context
        .lookup_variable_by_name(&argument_name.base_name)
        .expect("Function argument missing from scope stack");

    Target::registers_required_for_argument(context, lookup_result.type_())
}

pub fn allocate_registers<Target: arch::TargetArchitecture>(
    function: &midend::symtab::Function,
) -> AllocatedLocations {
    println!("Allocate registers for {}", function.name());

    let lifetimes = LifetimeSet::from_control_flow(&function.control_flow);
    // let depths = find_block_depths(control_flow);

    let _target_registers = Target::registers();

    let mut _arguments = function
        .prototype
        .arguments
        .iter()
        .map(|argument| lifetimes.lookup_by_variable(argument).unwrap())
        .collect::<Vec<_>>();

    let mut _stack_arguments: Vec<&lifetime::Lifetime> = Vec::new();

    let control_flow = &function.control_flow;

    // println!("interference graph: {:?}", graph);

    let depths = find_block_depths(control_flow);

    println!("{:?}", depths);
    AllocatedLocations::new()
}

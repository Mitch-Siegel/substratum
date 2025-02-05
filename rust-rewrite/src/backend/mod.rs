mod lifetime;

use lifetime::LifetimeSet;

use crate::midend::*;
use crate::midend::control_flow::*;

pub fn allocate_registers(scope: &Scope, control_flow: &ControlFlow) {
    println!("Allocate registers for scope");
    let lifetimes = LifetimeSet::from_control_flow(control_flow);

    lifetimes.print_numerical();
}

pub fn generate_code_for_function(function: Function) {
    println!("generate code for {}", function.prototype());
    allocate_registers(function.scope(), function.control_flow());
}

pub fn generate_code(symbol_table: SymbolTable) {
    for (_, member) in symbol_table.functions() {
        match member {
            FunctionOrPrototype::Function(f) => generate_code_for_function(f),
            FunctionOrPrototype::Prototype(p) => println!("{}", p),
        }
    }
}


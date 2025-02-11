mod lifetime;
mod regalloc;

use std::collections::{BTreeMap, HashMap, HashSet};

use lifetime::LifetimeSet;

use crate::midend::control_flow::*;
use crate::midend::program_point::ProgramPoint;
use crate::midend::*;


pub fn generate_code_for_function(function: Function) {
    println!("generate code for {}", function.prototype());
    regalloc::allocate_registers(function.scope(), function.control_flow());
}

pub fn generate_code(symbol_table: SymbolTable) {
    for (_, member) in symbol_table.functions() {
        match member {
            FunctionOrPrototype::Function(f) => generate_code_for_function(f),
            FunctionOrPrototype::Prototype(p) => println!("{}", p),
        }
    }
}

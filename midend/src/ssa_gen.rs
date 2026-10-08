use crate::symtab;

mod add_block_args;
mod convert_reads;
mod convert_writes;

use add_block_args::add_block_arguments;
use convert_reads::convert_reads_to_ssa;
use convert_writes::convert_writes_to_ssa;

// TODO: fix mutability here (hide control flow and require &mut?)
#[allow(clippy::needless_pass_by_ref_mut)]
fn convert_function_to_ssa(function: &mut symtab::values::Function) {
    let symtab::values::Function {
        control_flow,
        prototype: _,
    } = &function;

    let Some(cf) = &mut *control_flow.borrow_mut() else {
        return;
    };

    add_block_arguments(cf);
    convert_writes_to_ssa(cf);
    convert_reads_to_ssa(cf);
}

fn on_symbol(_path: &symtab::RawPath, symbol: &mut symtab::SymbolDef, _: &mut ()) {
    if let symtab::SymbolDef::Value(symtab::Value::Function(f)) = symbol {
        convert_function_to_ssa(f);
    }
}

pub(crate) fn convert_functions_to_ssa(symtab: &mut symtab::SymbolTable) {
    symtab::MutVisitor::visit(symtab, on_symbol, ());
}

#[allow(unused)]
fn remove_ssa(_function: &mut symtab::values::Function) {
    unimplemented!()
}

use crate::frontend;
use crate::trace;

mod idfa;
pub mod ir;
pub mod linearizer;
mod optimization;
//mod ssa_gen;
pub mod symtab;
pub mod types;

pub fn symbol_table_from_modules(modules: Vec<frontend::ast::ModuleTree>) -> symtab::SymbolTable {
    let _ = trace::span_auto!(trace::Level::DEBUG, "Generate symbol table from AST");

    tracing::debug!("Linearize");
    let symtab = linearizer::linearize(modules);

    //tracing::debug!("collapse scopes");
    //symtab.collapse_scopes();

    tracing::debug!("convert IR to SSA");
    //ssa_gen::convert_functions_to_ssa(&mut symtab);

    // optimization::optimize_functions(&mut symtab.functions);
    //

    tracing::debug!("convert IR back from SSA");
    //ssa_gen::remove_ssa_from_functions(&mut symtab);

    *symtab
}

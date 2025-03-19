use symtab::SymbolTable;

use crate::frontend;

mod idfa;
pub mod ir;
pub mod linearizer;
mod optimization;
pub mod program_point;
mod ssa_gen;
pub mod symtab;
pub mod types;

pub fn symbol_table_from_program(
    program: Vec<frontend::ast::TranslationUnitTree>,
) -> symtab::SymbolTable {
    let mut symtab = SymbolTable::new();
    linearizer::linearize(&mut symtab, program);

    symtab.functions = ssa_gen::convert_functions_to_ssa(symtab.functions);

    optimization::optimize_functions(&mut symtab.functions);

    symtab
}

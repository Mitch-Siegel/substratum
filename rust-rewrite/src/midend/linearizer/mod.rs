use treewalk::TableWalk;

use crate::frontend::{ast::TranslationUnitTree, *};

use super::symtab::SymbolTable;

mod treewalk;
pub mod walkcontext;

pub fn linearize(symtab: &mut SymbolTable, program: Vec<TranslationUnitTree>) {
    for translation_unit in program {
        translation_unit.walk(symtab);
    }
}

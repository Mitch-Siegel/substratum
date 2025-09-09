use crate::midend::*;

pub fn lower_symtab(_symtab: &mut symtab::SymbolTable) {}

fn assert_symbol_lowered(def_path: &symtab::DefPath, symbol: &symtab::SymbolDef, _: &mut ()) {
    match symbol {
        symtab::SymbolDef::Function(function) => {
            assert!(
                function.is_fully_lowered(),
                "Function '{}' (with defpath '{}' is not fully lowered",
                function.name(),
                def_path,
            );
        }
        _ => (),
    }
}

pub fn assert_lowered(symtab: &symtab::SymbolTable) {
    symtab::symtab_visitor::SymtabVisitor::visit(symtab, assert_symbol_lowered, ());
}

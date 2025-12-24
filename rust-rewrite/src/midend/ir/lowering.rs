use crate::midend::*;

mod function;
use function::lower_function;

#[derive(Debug, Default)]
struct FunctionsToLower(Vec<symtab::DefPath>);

fn check_symbol_for_lowering(
    def_path: &symtab::DefPath,
    symbol: &symtab::SymbolDef,
    to_lower: &mut FunctionsToLower,
) {
    match symbol {
        symtab::SymbolDef::Value(symtab::Value::Function(_)) => {
            to_lower.0.push(def_path.clone());
        }
        _ => (),
    }
}

pub fn lower_symtab(mut symtab: Box<symtab::SymbolTable>) -> Box<symtab::SymbolTable> {
    let functions_to_lower = symtab::Visitor::visit(symtab.as_ref(), check_symbol_for_lowering);

    for to_lower in functions_to_lower.0 {
        symtab = lower_function(to_lower, symtab);
    }

    symtab
}

fn assert_symbol_lowered(def_path: &symtab::DefPath, symbol: &symtab::SymbolDef, _: &mut ()) {
    match symbol {
        symtab::SymbolDef::Value(symtab::Value::Function(function)) => {
            assert!(
                function.is_lowered(),
                "Function '{}' (with defpath '{}' is not fully lowered",
                function.name(),
                def_path,
            );
        }
        _ => (),
    }
}

pub fn assert_lowered(symtab: &symtab::SymbolTable) {
    symtab::Visitor::visit(symtab, assert_symbol_lowered);
}

use crate::midend::symtab;

mod function;
use function::lower_function;

#[derive(Debug, Default)]
struct FunctionsToLower(Vec<symtab::RawPath>);

fn check_symbol_for_lowering(
    def_path: &symtab::RawPath,
    symbol: &symtab::SymbolDef,
    to_lower: &mut FunctionsToLower,
) {
    if let symtab::SymbolDef::Value(symtab::Value::Function(_)) = symbol {
        to_lower.0.push(def_path.clone());
    }
}

pub(crate) fn lower_symtab(mut symtab: symtab::SymbolTable) -> symtab::SymbolTable {
    let functions_to_lower = symtab::Visitor::visit(&symtab, check_symbol_for_lowering);

    for to_lower in functions_to_lower.0 {
        symtab = lower_function(&to_lower, symtab);
    }

    symtab
}

fn assert_symbol_lowered(def_path: &symtab::RawPath, symbol: &symtab::SymbolDef, _: &mut ()) {
    if let symtab::SymbolDef::Value(symtab::Value::Function(function)) = symbol {
        assert!(
            function.is_lowered(),
            "Function '{}' (with defpath '{}' is not fully lowered",
            function.name(),
            def_path,
        );
    }
}

pub(crate) fn assert_lowered(symtab: &symtab::SymbolTable) {
    symtab::Visitor::visit(symtab, assert_symbol_lowered);
}

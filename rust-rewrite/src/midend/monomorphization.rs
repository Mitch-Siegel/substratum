use crate::midend::*;

fn collect_generics(def_path: &symtab::DefPath, symbol: &symtab::SymbolDef, _ctx: &mut ()) {
    match symbol {
        symtab::SymbolDef::Variable(v) => {
            if let Some(ty) = v.type_() {
                println!("{}- {}", def_path, ty)
            }
        }
        _ => (),
    }
}

pub fn monomorphize_generics(symtab: &mut symtab::SymbolTable) {
    let _ = symtab::Visitor::visit(symtab, collect_generics);
}

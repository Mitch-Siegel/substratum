use crate::midend::symtab::*;

pub struct MutVisitor {}

#[allow(dead_code)]
impl MutVisitor {
    pub fn visit<C>(
        symtab: &mut SymbolTable,
        on_symbol: fn(&DefPath, &mut SymbolDef, &mut C),
        mut data: C,
    ) -> C {
        for (path, def) in symtab.defs_mut() {
            on_symbol(path, def, &mut data);
        }

        data
    }
}

pub struct Visitor {}

impl Visitor {
    pub fn visit<C>(symtab: &SymbolTable, on_symbol: fn(&DefPath, &SymbolDef, &mut C)) -> C
    where
        C: Default,
    {
        Self::visit_with_starting_data(symtab, on_symbol, C::default())
    }

    pub fn visit_with_starting_data<C>(
        symtab: &SymbolTable,
        on_symbol: fn(&DefPath, &SymbolDef, &mut C),
        mut data: C,
    ) -> C {
        for (path, def) in symtab.defs() {
            on_symbol(path, def, &mut data);
        }

        data
    }
}

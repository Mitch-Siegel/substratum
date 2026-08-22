use crate::symtab::{RawPath, SymbolDef, SymbolTable};

pub(crate) struct MutVisitor {}

#[allow(dead_code)]
impl MutVisitor {
    pub(crate) fn visit<C>(
        symtab: &mut SymbolTable,
        on_symbol: fn(&RawPath, &mut SymbolDef, &mut C),
        mut data: C,
    ) -> C {
        for (path, def) in symtab.defs_mut() {
            on_symbol(path, def, &mut data);
        }

        data
    }
}

pub(crate) struct Visitor {}

impl Visitor {
    pub(crate) fn visit<C>(symtab: &SymbolTable, on_symbol: fn(&RawPath, &SymbolDef, &mut C)) -> C
    where
        C: Default,
    {
        Self::visit_with_starting_data(symtab, on_symbol, C::default())
    }

    pub(crate) fn visit_with_starting_data<C>(
        symtab: &SymbolTable,
        on_symbol: fn(&RawPath, &SymbolDef, &mut C),
        mut data: C,
    ) -> C {
        for (path, def) in symtab.defs() {
            on_symbol(path, def, &mut data);
        }

        data
    }
}

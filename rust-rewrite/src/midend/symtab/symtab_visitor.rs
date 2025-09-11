use crate::midend::symtab::*;

pub struct MutBasicBlockVisitor<C> {
    data: C,
    on_block: fn(&mut ir::BasicBlock, &mut C),
}

impl<C> MutBasicBlockVisitor<C> {}

pub struct MutSymtabVisitor<C> {
    data: C,
}

impl<C> MutSymtabVisitor<C> {
    pub fn visit(
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

pub struct SymtabVisitor<'a, C> {
    data: &'a C,
}

impl<'a, C> SymtabVisitor<'a, C>
where
    C: Default,
{
    pub fn visit(symtab: &SymbolTable, on_symbol: fn(&DefPath, &SymbolDef, &mut C)) -> C {
        Self::visit_with_starting_data(symtab, on_symbol, C::default())
    }

    pub fn visit_with_starting_data(
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

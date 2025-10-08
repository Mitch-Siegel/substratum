use crate::midend::treewalk::*;

pub struct CollectCtx {
    symtab: Box<symtab::SymbolTable>,
    definition_path: symtab::DefPath,
}

impl CollectCtx {
    pub fn new(symtab: Box<symtab::SymbolTable>) -> Self {
        Self {
            symtab,
            definition_path: symtab::DefPath::empty(),
        }
    }

    pub fn take(self) -> Box<symtab::SymbolTable> {
        if !self.definition_path.is_empty() {
            panic!("symbol collection context defpath not empty");
        }
        self.symtab
    }

    pub fn declare(
        &mut self,
        symbol_component: symtab::DefPathComponent,
    ) -> Result<symtab::DefPath, symtab::SymbolError> {
        self.symtab.declare(
            self.definition_path
                .clone()
                .with_component(symbol_component)?,
        )
    }

    pub fn push_def_path(
        &mut self,
        component: symtab::DefPathComponent,
    ) -> Result<(), symtab::SymbolError> {
        self.definition_path.push(component)
    }

    pub fn pop_def_path(
        &mut self,
        expect: symtab::DefPathComponent,
    ) -> Result<(), (symtab::DefPathComponent, symtab::DefPathComponent)> {
        let popped = self.definition_path.pop().unwrap();

        if popped == expect {
            Ok(())
        } else {
            Err((popped, expect))
        }
    }
}

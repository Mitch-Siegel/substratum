use crate::midend::{symtab::DefPath, treewalk::*};

pub struct CollectCtx {
    symtab: Box<symtab::SymbolTable>,
    definition_path: symtab::DefPath,
}

impl CollectCtx {
    pub fn new(symtab: Box<symtab::SymbolTable>, definition_path: symtab::DefPath) -> Self {
        Self {
            symtab,
            definition_path,
        }
    }

    pub fn take(self) -> (Box<symtab::SymbolTable>, symtab::DefPath) {
        (self.symtab, self.definition_path)
    }

    pub fn def_path(&self) -> &DefPath {
        &self.definition_path
    }

    pub fn declare(
        &mut self,
        symbol_component: symtab::PathSegment,
    ) -> Result<symtab::DefPath, symtab::SymbolError> {
        unimplemented!();
        /*
        self.symtab.declare(
            self.definition_path
                .clone()
                .with_component(symbol_component)?,
        )
        */
    }

    pub fn push_def_path(
        &mut self,
        component: symtab::PathSegment,
    ) -> Result<(), symtab::SymbolError> {
        unimplemented!();
        //self.definition_path.push(component)
    }

    pub fn pop_def_path(
        &mut self,
        expect: symtab::PathSegment,
    ) -> Result<(), (symtab::PathSegment, symtab::PathSegment)> {
        unimplemented!();
        /*
        let popped = self.definition_path.pop().unwrap();

        if popped == expect {
            Ok(())
        } else {
            Err((popped, expect))
        }
        */
    }

    pub fn declare_variable(
        &mut self,
        name: String,
    ) -> Result<symtab::DefPath, symtab::SymbolError> {
        unimplemented!();
        /*
        self.symtab.declare(
            self.definition_path
                .clone()
                .with_component(symtab::DefPathComponent::Variable(name))
                .unwrap(),
        )
        */
    }
}

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

    pub fn lookup_decl(
        &self,
        symbol: symtab::DefPathComponent,
    ) -> Result<symtab::DefPath, symtab::SymbolError> {
        self.symtab.lookup_decl(&self.definition_path, &symbol)
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

    pub fn declare_variable(
        &mut self,
        name: String,
    ) -> Result<symtab::DefPath, symtab::SymbolError> {
        self.symtab.declare(
            self.definition_path
                .clone()
                .with_component(symtab::DefPathComponent::Variable(name))
                .unwrap(),
        )
    }

    pub fn new_subscope(&mut self) -> Result<usize, symtab::SymbolError> {
        let children_of_current = self.symtab.children(&self.definition_path);

        let next_subscope_idx = children_of_current
            .into_iter()
            .filter(|child_path| match child_path.last() {
                symtab::DefPathComponent::Scope(_) => true,
                _ => false,
            })
            .count();

        let next_subscope_component =
            symtab::DefPathComponent::Scope(symtab::ScopeIndex::new(next_subscope_idx));
        self.push_def_path(next_subscope_component)?;
        Ok(next_subscope_idx)
    }

    pub fn finish_subscope(
        &mut self,
        subscope_idx: usize,
    ) -> Result<(), (symtab::DefPathComponent, symtab::DefPathComponent)> {
        self.pop_def_path(symtab::DefPathComponent::Scope(symtab::ScopeIndex::new(
            subscope_idx,
        )))
    }
}

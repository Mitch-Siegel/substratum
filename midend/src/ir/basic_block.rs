use std::collections::BTreeSet;

use crate::{
    ir::{self, IrLine, ValueId},
    symtab,
};

#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) struct BasicBlock {
    pub label: usize,
    def_path: symtab::ScopePath,
    statements: Vec<IrLine>,
    pub arguments: BTreeSet<ValueId>,
}

impl BasicBlock {
    pub(crate) fn new(label: usize, def_path: symtab::ScopePath) -> Self {
        Self {
            label,
            def_path,
            statements: Vec::new(),
            arguments: BTreeSet::new(),
        }
    }

    pub(crate) fn def_path(&self) -> &symtab::ScopePath {
        &self.def_path
    }

    /// split the block at statement with specified index, returning vec of that statement and any
    /// following it
    #[allow(unused)]
    pub(crate) fn split_at(&mut self, idx: usize) -> Vec<IrLine> {
        self.statements.split_off(idx)
    }

    pub(crate) fn push(&mut self, line: IrLine) {
        self.statements.push(line);
    }

    #[allow(unused)]
    pub(crate) fn append(&mut self, others: &mut Vec<IrLine>) {
        self.statements.append(others);
    }

    #[allow(unused)]
    pub(crate) fn statements(&self) -> impl Iterator<Item = &IrLine> {
        self.statements.iter()
    }

    pub(crate) fn statements_mut(&mut self) -> impl Iterator<Item = &mut IrLine> {
        self.statements.iter_mut()
    }

    pub(crate) fn len(&self) -> usize {
        self.statements.len()
    }
}

impl<'a> IntoIterator for &'a BasicBlock {
    type Item = &'a ir::IrLine;
    type IntoIter = std::slice::Iter<'a, ir::IrLine>;
    fn into_iter(self) -> Self::IntoIter {
        self.statements.iter()
    }
}

impl<'a> IntoIterator for &'a mut BasicBlock {
    type Item = &'a mut ir::IrLine;
    type IntoIter = std::slice::IterMut<'a, ir::IrLine>;
    fn into_iter(self) -> Self::IntoIter {
        self.statements.iter_mut()
    }
}

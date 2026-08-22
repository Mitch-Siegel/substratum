use std::collections::BTreeSet;

use crate::{
    ir,
    ir::{IrLine, OperandTypeInference, TypeInferenceContext, ValueId},
    symtab,
};

#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) struct BasicBlock {
    pub label: usize,
    def_path: symtab::ScopePath,
    statements: Vec<IrLine>,
    // lines which may not have had any type propagation done on their ValueIds
    unpropagated_lines: BTreeSet<usize>,
    pub arguments: BTreeSet<ValueId>,
}

impl BasicBlock {
    pub(crate) fn new(label: usize, def_path: symtab::ScopePath) -> Self {
        Self {
            label,
            def_path,
            statements: Vec::new(),
            unpropagated_lines: BTreeSet::new(),
            arguments: BTreeSet::new(),
        }
    }

    pub(crate) fn with_statements(
        label: usize,
        def_path: symtab::ScopePath,
        statements: Vec<ir::IrLine>,
    ) -> Self {
        let unpropagated_lines: BTreeSet<usize> = (0..statements.len()).collect();
        Self {
            label,
            def_path,
            statements,
            unpropagated_lines,
            arguments: BTreeSet::new(),
        }
    }

    pub(crate) fn def_path(&self) -> &symtab::ScopePath {
        &self.def_path
    }

    /// split the block at statement with specified index, returning vec of that statement and any
    /// following it
    pub(crate) fn split_at(&mut self, idx: usize) -> Vec<IrLine> {
        for no_longer_unpropagated in idx..self.statements.len() {
            self.unpropagated_lines.remove(&no_longer_unpropagated);
        }
        self.statements.split_off(idx)
    }

    pub(crate) fn push(&mut self, line: IrLine) {
        self.statements.push(line);
    }

    pub(crate) fn append(&mut self, others: &mut Vec<IrLine>) {
        self.statements.append(others);
    }

    pub(crate) fn statements(&self) -> impl Iterator<Item = &IrLine> {
        self.statements.iter()
    }
}

impl OperandTypeInference for BasicBlock {
    /// do type inference on the block, returning whether any line in the block still has un-inferred types
    fn infer_types(&mut self, ctx: &TypeInferenceContext) -> bool {
        self.unpropagated_lines = self
            .unpropagated_lines
            .iter()
            .filter(|line_idx| self.statements[**line_idx].infer_types(ctx))
            .copied()
            .collect();
        self.unpropagated_lines.is_empty()
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

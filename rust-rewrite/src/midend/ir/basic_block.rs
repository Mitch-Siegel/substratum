use crate::midend::{ir::*, *};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BasicBlock {
    pub label: usize,
    def_path: symtab::DefPath,
    statements: Vec<IrLine>,
    // lines which may not have had any type propagation done on their ValueIds
    unpropagated_lines: BTreeSet<usize>,
    pub arguments: BTreeSet<ValueId>,
}

impl BasicBlock {
    pub fn new(label: usize, def_path: symtab::DefPath) -> Self {
        BasicBlock {
            label,
            def_path,
            statements: Vec::new(),
            unpropagated_lines: BTreeSet::new(),
            arguments: BTreeSet::new(),
        }
    }

    pub fn with_statements(
        label: usize,
        def_path: symtab::DefPath,
        statements: Vec<ir::IrLine>,
    ) -> Self {
        let unpropagated_lines: BTreeSet<usize> = (0..statements.len()).into_iter().collect();
        Self {
            label,
            def_path,
            statements,
            unpropagated_lines,
            arguments: BTreeSet::new(),
        }
    }

    pub fn def_path(&self) -> &symtab::DefPath {
        &self.def_path
    }

    /// split the block at statement with specified index, returning vec of that statement and any
    /// following it
    pub fn split_at(&mut self, idx: usize) -> Vec<IrLine> {
        for no_longer_unpropagated in idx..self.statements.len() {
            self.unpropagated_lines.remove(&no_longer_unpropagated);
        }
        self.statements.split_off(idx)
    }

    pub fn push(&mut self, line: IrLine) {
        self.statements.push(line)
    }

    pub fn append(&mut self, others: &mut Vec<IrLine>) {
        self.statements.append(others)
    }

    pub fn statements(&self) -> impl Iterator<Item = &IrLine> {
        self.statements.iter()
    }
}

impl OperandTypeInference for BasicBlock {
    fn infer_types(&mut self, ctx: &TypeInferenceContext) -> bool {
        self.unpropagated_lines = self
            .unpropagated_lines
            .iter()
            .map(
                |line_idx| match self.statements[*line_idx].infer_types(ctx) {
                    true => None,
                    false => Some(line_idx),
                },
            )
            .flatten()
            .cloned()
            .collect();
        self.unpropagated_lines.len() == 0
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

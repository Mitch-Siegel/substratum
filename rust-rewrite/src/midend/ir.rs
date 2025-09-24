pub mod block_manager;
pub mod control_flow;
pub mod lowered;
pub mod lowering;
#[cfg(test)]
mod tests;
pub mod unlowered;
pub mod value;

use std::collections::BTreeSet;
use std::fmt::Display;

use crate::{frontend::sourceloc::SourceLoc, midend::*};
use serde::Serialize;

pub use block_manager::BlockManager;
pub use control_flow::ControlFlow;
pub use value::*;

#[derive(Debug, PartialEq, Eq, Clone)]
pub enum Operation {
    Lowered(lowered::Operation),
    Unlowered(unlowered::Operation),
}

impl OperandTypePropagation for Operation {
    fn propagate_types(&self, ctx: &TypePropagationContext) -> bool {
        match self {
            Self::Lowered(l) => l.propagate_types(ctx),
            Self::Unlowered(ul) => ul.propagate_types(ctx),
        }
    }
}

impl Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Lowered(lowered) => write!(f, "{}", lowered),
            Self::Unlowered(unlowered) => write!(f, "{}", unlowered),
        }
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct IrLine {
    pub loc: SourceLoc,
    pub operation: Operation,
}

impl Display for IrLine {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.operation)
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BasicBlock {
    pub label: usize,
    statements: Vec<IrLine>,
    // lines which may not have had any type propagation done on their ValueIds
    unpropagated_lines: BTreeSet<usize>,
    pub arguments: BTreeSet<ValueId>,
}

impl BasicBlock {
    pub fn new(label: usize) -> Self {
        BasicBlock {
            label,
            statements: Vec::new(),
            unpropagated_lines: BTreeSet::new(),
            arguments: BTreeSet::new(),
        }
    }

    pub fn with_statements(label: usize, statements: Vec<ir::IrLine>) -> Self {
        let unpropagated_lines: BTreeSet<usize> = (0..statements.len()).into_iter().collect();
        Self {
            label,
            statements,
            unpropagated_lines,
            arguments: BTreeSet::new(),
        }
    }

    /// split the block at statement with specified index, returning vec of that statement and any
    /// following it
    pub fn split_at(&mut self, idx: usize) -> Vec<IrLine> {
        for no_longer_unpropagated in idx..self.statements.len() {
            self.unpropagated_lines.remove(&no_longer_unpropagated);
        }
        self.statements.split_off(idx)
    }

    pub fn append(&mut self, line: IrLine) {
        self.statements.push(line)
    }

    pub fn statements(&self) -> impl Iterator<Item = &IrLine> {
        self.statements.iter()
    }

    pub fn propagate_types(
        &mut self,
        symtab: Box<symtab::SymbolTable>,
        values: &mut ValueInterner,
    ) -> Box<symtab::SymbolTable> {
        // TODO: make basic blocks own their own def path
        let ctx = TypePropagationContext::new(symtab, values, symtab::DefPath::empty());
        while self.unpropagated_lines.len() > 0 {
            let idx_to_propagate = self.unpropagated_lines.pop_first().unwrap();
            self.statements[idx_to_propagate].propagate_types(&ctx);
        }
        ctx.take()
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

impl IrLine {
    pub fn is_lowered(&self) -> bool {
        match self.operation {
            Operation::Lowered(_) => true,
            Operation::Unlowered(_) => false,
        }
    }
}

impl OperandTypePropagation for IrLine {
    fn propagate_types(&self, ctx: &TypePropagationContext) -> bool {
        self.operation.propagate_types(ctx)
    }
}

// IrLine constructors
impl IrLine {
    fn new_lowered(loc: SourceLoc, operation: lowered::Operation) -> Self {
        IrLine {
            loc: loc,
            operation: Operation::Lowered(operation),
        }
    }
    fn new_unlowered(loc: SourceLoc, operation: unlowered::Operation) -> Self {
        IrLine {
            loc: loc,
            operation: Operation::Unlowered(operation),
        }
    }

    pub fn new_assignment(loc: SourceLoc, destination: ValueId, source: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_assignment(destination, source))
    }

    pub fn new_binary_arithmetic_expression(
        loc: SourceLoc,
        destination: ValueId,
        operands: lowered::operands::BinaryArithmeticOperands,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_binary_arithmetic_expression(destination, operands),
        )
    }

    pub fn new_binary_comparison_expression(
        loc: SourceLoc,
        destination: ValueId,
        operands: lowered::operands::BinaryComparisonOperands,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_binary_comparison_expression(destination, operands),
        )
    }

    pub fn new_jump(
        loc: SourceLoc,
        destination_block: usize,
        condition: lowered::operands::JumpCondition,
    ) -> Self {
        Self::new_lowered(loc, lowered::new_jump(destination_block, condition))
    }

    pub fn new_function_call(
        loc: SourceLoc,
        name: String,
        arguments: lowered::operands::OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_function_call(name, arguments, return_value_to),
        )
    }

    pub fn new_method_call(
        loc: SourceLoc,
        receiver: ValueId,
        name: String,
        arguments: lowered::operands::OrderedArgumentList,
        return_value_to: ValueId,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_method_call(receiver, name, arguments, Some(return_value_to)),
        )
    }

    pub fn new_compute_field_address(
        loc: SourceLoc,
        receiver: ValueId,
        field_offset: usize,
        destination: ValueId,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::new_compute_field_address(receiver, field_offset, destination),
        )
    }

    pub fn new_load(loc: SourceLoc, pointer: ValueId, destination: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_load(pointer, destination))
    }

    pub fn new_store(loc: SourceLoc, source: ValueId, pointer: ValueId) -> Self {
        Self::new_lowered(loc, lowered::new_store(source, pointer))
    }

    //
    // unlowered IR constructors
    //
    pub fn new_match(
        loc: SourceLoc,
        def_path: symtab::DefPath,
        scrutinee: ValueId,
        arms: Vec<unlowered::operands::MatchArm>,
    ) -> Self {
        Self::new_unlowered(loc, unlowered::new_match(def_path, scrutinee, arms))
    }

    pub fn new_discriminant(
        loc: SourceLoc,
        def_path: symtab::DefPath,
        enum_value: ValueId,
        destination: ValueId,
    ) -> Self {
        Self::new_unlowered(
            loc,
            unlowered::new_discriminant(def_path, enum_value, destination),
        )
    }

    pub fn new_get_field_pointer(
        loc: SourceLoc,
        def_path: symtab::DefPath,
        receiver: ValueId,
        field_name: String,
        destination: ValueId,
    ) -> Self {
        Self::new_unlowered(
            loc,
            unlowered::new_get_field_pointer(def_path, receiver, field_name, destination),
        )
    }
    //
    // general utility functions
    //
    pub fn read_value_ids(&self) -> Vec<ValueId> {
        match &self.operation {
            Operation::Lowered(lowered) => lowered.read_value_ids(),
            Operation::Unlowered(unlowered) => unlowered.read_value_ids(),
        }
    }

    pub fn write_value_ids(&self) -> Vec<ValueId> {
        match &self.operation {
            Operation::Lowered(lowered) => lowered.write_value_ids(),
            Operation::Unlowered(unlowered) => unlowered.write_value_ids(),
        }
    }
}

struct TypePropagationContext<'a> {
    symtab: Box<symtab::SymbolTable>,
    values: &'a mut ValueInterner,
    def_path: symtab::DefPath,
}

impl<'a> TypePropagationContext<'a> {
    pub fn new(
        symtab: Box<symtab::SymbolTable>,
        values: &'a mut ValueInterner,
        def_path: symtab::DefPath,
    ) -> Self {
        Self {
            symtab,
            values,
            def_path,
        }
    }

    pub fn take(self) -> Box<symtab::SymbolTable> {
        self.symtab
    }
}

enum TypePropagationError {
    ValueError(value::ValueError),
}

impl From<ValueError> for TypePropagationError {
    fn from(ve: ValueError) -> Self {
        Self::ValueError(ve)
    }
}

impl<'a> TypePropagationContext<'a> {
    pub fn type_for_value(&self, value_id: &ValueId) -> Option<types::Semantic> {
        match self.values.semantic_for_id(value_id) {
            Ok(ty) => Some(ty),
            _ => None,
        }
    }

    pub fn assign_type_to_value(
        &mut self,
        value_id: &ValueId,
        ty: types::Semantic,
    ) -> Result<(), TypePropagationError> {
        let value = self.values.value_mut_for_id(value_id)?;
        value.set_type(ty)?;
        Ok(())
    }
}

#[enum_delegate::register]
pub trait OperandTypePropagation {
    fn propagate_types(&self, ctx: &TypePropagationContext) -> bool;
}

pub trait IrOperation: OperandTypePropagation {
    fn read_value_ids(&self) -> Vec<ValueId>;
    fn write_value_ids(&self) -> Vec<ValueId>;
}

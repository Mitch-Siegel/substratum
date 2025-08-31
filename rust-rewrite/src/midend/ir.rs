pub mod control_flow;
pub mod lowered;
#[cfg(test)]
mod tests;
pub mod unlowered;
pub mod value;

use std::collections::BTreeSet;
use std::fmt::Display;

use crate::frontend::sourceloc::SourceLoc;
use crate::midend::{ir, symtab};
use serde::Serialize;

pub use control_flow::ControlFlow;
pub use value::*;

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum Operation {
    Lowered(lowered::Operation),
    Unlowered(unlowered::Operation),
}

impl Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Lowered(lowered) => write!(f, "{}", lowered),
            Self::Unlowered(unlowered) => write!(f, "{}", unlowered),
        }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct IrLine {
    pub loc: SourceLoc,
    pub operation: Operation,
}

impl Display for IrLine {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.operation)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct BasicBlock {
    pub label: usize,
    pub statements: Vec<IrLine>,
    pub arguments: BTreeSet<ValueId>,
}

impl BasicBlock {
    pub fn new(label: usize) -> Self {
        BasicBlock {
            statements: Vec::new(),
            label,
            arguments: BTreeSet::new(),
        }
    }
}

impl<'a> From<symtab::symbol::DefResolver<'a>> for &'a BasicBlock {
    fn from(resolver: symtab::symbol::DefResolver<'a>) -> Self {
        match resolver.to_resolve {
            symtab::symbol::SymbolDef::BasicBlock(block) => block,
            symbol => panic!("Unexpected symbol seen for basic block: {}", symbol),
        }
    }
}
impl<'a> From<symtab::symbol::MutDefResolver<'a>> for &'a mut BasicBlock {
    fn from(resolver: symtab::symbol::MutDefResolver<'a>) -> Self {
        match resolver.to_resolve {
            symtab::symbol::SymbolDef::BasicBlock(block) => block,
            symbol => panic!("Unexpected symbol seen for basic block: {}", symbol),
        }
    }
}

impl<'a> Into<symtab::symbol::SymbolDef> for symtab::symbol::DefGenerator<'a, BasicBlock> {
    fn into(self) -> symtab::symbol::SymbolDef {
        symtab::symbol::SymbolDef::BasicBlock(self.to_generate_def_for)
    }
}

impl<'a> symtab::symbol::Symbol for BasicBlock {
    type SymbolKey = usize;
    fn symbol_key(&self) -> &Self::SymbolKey {
        &self.label
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
    fn new(loc: SourceLoc, operation: Operation) -> Self {
        IrLine {
            loc: loc,
            operation: operation,
        }
    }
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

    pub fn new_get_field_pointer(
        loc: SourceLoc,
        receiver: ValueId,
        field_name: String,
        destination: ValueId,
    ) -> Self {
        Self::new_lowered(
            loc,
            lowered::get_field_pointer(receiver, field_name, destination),
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
        scrutinee: ValueId,
        arms: Vec<unlowered::operands::MatchArm>,
    ) -> Self {
        Self::new_unlowered(loc, unlowered::new_match(scrutinee, arms))
    }

    //
    // general utility functions
    //
    pub fn read_value_ids(&self) -> Vec<ValueId> {
        let mut value_ids: Vec<&ValueId> = Vec::new();
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

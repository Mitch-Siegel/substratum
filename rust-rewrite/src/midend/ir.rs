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

    pub fn new_jump(
        loc: SourceLoc,
        destination_block: usize,
        condition: lowered::operands::JumpCondition,
    ) -> Self {
        Self::new(loc, lowered::new_jump(destination_block, condition))
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
        Self::new(
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
        Self::new(
            loc,
            lowered::get_field_pointer(receiver, field_name, destination),
        )
    }

    pub fn new_load(loc: SourceLoc, pointer: ValueId, destination: ValueId) -> Self {
        Self::new(loc, lowered::new_load(pointer, destination))
    }

    pub fn new_store(loc: SourceLoc, source: ValueId, pointer: ValueId) -> Self {
        Self::new(loc, lowered::new_store(source, pointer))
    }

    pub fn read_value_ids(&self) -> Vec<&ValueId> {
        let mut value_ids: Vec<&ValueId> = Vec::new();
        match &self.operation {
            Operations::Assignment(source_dest) => value_ids.push(&source_dest.source),
            Operations::BinaryOperation(operation) => {
                let sources = &operation.raw_operands().sources;

                value_ids.push(&sources.a);
                value_ids.push(&sources.b);
            }
            Operations::Jump(jump_operands) => {
                match &jump_operands.condition {
                    JumpCondition::Unconditional => {}
                    JumpCondition::Eq(condition_operands)
                    | JumpCondition::NE(condition_operands)
                    | JumpCondition::GT(condition_operands)
                    | JumpCondition::LT(condition_operands)
                    | JumpCondition::GE(condition_operands)
                    | JumpCondition::LE(condition_operands) => {
                        value_ids.push(&condition_operands.a);
                        value_ids.push(&condition_operands.b);
                    }
                };
                for arg in jump_operands.block_args.values() {
                    value_ids.push(arg);
                }
            }
            Operations::FunctionCall(function_call) => {
                for arg in &function_call.arguments {
                    value_ids.push(arg);
                }
            }
            Operations::MethodCall(method_call) => {
                let inner_function_call = &method_call.call;
                for arg in &inner_function_call.arguments {
                    value_ids.push(arg);
                }
            }
            Operations::Load(load) => {
                value_ids.push(&load.pointer);
            }
            Operations::Store(store) => {
                value_ids.push(&store.source);
            }
            Operations::ComputeFieldAddress(field_address) => {
                value_ids.push(&field_address.receiver)
            }
            Operations::GetFieldPointer(field_pointer) => value_ids.push(&field_pointer.receiver),
            Operations::Switch(switch) => value_ids.push(&switch.scrutinee),
        }
        value_ids
    }

    pub fn read_value_ids_mut(&mut self) -> Vec<&mut ValueId> {
        let mut value_ids: Vec<&mut ValueId> = Vec::new();
        match &mut self.operation {
            Operations::Assignment(source_dest) => value_ids.push(&mut source_dest.source),
            Operations::BinaryOperation(operation) => {
                let sources = &mut operation.raw_operands_mut().sources;
                value_ids.push(&mut sources.a);
                value_ids.push(&mut sources.b);
            }
            Operations::Jump(jump_operands) => {
                match &mut jump_operands.condition {
                    JumpCondition::Unconditional => {}
                    JumpCondition::Eq(condition_operands)
                    | JumpCondition::NE(condition_operands)
                    | JumpCondition::GT(condition_operands)
                    | JumpCondition::LT(condition_operands)
                    | JumpCondition::GE(condition_operands)
                    | JumpCondition::LE(condition_operands) => {
                        value_ids.push(&mut condition_operands.a);
                        value_ids.push(&mut condition_operands.b);
                    }
                };
                for arg in jump_operands.block_args.values_mut() {
                    value_ids.push(arg);
                }
            }
            Operations::FunctionCall(function_call) => {
                for arg in &mut function_call.arguments {
                    value_ids.push(arg);
                }
            }
            Operations::MethodCall(method_call) => {
                let inner_function_call = &mut method_call.call;
                for arg in &mut inner_function_call.arguments {
                    value_ids.push(arg)
                }
            }
            Operations::Load(load) => {
                value_ids.push(&mut load.destination);
            }
            Operations::Store(store) => {
                value_ids.push(&mut store.pointer); // TODO: accurately track this?
            }
            Operations::ComputeFieldAddress(field_address) => {
                value_ids.push(&mut field_address.receiver)
            }
            Operations::GetFieldPointer(field_pointer) => {
                value_ids.push(&mut field_pointer.receiver)
            }
            Operations::Switch(switch) => value_ids.push(&mut switch.scrutinee),
        }

        value_ids
    }

    pub fn write_value_ids(&self) -> Vec<&ValueId> {
        let mut value_ids: Vec<&ValueId> = Vec::new();
        match &self.operation {
            Operations::Assignment(source_dest) => value_ids.push(&source_dest.destination),
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                value_ids.push(&arithmetic_operands.destination);
            }
            Operations::FunctionCall(function_call) => {
                if let Some(retval) = &function_call.return_value_to {
                    value_ids.push(retval);
                }
            }
            Operations::MethodCall(method_call) => {
                let inner_function_call = &method_call.call;
                if let Some(retval) = &inner_function_call.return_value_to {
                    value_ids.push(retval);
                }
            }
            Operations::ComputeFieldAddress(field_address) => {
                value_ids.push(&field_address.receiver)
            }
            Operations::GetFieldPointer(field_pointer) => value_ids.push(&field_pointer.receiver),
            Operations::Store(store) => {
                value_ids.push(&store.pointer);
            }
            Operations::Jump(_) | Operations::Load(_) | Operations::Switch(_) => {}
        }

        value_ids
    }

    pub fn write_value_ids_mut(&mut self) -> Vec<&mut ValueId> {
        let mut value_ids: Vec<&mut ValueId> = Vec::new();
        match &mut self.operation {
            Operations::Assignment(source_dest) => value_ids.push(&mut source_dest.destination),
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands_mut();
                value_ids.push(&mut arithmetic_operands.destination);
            }
            Operations::FunctionCall(function_call) => {
                if let Some(retval) = &mut function_call.return_value_to {
                    value_ids.push(retval);
                }
            }
            Operations::MethodCall(method_call) => {
                let inner_function_call = &mut method_call.call;
                if let Some(retval) = &mut inner_function_call.return_value_to {
                    value_ids.push(retval);
                }
            }
            Operations::Store(store) => {
                value_ids.push(&mut store.pointer);
            }
            Operations::Load(load) => {
                value_ids.push(&mut load.destination);
            }
            Operations::ComputeFieldAddress(field_address) => {
                value_ids.push(&mut field_address.destination);
            }
            Operations::GetFieldPointer(field_pointer) => {
                value_ids.push(&mut field_pointer.destination);
            }
            Operations::Jump(_) | Operations::Switch(_) => {}
        }

        value_ids
    }
}

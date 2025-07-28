pub mod control_flow;
pub mod operands;
pub mod operations;
#[cfg(test)]
mod tests;
pub mod value;

use std::collections::BTreeSet;
use std::fmt::Display;

use crate::frontend::sourceloc::SourceLocWithMod;
use crate::midend::{ir, symtab};
use serde::Serialize;

pub use control_flow::ControlFlow;
pub use operands::*;
pub use operations::*;
pub use value::*;

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct IrLine {
    pub loc: SourceLocWithMod,
    pub operation: Operations,
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
    fn new(loc: SourceLocWithMod, operation: Operations) -> Self {
        IrLine {
            loc: loc,
            operation: operation,
        }
    }

    pub fn new_assignment(loc: SourceLocWithMod, destination: ValueId, source: ValueId) -> Self {
        Self::new(loc, Operations::new_assignment(destination, source))
    }

    pub fn new_binary_op(loc: SourceLocWithMod, op: BinaryOperations) -> Self {
        Self::new(loc, Operations::BinaryOperation(op))
    }

    pub fn new_jump(
        loc: SourceLocWithMod,
        destination_block: usize,
        condition: operands::JumpCondition,
    ) -> Self {
        Self::new(loc, Operations::new_jump(destination_block, condition))
    }

    pub fn new_function_call(
        loc: SourceLocWithMod,
        name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self::new(
            loc,
            Operations::new_function_call(name, arguments, return_value_to),
        )
    }

    pub fn new_method_call(
        loc: SourceLocWithMod,
        receiver: ValueId,
        name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self::new(
            loc,
            Operations::new_method_call(receiver, name, arguments, return_value_to),
        )
    }

    pub fn new_field_read(
        loc: SourceLocWithMod,
        receiver: ValueId,
        field_name: String,
        destination: ValueId,
    ) -> Self {
        Self::new(
            loc,
            Operations::new_field_read(receiver, field_name, destination),
        )
    }

    pub fn new_field_write(
        source: ValueId,
        loc: SourceLocWithMod,
        receiver: ValueId,
        field_name: String,
    ) -> Self {
        Self::new(
            loc,
            Operations::new_field_write(source, receiver, field_name),
        )
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
            Operations::FieldRead(field_read) => {
                value_ids.push(&field_read.receiver);
            }
            Operations::FieldWrite(field_write) => {
                value_ids.push(&field_write.source);
            }
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
            Operations::FieldRead(field_read) => {
                value_ids.push(&mut field_read.receiver);
            }
            Operations::FieldWrite(field_write) => {
                value_ids.push(&mut field_write.source);
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
            Operations::FieldWrite(field_write) => {
                value_ids.push(&field_write.receiver);
            }
            Operations::Jump(_) | Operations::FieldRead(_) | Operations::Switch(_) => {}
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
            Operations::FieldWrite(field_write) => {
                value_ids.push(&mut field_write.receiver);
            }
            Operations::Jump(_) | Operations::FieldRead(_) | Operations::Switch(_) => {}
        }

        value_ids
    }
}

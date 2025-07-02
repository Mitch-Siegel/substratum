pub mod control_flow;
pub mod operands;
pub mod operations;
#[cfg(test)]
mod tests;

use std::collections::BTreeSet;
use std::fmt::Display;

use crate::frontend::sourceloc::SourceLoc;
use crate::midend::{ir, symtab};
use serde::Serialize;

pub use control_flow::ControlFlow;
pub use operands::*;
pub use operations::*;

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct IrLine {
    pub loc: SourceLoc,
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
    pub statements: Vec<ir::IrLine>,
    pub arguments: BTreeSet<ir::OperandName>,
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

impl<'a> Into<symtab::symbol::SymbolDef> for symtab::symbol::DefGenerator<'a, BasicBlock> {
    fn into(self) -> symtab::symbol::SymbolDef {
        symtab::symbol::SymbolDef::BasicBlock(self.to_generate_def_for)
    }
}

impl<'a> symtab::symbol::Symbol<'a> for BasicBlock {
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
    fn new(loc: SourceLoc, operation: Operations) -> Self {
        IrLine {
            loc: loc,
            operation: operation,
        }
    }

    pub fn new_assignment(loc: SourceLoc, destination: Operand, source: Operand) -> Self {
        Self::new(loc, Operations::new_assignment(destination, source))
    }

    pub fn new_binary_op(loc: SourceLoc, op: BinaryOperations) -> Self {
        Self::new(loc, Operations::BinaryOperation(op))
    }

    pub fn new_jump(
        loc: SourceLoc,
        destination_block: usize,
        condition: operands::JumpCondition,
    ) -> Self {
        Self::new(loc, Operations::new_jump(destination_block, condition))
    }

    pub fn new_function_call(
        loc: SourceLoc,
        name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<Operand>,
    ) -> Self {
        Self::new(
            loc,
            Operations::new_function_call(name, arguments, return_value_to),
        )
    }

    pub fn new_method_call(
        loc: SourceLoc,
        receiver: Operand,
        name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<Operand>,
    ) -> Self {
        Self::new(
            loc,
            Operations::new_method_call(receiver, name, arguments, return_value_to),
        )
    }

    pub fn new_field_read(
        loc: SourceLoc,
        receiver: Operand,
        field_name: String,
        destination: Operand,
    ) -> Self {
        Self::new(
            loc,
            Operations::new_field_read(receiver, field_name, destination),
        )
    }

    pub fn new_field_write(
        source: Operand,
        loc: SourceLoc,
        receiver: Operand,
        field_name: String,
    ) -> Self {
        Self::new(
            loc,
            Operations::new_field_write(source, receiver, field_name),
        )
    }

    pub fn read_operand_names(&self) -> Vec<&OperandName> {
        let mut operand_names: Vec<&OperandName> = Vec::new();
        match &self.operation {
            Operations::Assignment(source_dest) => match &source_dest.source.get_name() {
                Some(name) => operand_names.push(name),
                None => {}
            },
            Operations::BinaryOperation(operation) => {
                let sources = &operation.raw_operands().sources;
                match sources.a.get_name() {
                    Some(name) => operand_names.push(name),
                    None => {}
                }
                match sources.b.get_name() {
                    Some(name) => operand_names.push(name),
                    None => {}
                }
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
                        match condition_operands.a.get_name() {
                            Some(name) => operand_names.push(name),
                            None => {}
                        }
                        match condition_operands.b.get_name() {
                            Some(name) => operand_names.push(name),
                            None => {}
                        }
                    }
                };
                for arg in jump_operands.block_args.values() {
                    operand_names.push(arg);
                }
            }
            Operations::FunctionCall(function_call) => {
                for arg in &function_call.arguments {
                    match arg.get_name() {
                        Some(arg_name) => operand_names.push(arg_name),
                        None => (),
                    }
                }
            }
            Operations::MethodCall(method_call) => {
                let inner_function_call = &method_call.call;
                for arg in &inner_function_call.arguments {
                    match arg.get_name() {
                        Some(arg_name) => operand_names.push(arg_name),
                        None => (),
                    }
                }
            }
            Operations::FieldRead(field_read) => {
                operand_names.push(field_read.receiver.get_name().unwrap());
            }
            Operations::FieldWrite(field_write) => {
                operand_names.push(field_write.source.get_name().unwrap());
            }
        }
        operand_names
    }

    pub fn read_operand_names_mut(&mut self) -> Vec<&mut OperandName> {
        let mut operand_names: Vec<&mut OperandName> = Vec::new();
        match &mut self.operation {
            Operations::Assignment(source_dest) => match source_dest.source.get_name_mut() {
                Some(name) => operand_names.push(name),
                None => {}
            },
            Operations::BinaryOperation(operation) => {
                let sources = &mut operation.raw_operands_mut().sources;
                match sources.a.get_name_mut() {
                    Some(name) => operand_names.push(name),
                    None => {}
                }
                match sources.b.get_name_mut() {
                    Some(name) => operand_names.push(name),
                    None => {}
                }
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
                        match condition_operands.a.get_name_mut() {
                            Some(name) => operand_names.push(name),
                            None => {}
                        }
                        match condition_operands.b.get_name_mut() {
                            Some(name) => operand_names.push(name),
                            None => {}
                        }
                    }
                };
                for arg in jump_operands.block_args.values_mut() {
                    operand_names.push(arg);
                }
            }
            Operations::FunctionCall(function_call) => {
                for arg in &mut function_call.arguments {
                    match arg.get_name_mut() {
                        Some(arg_name) => operand_names.push(arg_name),
                        None => (),
                    }
                }
            }
            Operations::MethodCall(method_call) => {
                let inner_function_call = &mut method_call.call;
                for arg in &mut inner_function_call.arguments {
                    match arg.get_name_mut() {
                        Some(arg_name) => operand_names.push(arg_name),
                        None => (),
                    }
                }
            }
            Operations::FieldRead(field_read) => {
                operand_names.push(field_read.receiver.get_name_mut().unwrap());
            }
            Operations::FieldWrite(field_write) => {
                operand_names.push(field_write.source.get_name_mut().unwrap());
            }
        }

        operand_names
    }

    pub fn write_operand_names(&self) -> Vec<&OperandName> {
        let mut operand_names: Vec<&OperandName> = Vec::new();
        match &self.operation {
            Operations::Assignment(source_dest) => {
                operand_names.push(source_dest.destination.get_name().unwrap())
            }
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands();
                operand_names.push(arithmetic_operands.destination.get_name().unwrap());
            }
            Operations::FunctionCall(function_call) => {
                if let Some(retval) = &function_call.return_value_to {
                    operand_names.push(retval.get_name().unwrap());
                }
            }
            Operations::MethodCall(method_call) => {
                let inner_function_call = &method_call.call;
                if let Some(retval) = &inner_function_call.return_value_to {
                    operand_names.push(retval.get_name().unwrap());
                }
            }
            Operations::FieldWrite(field_write) => {
                operand_names.push(field_write.receiver.get_name().unwrap());
            }
            Operations::Jump(_) | Operations::FieldRead(_) => {}
        }

        operand_names
    }

    pub fn write_operand_names_mut(&mut self) -> Vec<&mut OperandName> {
        let mut operand_names: Vec<&mut OperandName> = Vec::new();
        match &mut self.operation {
            Operations::Assignment(source_dest) => {
                operand_names.push(source_dest.destination.get_name_mut().unwrap())
            }
            Operations::BinaryOperation(operation) => {
                let arithmetic_operands = operation.raw_operands_mut();
                operand_names.push(arithmetic_operands.destination.get_name_mut().unwrap());
            }
            Operations::FunctionCall(function_call) => {
                if let Some(retval) = &mut function_call.return_value_to {
                    operand_names.push(retval.get_name_mut().unwrap());
                }
            }
            Operations::MethodCall(method_call) => {
                let inner_function_call = &mut method_call.call;
                if let Some(retval) = &mut inner_function_call.return_value_to {
                    operand_names.push(retval.get_name_mut().unwrap());
                }
            }
            Operations::FieldWrite(field_write) => {
                operand_names.push(field_write.receiver.get_name_mut().unwrap());
            }
            Operations::Jump(_) | Operations::FieldRead(_) => {}
        }

        operand_names
    }
}

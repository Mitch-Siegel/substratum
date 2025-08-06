use serde::Serialize;
use std::{collections::HashMap, fmt::Display};

use crate::midend::ir::*;

/// ## Binary Operations
#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum BinaryOperations {
    Add(BinaryArithmeticOperands),
    Subtract(BinaryArithmeticOperands),
    Multiply(BinaryArithmeticOperands),
    Divide(BinaryArithmeticOperands),
    LThan(BinaryArithmeticOperands),
    GThan(BinaryArithmeticOperands),
    LThanE(BinaryArithmeticOperands),
    GThanE(BinaryArithmeticOperands),
    Equals(BinaryArithmeticOperands),
    NotEquals(BinaryArithmeticOperands),
}

impl Display for BinaryOperations {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Add(operands) => {
                write!(
                    f,
                    "{} = {} + {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::Subtract(operands) => {
                write!(
                    f,
                    "{} = {} - {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::Multiply(operands) => {
                write!(
                    f,
                    "{} = {} * {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::Divide(operands) => {
                write!(
                    f,
                    "{} = {} / {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::LThan(operands) => {
                write!(
                    f,
                    "{} = {} < {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::GThan(operands) => {
                write!(
                    f,
                    "{} = {} > {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::LThanE(operands) => {
                write!(
                    f,
                    "{} = {} <= {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::GThanE(operands) => {
                write!(
                    f,
                    "{} = {} >= {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::Equals(operands) => {
                write!(
                    f,
                    "{} = {} == {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
            Self::NotEquals(operands) => {
                write!(
                    f,
                    "{} = {} != {}",
                    operands.destination, operands.sources.a, operands.sources.b
                )
            }
        }
    }
}

impl BinaryOperations {
    pub fn new_add(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::Add(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_subtract(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::Subtract(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_multiply(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::Multiply(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_divide(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::Divide(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_lthan(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::LThan(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_gthan(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::GThan(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_lthan_e(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::LThanE(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_gthan_e(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::GThanE(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_equals(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::Equals(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn new_not_equals(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryOperations::NotEquals(BinaryArithmeticOperands::from(
            destination,
            source_a,
            source_b,
        ))
    }

    pub fn raw_operands(&self) -> &BinaryArithmeticOperands {
        match self {
            Self::Add(ops)
            | Self::Subtract(ops)
            | Self::Multiply(ops)
            | Self::Divide(ops)
            | Self::LThan(ops)
            | Self::GThan(ops)
            | Self::LThanE(ops)
            | Self::GThanE(ops)
            | Self::Equals(ops)
            | Self::NotEquals(ops) => ops,
        }
    }

    pub fn raw_operands_mut(&mut self) -> &mut BinaryArithmeticOperands {
        match self {
            Self::Add(ops)
            | Self::Subtract(ops)
            | Self::Multiply(ops)
            | Self::Divide(ops)
            | Self::LThan(ops)
            | Self::GThan(ops)
            | Self::LThanE(ops)
            | Self::GThanE(ops)
            | Self::Equals(ops)
            | Self::NotEquals(ops) => ops,
        }
    }
}

/// ## Jump
#[derive(Debug, Serialize, Clone, PartialEq, Eq)]
pub struct JumpOperation {
    pub destination_block: usize,
    pub block_args: HashMap<ValueId, ValueId>,
    pub condition: JumpCondition,
}

impl Display for JumpOperation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut block_args_string = String::new();
        for (arg, operand) in &self.block_args {
            block_args_string += &format!("{}:{} ", arg, operand);
        }
        write!(
            f,
            "{} Block{}({})",
            self.condition, self.destination_block, block_args_string
        )
    }
}

impl JumpOperation {
    pub fn new(destination_block: usize, condition: JumpCondition) -> Self {
        Self {
            destination_block,
            block_args: HashMap::new(),
            condition,
        }
    }
}

/// ## Enum of all operations
#[derive(Debug, Serialize, Clone, PartialEq, Eq)]
pub enum Operations {
    Assignment(SourceDestOperands),
    BinaryOperation(BinaryOperations),
    Jump(JumpOperation),
    FunctionCall(FunctionCallOperands),
    MethodCall(MethodCallOperands),
    Load(LoadOperands),
    Store(StoreOperands),
    ComputeFieldAddress(FieldAddressOperands),
    GetFieldPointer(FieldPointerOperands),
    Switch(SwitchOperands),
}

impl Display for Operations {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Assignment(assignment) => {
                write!(f, "{} = {}", assignment.destination, assignment.source)
            }
            Self::BinaryOperation(binary_operation) => write!(f, "{}", binary_operation),
            Self::Jump(jump) => write!(f, "{}", jump),
            Self::FunctionCall(function_call) => write!(f, "{}", function_call),
            Self::MethodCall(method_call) => write!(f, "{}", method_call),
            Self::Load(load) => write!(f, "{} = *{}", load.destination, load.pointer),
            Self::Store(store) => write!(f, "*{} = {}", store.pointer, store.source),
            Self::ComputeFieldAddress(field_address) => write!(
                f,
                "{} = {} + {}",
                field_address.destination, field_address.receiver, field_address.offset
            ),
            Self::GetFieldPointer(field_read) => write!(
                f,
                "{} = {}.{}",
                field_read.destination, field_read.receiver, field_read.field_name
            ),
            Self::Switch(switch) => write!(
                f,
                "switch {} (default {}): {:?}",
                switch.scrutinee, switch.default_label, switch.cases
            ),
        }
    }
}

impl Operations {
    pub fn new_assignment(destination: ValueId, source: ValueId) -> Self {
        Self::Assignment(SourceDestOperands {
            destination,
            source,
        })
    }

    pub fn new_jump(destination_block: usize, condition: JumpCondition) -> Self {
        Self::Jump(JumpOperation::new(destination_block, condition))
    }

    pub fn new_function_call(
        name: String,
        arguments: OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self::FunctionCall(FunctionCallOperands::new(name, arguments, return_value_to))
    }

    pub fn new_method_call(
        receiver: ValueId,
        name: String,
        arguments: OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self::MethodCall(MethodCallOperands::new(
            receiver,
            name,
            arguments,
            return_value_to,
        ))
    }

    pub fn new_compute_field_address(
        receiver: ValueId,
        offset: usize,
        destination: ValueId,
    ) -> Self {
        Self::ComputeFieldAddress(FieldAddressOperands {
            receiver,
            offset,
            destination,
        })
    }

    pub fn get_field_pointer(receiver: ValueId, field_name: String, destination: ValueId) -> Self {
        Self::GetFieldPointer(FieldPointerOperands {
            receiver,
            field_name,
            destination,
        })
    }

    pub fn new_load(pointer: ValueId, destination: ValueId) -> Self {
        Self::Load(LoadOperands {
            pointer,
            destination,
        })
    }

    pub fn new_store(source: ValueId, pointer: ValueId) -> Self {
        Self::Store(StoreOperands { source, pointer })
    }
}

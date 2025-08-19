use serde::Serialize;
pub mod operands;
pub mod operations;

use crate::midend::ir::*;
use operands::*;
use operations::*;
use std::fmt::Display;

/// ## Enum of all operations
#[derive(Debug, Serialize, Clone, PartialEq, Eq)]
pub enum Operation {
    Assignment(SourceDestOperands),
    BinaryArithmetic(BinaryArithmeticOperands),
    BinaryComparison(BinaryComparisonOperands),
    Jump(JumpOperation),
    FunctionCall(FunctionCallOperands),
    MethodCall(MethodCallOperands),
    Load(LoadOperands),
    Store(StoreOperands),
    ComputeFieldAddress(FieldAddressOperands),
    GetFieldPointer(FieldPointerOperands),
    Switch(SwitchOperands),
}

impl Operation {
    pub fn read_value_ids(&self) -> Vec<ValueId> {
        match self {
            Self::Assignment(source_dest) => vec![source_dest.source],
            Self::BinaryArithmetic(arithmetic) => {
                let sources = &arithmetic.sources;
                vec![sources.a, sources.b]
            }
            Self::Jump(jump_operands) => {
                let mut operands = Vec::new();
                match &jump_operands.condition {
                    lowered::operands::JumpCondition::Unconditional => {}
                    lowered::operands::JumpCondition::Eq(condition_operands)
                    | lowered::operands::JumpCondition::NE(condition_operands)
                    | lowered::operands::JumpCondition::GT(condition_operands)
                    | lowered::operands::JumpCondition::LT(condition_operands)
                    | lowered::operands::JumpCondition::GE(condition_operands)
                    | lowered::operands::JumpCondition::LE(condition_operands) => {
                        operands.push(condition_operands.a);
                        operands.push(condition_operands.b);
                    }
                };
                for arg in jump_operands.block_args.values() {
                    operands.push(*arg);
                }
                operands
            }
            Self::FunctionCall(function_call) => function_call.arguments.clone(),
            Self::MethodCall(method_call) => method_call.call.arguments.clone(),
            Self::Load(load) => {
                vec![load.pointer]
            }
            Self::Store(store) => {
                vec![store.source]
            }
            Self::ComputeFieldAddress(field_address) => vec![field_address.receiver],
            Self::GetFieldPointer(field_pointer) => vec![field_pointer.receiver],
            Self::Switch(switch) => vec![switch.scrutinee],
        }
    }

    fn write_value_ids(&self) -> Vec<ValueId> {
        match self {
            Self::Assignment(assignment) => vec![assignment.destination],
            Self::BinaryArithmetic(arithmetic) => vec![arithmetic.destination],

            Self::FunctionCall(function_call) => {
                if let Some(retval) = &function_call.return_value_to {
                    vec![*retval]
                } else {
                    vec![]
                }
            }
            Self::MethodCall(method_call) => {
                let inner_function_call = &method_call.call;
                if let Some(retval) = &inner_function_call.return_value_to {
                    vec![*retval]
                } else {
                    vec![]
                }
            }
            Self::ComputeFieldAddress(field_address) => vec![field_address.receiver],
            Self::GetFieldPointer(field_pointer) => vec![field_pointer.receiver],
            Self::Load(load) => {
                vec![load.destination]
            }
            Self::Store(store) => {
                vec![store.pointer]
            }
            Self::Jump(_) | Self::Switch(_) => {
                vec![]
            }
        }
    }
}

impl Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Assignment(assignment) => {
                write!(f, "{} = {}", assignment.destination, assignment.source)
            }
            Self::BinaryArithmetic(arithmetic) => write!(f, "{}", arithmetic),
            Self::BinaryComparison(comparison) => write!(f, "{}", comparison),
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

pub fn new_assignment(destination: ValueId, source: ValueId) -> Operation {
    Operation::Assignment(SourceDestOperands {
        destination,
        source,
    })
}

pub fn new_add(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryArithmetic(BinaryArithmeticOperands::new_add(
        destination,
        source_a,
        source_b,
    ))
}

pub fn new_sub(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryArithmetic(BinaryArithmeticOperands::new_sub(
        destination,
        source_a,
        source_b,
    ))
}
pub fn new_mul(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryArithmetic(BinaryArithmeticOperands::new_mul(
        destination,
        source_a,
        source_b,
    ))
}
pub fn new_div(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryArithmetic(BinaryArithmeticOperands::new_div(
        destination,
        source_a,
        source_b,
    ))
}

pub fn new_lt(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryComparison(BinaryComparisonOperands::new_lt(
        destination,
        source_a,
        source_b,
    ))
}

pub fn new_gt(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryComparison(BinaryComparisonOperands::new_gt(
        destination,
        source_a,
        source_b,
    ))
}

pub fn new_le(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryComparison(BinaryComparisonOperands::new_le(
        destination,
        source_a,
        source_b,
    ))
}

pub fn new_ge(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryComparison(BinaryComparisonOperands::new_ge(
        destination,
        source_a,
        source_b,
    ))
}

pub fn new_eq(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryComparison(BinaryComparisonOperands::new_eq(
        destination,
        source_a,
        source_b,
    ))
}

pub fn new_ne(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Operation {
    Operation::BinaryComparison(BinaryComparisonOperands::new_ne(
        destination,
        source_a,
        source_b,
    ))
}

pub fn new_jump(destination_block: usize, condition: JumpCondition) -> Operation {
    Operation::Jump(JumpOperation::new(destination_block, condition))
}

pub fn new_function_call(
    name: String,
    arguments: OrderedArgumentList,
    return_value_to: Option<ValueId>,
) -> Operation {
    Operation::FunctionCall(FunctionCallOperands::new(name, arguments, return_value_to))
}

pub fn new_method_call(
    receiver: ValueId,
    name: String,
    arguments: OrderedArgumentList,
    return_value_to: Option<ValueId>,
) -> Operation {
    Operation::MethodCall(MethodCallOperands::new(
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
) -> Operation {
    Operation::ComputeFieldAddress(FieldAddressOperands {
        receiver,
        offset,
        destination,
    })
}

pub fn get_field_pointer(receiver: ValueId, field_name: String, destination: ValueId) -> Operation {
    Operation::GetFieldPointer(FieldPointerOperands {
        receiver,
        field_name,
        destination,
    })
}

pub fn new_load(pointer: ValueId, destination: ValueId) -> Operation {
    Operation::Load(LoadOperands {
        pointer,
        destination,
    })
}

pub fn new_store(source: ValueId, pointer: ValueId) -> Operation {
    Operation::Store(StoreOperands { source, pointer })
}

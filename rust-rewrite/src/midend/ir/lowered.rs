use serde::Serialize;
pub mod operands;

use crate::midend::ir::*;
pub use operands::*;
use std::fmt::Display;

/// ## Enum of all operations
#[derive(Debug, Serialize, Clone, PartialEq, Eq)]
#[enum_delegate::implement(OperandTypePropagation)]
pub enum Operation {
    Assignment(AssignmentOperands),
    BinaryArithmetic(BinaryArithmeticExpressionOperands),
    BinaryComparison(BinaryComparisonExpressionOperands),
    Jump(JumpOperands),
    FunctionCall(FunctionCallOperands),
    MethodCall(MethodCallOperands),
    Load(LoadOperands),
    Store(StoreOperands),
    ComputeFieldAddress(FieldAddressOperands),
    Switch(SwitchOperands),
}

impl IrOperation for Operation {
    fn read_value_ids(&self) -> Vec<ValueId> {
        match self {
            Self::Assignment(source_dest) => vec![source_dest.source],
            Self::BinaryArithmetic(arithmetic) => {
                let sources = &arithmetic.arithmetic.sources;
                vec![sources.lhs, sources.rhs]
            }
            Self::BinaryComparison(comparison) => {
                let sources = &comparison.comparison.sources;
                vec![sources.lhs, sources.rhs]
            }
            Self::Jump(jump) => {
                let mut operands = Vec::new();
                match &jump.condition {
                    lowered::operands::JumpCondition::Conditional(condition) => {
                        operands.push(condition.sources.lhs);
                        operands.push(condition.sources.lhs);
                    }
                    lowered::operands::JumpCondition::Unconditional => {}
                };
                for arg in jump.block_args.values() {
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
            Self::Switch(switch) => vec![switch.scrutinee],
        }
    }

    fn write_value_ids(&self) -> Vec<ValueId> {
        match self {
            Self::Assignment(assignment) => vec![assignment.destination],
            Self::BinaryArithmetic(arithmetic) => vec![arithmetic.destination],
            Self::BinaryComparison(comparison) => vec![comparison.destination],
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

pub fn new_binary_arithmetic_expression(
    destination: ValueId,
    operands: BinaryArithmeticOperands,
) -> Operation {
    Operation::BinaryArithmetic(BinaryArithmeticExpressionOperands::new(
        destination,
        operands,
    ))
}

pub fn new_binary_comparison_expression(
    destination: ValueId,
    comparison: BinaryComparisonOperands,
) -> Operation {
    Operation::BinaryComparison(BinaryComparisonExpressionOperands::new(
        destination,
        comparison,
    ))
}

pub fn new_jump(destination_block: usize, condition: JumpCondition) -> Operation {
    Operation::Jump(JumpOperands::new(destination_block, condition))
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

pub fn new_load(pointer: ValueId, destination: ValueId) -> Operation {
    Operation::Load(LoadOperands {
        pointer,
        destination,
    })
}

pub fn new_store(source: ValueId, pointer: ValueId) -> Operation {
    Operation::Store(StoreOperands { source, pointer })
}

use std::fmt::Display;

use serde::Serialize;

use crate::midend::ir::*;

/*
 groupings of operands
*/

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct DualSourceOperands {
    pub a: ValueId,
    pub b: ValueId,
}

impl DualSourceOperands {
    pub fn new(a: ValueId, b: ValueId) -> Self {
        DualSourceOperands { a, b }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct BinaryArithmeticOperands {
    pub destination: ValueId,
    pub sources: DualSourceOperands,
}

impl BinaryArithmeticOperands {
    pub fn from(destination: ValueId, source_a: ValueId, source_b: ValueId) -> Self {
        BinaryArithmeticOperands {
            destination,
            sources: DualSourceOperands::new(source_a, source_b),
        }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct SourceDestOperands {
    pub destination: ValueId,
    pub source: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum JumpCondition {
    Unconditional,
    Eq(DualSourceOperands),
    NE(DualSourceOperands),
    GT(DualSourceOperands),
    LT(DualSourceOperands),
    GE(DualSourceOperands),
    LE(DualSourceOperands),
}

impl Display for JumpCondition {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Unconditional => {
                write!(f, "jmp")
            }
            Self::Eq(operands) => {
                write!(f, "jeq({}, {})", operands.a, operands.b)
            }
            Self::NE(operands) => {
                write!(f, "jne({}, {})", operands.a, operands.b)
            }
            Self::LT(operands) => {
                write!(f, "jl({}, {})", operands.a, operands.b)
            }
            Self::GT(operands) => {
                write!(f, "jg({}, {})", operands.a, operands.b)
            }
            Self::LE(operands) => {
                write!(f, "jle({}, {})", operands.a, operands.b)
            }
            Self::GE(operands) => {
                write!(f, "jge({}, {})", operands.a, operands.b)
            }
        }
    }
}

pub type OrderedArgumentList = Vec<ValueId>;

fn arg_list_to_string(args: &OrderedArgumentList) -> String {
    let mut arg_string = String::new();
    for arg in args {
        if arg_string.len() > 0 {
            arg_string += &",";
        }

        arg_string += &format!("{}", arg);
    }
    arg_string
}

/// ## Function Call Operands
#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct FunctionCallOperands {
    pub function_name: String,
    pub arguments: OrderedArgumentList,
    pub return_value_to: Option<ValueId>,
}

impl Display for FunctionCallOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}({})",
            self.function_name,
            arg_list_to_string(&self.arguments)
        )
    }
}

impl FunctionCallOperands {
    pub fn new(
        name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self {
            function_name: name.into(),
            arguments,
            return_value_to,
        }
    }
}

/// ## Method Call Operands
#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct MethodCallOperands {
    pub receiver: ValueId,
    pub call: FunctionCallOperands,
}

impl Display for MethodCallOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.receiver, self.call)
    }
}

impl MethodCallOperands {
    pub fn new(
        receiver: ValueId,
        method_name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self {
            receiver,
            call: FunctionCallOperands::new(method_name, arguments, return_value_to),
        }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct LoadOperands {
    pub pointer: ValueId,
    pub destination: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct StoreOperands {
    pub pointer: ValueId,
    pub source: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct FieldAddressOperands {
    pub receiver: ValueId,
    pub offset: usize,
    pub destination: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct FieldPointerOperands {
    pub receiver: ValueId,
    pub field_name: String,
    pub destination: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct SwitchOperands {
    pub scrutinee: ValueId,
    pub default_label: usize,
    pub cases: Vec<(ValueId, usize)>,
}

#[cfg(test)]
mod tests {
    use std::cmp::Ordering;

    use crate::midend::ir::*;
}

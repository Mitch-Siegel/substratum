use std::fmt::Display;

use serde::Serialize;

use crate::midend::ir::*;

/*
 groupings of operands
*/

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum BinaryArithmeticKind {
    Add,
    Sub,
    Mul,
    Div,
}

impl Display for BinaryArithmeticKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::Add => "+",
                Self::Sub => "-",
                Self::Mul => "*",
                Self::Div => "/",
            }
        )
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct BinaryArithmeticExpressionOperands {
    pub destination: ValueId,
    pub arithmetic: BinaryArithmeticOperands,
}

impl BinaryArithmeticExpressionOperands {
    pub fn new(destination: ValueId, arithmetic: BinaryArithmeticOperands) -> Self {
        Self {
            destination,
            arithmetic,
        }
    }
}

impl Display for BinaryArithmeticExpressionOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.destination, self.arithmetic)
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct BinaryArithmeticOperands {
    pub sources: BinarySourceOperands,
    pub kind: BinaryArithmeticKind,
}

impl Display for BinaryArithmeticOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {} {}", self.sources.lhs, self.kind, self.sources.rhs)
    }
}

impl BinaryArithmeticOperands {
    pub fn new(lhs: ValueId, rhs: ValueId, kind: BinaryArithmeticKind) -> Self {
        Self {
            sources: BinarySourceOperands::new(lhs, rhs),
            kind,
        }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct BinarySourceOperands {
    pub lhs: ValueId,
    pub rhs: ValueId,
}

impl BinarySourceOperands {
    pub fn new(a: ValueId, b: ValueId) -> Self {
        BinarySourceOperands { lhs: a, rhs: b }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct SourceDestOperands {
    pub destination: ValueId,
    pub source: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum BinaryComparisonKind {
    LT,
    GT,
    LE,
    GE,
    EQ,
    NE,
}

impl Display for BinaryComparisonKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}",
            match self {
                Self::LT => "<",
                Self::GT => ">",
                Self::LE => "<=",
                Self::GE => ">=",
                Self::EQ => "==",
                Self::NE => "!=",
            }
        )
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct BinaryComparisonExpressionOperands {
    pub destination: ValueId,
    pub comparison: BinaryComparisonOperands,
}

impl BinaryComparisonExpressionOperands {
    pub fn new(destination: ValueId, comparison: BinaryComparisonOperands) -> Self {
        Self {
            destination,
            comparison,
        }
    }
}

impl Display for BinaryComparisonExpressionOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.destination, self.comparison)
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct BinaryComparisonOperands {
    pub sources: BinarySourceOperands,
    pub kind: BinaryComparisonKind,
}

impl Display for BinaryComparisonOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {} {}", self.sources.lhs, self.kind, self.sources.rhs)
    }
}

impl BinaryComparisonOperands {
    pub fn new(source_a: ValueId, source_b: ValueId, kind: BinaryComparisonKind) -> Self {
        Self {
            sources: BinarySourceOperands::new(source_a, source_b),
            kind,
        }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub enum JumpCondition {
    Unconditional,
    Conditional(BinaryComparisonOperands),
}

impl Display for JumpCondition {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Unconditional => {
                write!(f, "jmp")
            }
            Self::Conditional(condition) => match condition.kind {
                BinaryComparisonKind::LT => {
                    write!(
                        f,
                        "jl({}, {})",
                        condition.sources.lhs, condition.sources.rhs
                    )
                }
                BinaryComparisonKind::GT => {
                    write!(
                        f,
                        "jg({}, {})",
                        condition.sources.lhs, condition.sources.rhs
                    )
                }
                BinaryComparisonKind::LE => {
                    write!(
                        f,
                        "jle({}, {})",
                        condition.sources.lhs, condition.sources.rhs
                    )
                }
                BinaryComparisonKind::GE => {
                    write!(
                        f,
                        "jge({}, {})",
                        condition.sources.lhs, condition.sources.rhs
                    )
                }
                BinaryComparisonKind::EQ => {
                    write!(
                        f,
                        "jeq({}, {})",
                        condition.sources.lhs, condition.sources.rhs
                    )
                }
                BinaryComparisonKind::NE => {
                    write!(
                        f,
                        "jne({}, {})",
                        condition.sources.lhs, condition.sources.rhs
                    )
                }
            },
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
        name: String,
        arguments: OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self {
            function_name: name,
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
        method_name: String,
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

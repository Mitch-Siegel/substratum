use serde::Serialize;
use std::collections::HashMap;
use std::fmt::Display;

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

impl OperandTypeInference for BinaryArithmeticExpressionOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
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

pub type AssignmentOperands = SourceDestOperands;
impl OperandTypeInference for AssignmentOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
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

impl OperandTypeInference for BinaryComparisonExpressionOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!()
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

#[derive(Debug, Serialize, Clone, PartialEq, Eq)]
pub struct JumpOperands {
    pub destination_block: usize,
    pub block_args: HashMap<ValueId, ValueId>,
    pub condition: JumpCondition,
}

impl JumpOperands {
    pub fn new(destination_block: usize, condition: JumpCondition) -> Self {
        Self {
            destination_block,
            block_args: HashMap::new(),
            condition,
        }
    }
}

impl OperandTypeInference for JumpOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
}

impl Display for JumpOperands {
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
pub struct CallParams {
    pub arguments: OrderedArgumentList,
    pub return_value_to: Option<ValueId>,
}

impl CallParams {
    pub fn new(arguments: OrderedArgumentList, return_value_to: Option<ValueId>) -> Self {
        Self {
            arguments,
            return_value_to,
        }
    }
}

impl OperandTypeInference for CallParams {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!()
    }
}

impl Display for CallParams {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({})", arg_list_to_string(&self.arguments))?;
        if let Some(retval) = self.return_value_to {
            write!(f, " -> {}", retval)?;
        }

        Ok(())
    }
}

/// ## Method Call Operands
#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct CallOperands {
    pub function_operand: ValueId,
    pub params: CallParams,
}

impl CallOperands {
    pub fn new(
        function_operand: ValueId,
        arguments: OrderedArgumentList,
        return_value_to: Option<ValueId>,
    ) -> Self {
        Self {
            function_operand,
            params: CallParams::new(arguments, return_value_to),
        }
    }
}

impl OperandTypeInference for CallOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
}

impl Display for CallOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.function_operand, self.params)
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct LoadOperands {
    pub pointer: ValueId,
    pub destination: ValueId,
}

impl OperandTypeInference for LoadOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct StoreOperands {
    pub pointer: ValueId,
    pub source: ValueId,
}

impl OperandTypeInference for StoreOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct FieldAddressOperands {
    pub receiver: ValueId,
    pub offset: usize,
    pub destination: ValueId,
}

impl OperandTypeInference for FieldAddressOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct SwitchOperands {
    pub scrutinee: ValueId,
    pub default_label: usize,
    pub cases: Vec<(ValueId, usize)>,
}

impl OperandTypeInference for SwitchOperands {
    fn infer_types(&mut self, _ctx: &TypeInferenceContext) -> bool {
        unimplemented!();
    }
}

#[cfg(test)]
mod tests {}

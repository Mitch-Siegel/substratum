use std::{collections::HashMap, fmt::Display};

use serde::Serialize;

use crate::{ir::ValueId, types};

/*
 groupings of operands
*/

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) enum BinaryArithmeticKind {
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

// FUTURE: isolate this to a single binray operand type for deduplication
#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct BinaryArithmeticExpressionOperands {
    pub destination: ValueId,
    pub arithmetic: BinaryArithmeticOperands,
}

impl BinaryArithmeticExpressionOperands {
    pub(crate) fn new(destination: ValueId, arithmetic: BinaryArithmeticOperands) -> Self {
        Self {
            destination,
            arithmetic,
        }
    }
}

impl types::Inference for BinaryArithmeticExpressionOperands {
    fn infer_types(&mut self, ctx: &mut types::inference::Ctx<'_>) -> types::inference::Output {
        ctx.check_inferee_type(self.destination)?;

        let assigned_ty = {
            let lhs_ty = ctx.type_for_value(self.arithmetic.sources.lhs)?;
            let rhs_ty = ctx.type_for_value(self.arithmetic.sources.rhs)?;

            let lhs_sem = ctx.types.lookup(lhs_ty, ctx.symtab);
            let rhs_sem = ctx.types.lookup(rhs_ty, ctx.symtab);

            let lhs_size = lhs_sem.size();
            let rhs_size = rhs_sem.size();

            // FUTURE: check against comparison types here
            if lhs_size > rhs_size {
                lhs_ty.clone()
            } else {
                rhs_ty.clone()
            }
        };

        ctx.assign_type_to_inferee(self.destination, assigned_ty)
            .unwrap()
    }
}

impl Display for BinaryArithmeticExpressionOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.destination, self.arithmetic)
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct BinaryArithmeticOperands {
    pub sources: BinarySourceOperands,
    pub kind: BinaryArithmeticKind,
}

impl Display for BinaryArithmeticOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {} {}", self.sources.lhs, self.kind, self.sources.rhs)
    }
}

impl BinaryArithmeticOperands {
    pub(crate) fn new(lhs: ValueId, rhs: ValueId, kind: BinaryArithmeticKind) -> Self {
        Self {
            sources: BinarySourceOperands::new(lhs, rhs),
            kind,
        }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct BinarySourceOperands {
    pub lhs: ValueId,
    pub rhs: ValueId,
}

impl BinarySourceOperands {
    pub(crate) fn new(a: ValueId, b: ValueId) -> Self {
        Self { lhs: a, rhs: b }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct SourceDestOperands {
    pub destination: ValueId,
    pub source: ValueId,
}

pub(crate) type AssignmentOperands = SourceDestOperands;
impl types::Inference for AssignmentOperands {
    fn infer_types(&mut self, ctx: &mut types::inference::Ctx) -> types::inference::Output {
        ctx.check_inferee_type(self.destination)?;

        let rhs_ty = ctx.type_for_value(self.source)?;
        ctx.assign_type_to_inferee(self.destination, rhs_ty.clone())
            .unwrap()
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) enum BinaryComparisonKind {
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
pub(crate) struct BinaryComparisonExpressionOperands {
    pub destination: ValueId,
    pub comparison: BinaryComparisonOperands,
}

impl BinaryComparisonExpressionOperands {
    pub(crate) fn new(destination: ValueId, comparison: BinaryComparisonOperands) -> Self {
        Self {
            destination,
            comparison,
        }
    }
}

impl types::Inference for BinaryComparisonExpressionOperands {
    fn infer_types(&mut self, ctx: &mut types::inference::Ctx) -> types::inference::Output {
        ctx.check_inferee_type(self.destination)?;

        let assigned_ty = {
            let lhs_ty = ctx.type_for_value(self.comparison.sources.lhs)?;
            let rhs_ty = ctx.type_for_value(self.comparison.sources.rhs)?;

            let lhs_sem = ctx.types.lookup(lhs_ty, ctx.symtab);
            let rhs_sem = ctx.types.lookup(rhs_ty, ctx.symtab);

            let lhs_size = lhs_sem.size();
            let rhs_size = rhs_sem.size();

            // FUTURE: check against comparison types here
            if lhs_size > rhs_size {
                lhs_ty.clone()
            } else {
                rhs_ty.clone()
            }
        };

        ctx.assign_type_to_inferee(self.destination, assigned_ty)
            .unwrap()
    }
}

impl Display for BinaryComparisonExpressionOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.destination, self.comparison)
    }
}

// FUTURE: isolate this to a single binray operand type for deduplication
#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct BinaryComparisonOperands {
    pub sources: BinarySourceOperands,
    pub kind: BinaryComparisonKind,
}

impl Display for BinaryComparisonOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {} {}", self.sources.lhs, self.kind, self.sources.rhs)
    }
}

impl BinaryComparisonOperands {
    pub(crate) fn new(source_a: ValueId, source_b: ValueId, kind: BinaryComparisonKind) -> Self {
        Self {
            sources: BinarySourceOperands::new(source_a, source_b),
            kind,
        }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) enum JumpCondition {
    Unconditional,
    Conditional(BinaryComparisonOperands),
}

impl types::Inference for JumpCondition {
    fn infer_types(&mut self, ctx: &mut types::inference::Ctx<'_>) -> types::inference::Output {
        match self {
            Self::Unconditional => (),
            Self::Conditional(c) => {
                ctx.type_for_value(c.sources.lhs)?;
                ctx.type_for_value(c.sources.rhs)?;
            }
        }
        types::inference::Output::Continue(types::inference::ContinueReason::AlreadyDone)
    }
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
pub(crate) struct JumpOperands {
    pub destination_block: usize,
    pub block_args: HashMap<ValueId, ValueId>,
    pub condition: JumpCondition,
}

impl JumpOperands {
    pub(crate) fn new(destination_block: usize, condition: JumpCondition) -> Self {
        Self {
            destination_block,
            block_args: HashMap::new(),
            condition,
        }
    }
}

impl types::Inference for JumpOperands {
    fn infer_types(&mut self, ctx: &mut types::inference::Ctx) -> types::inference::Output {
        self.condition.infer_types(ctx)
    }
}

impl Display for JumpOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{} Block{}({})",
            self.condition,
            self.destination_block,
            self.block_args
                .iter()
                .map(|(arg, operand)| format!("{arg}:{operand}"))
                .collect::<Vec<String>>()
                .join(", ")
        )
    }
}

pub(crate) type OrderedArgumentList = Vec<ValueId>;

/// ## Function Call Operands
#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct CallParams {
    pub arguments: OrderedArgumentList,
    pub return_value_to: Option<ValueId>,
}

impl CallParams {
    pub(crate) fn new(arguments: OrderedArgumentList, return_value_to: Option<ValueId>) -> Self {
        Self {
            arguments,
            return_value_to,
        }
    }
}

impl types::Inference for CallParams {
    fn infer_types(&mut self, _ctx: &mut types::inference::Ctx) -> types::inference::Output {
        unimplemented!()
    }
}

impl Display for CallParams {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "({})",
            self.arguments
                .iter()
                .map(|arg| format!("{arg}"))
                .collect::<Vec<String>>()
                .join(", ")
        )?;
        if let Some(retval) = self.return_value_to {
            write!(f, " -> {retval}")?;
        }

        Ok(())
    }
}

/// ## Method Call Operands
#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct CallOperands {
    pub function_operand: ValueId,
    pub params: CallParams,
}

impl CallOperands {
    pub(crate) fn new(
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

impl types::Inference for CallOperands {
    fn infer_types(&mut self, ctx: &mut types::inference::Ctx) -> types::inference::Output {
        let Some(ret_val_id) = self.params.return_value_to else {
            return types::inference::Output::Continue(
                types::inference::ContinueReason::AlreadyDone,
            );
        };

        ctx.check_inferee_type(ret_val_id)?;

        let called_function = self.function_operand;
        match ctx.type_for_value(called_function)? {
            types::Syntactic::Function { args: _, ret_ty } => ctx
                .assign_type_to_inferee(ret_val_id, *ret_ty.clone())
                .unwrap(),
            _ => {
                unreachable!("Function operand with non-function type");
            }
        }
    }
}

impl Display for CallOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}{}", self.function_operand, self.params)
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct LoadOperands {
    pub pointer: ValueId,
    pub destination: ValueId,
}

impl types::Inference for LoadOperands {
    fn infer_types(&mut self, _ctx: &mut types::inference::Ctx) -> types::inference::Output {
        unimplemented!();
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct StoreOperands {
    pub pointer: ValueId,
    pub source: ValueId,
}

impl types::Inference for StoreOperands {
    fn infer_types(&mut self, _ctx: &mut types::inference::Ctx) -> types::inference::Output {
        unimplemented!();
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct FieldAddressOperands {
    pub receiver: ValueId,
    pub offset: usize,
    pub destination: ValueId,
}

impl types::Inference for FieldAddressOperands {
    fn infer_types(&mut self, _ctx: &mut types::inference::Ctx) -> types::inference::Output {
        unimplemented!();
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub(crate) struct SwitchOperands {
    pub scrutinee: ValueId,
    pub default_label: usize,
    pub cases: Vec<(ValueId, usize)>,
}

impl types::Inference for SwitchOperands {
    fn infer_types(&mut self, _ctx: &mut types::inference::Ctx) -> types::inference::Output {
        unimplemented!();
    }
}

#[cfg(test)]
mod tests {}

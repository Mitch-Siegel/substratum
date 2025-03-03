use std::fmt::Display;

use serde::Serialize;

use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{linearizer, symtab::Variable, types::Type},
};

#[derive(Clone, Debug, Serialize)]
pub struct SsaName {
    base_name: String,
    ssa_number: usize,
}
impl Display for SsaName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_string())
    }
}
impl SsaName {
    pub fn from_string(base_name: String) -> Self {
        Self {
            base_name,
            ssa_number: 0,
        }
    }

    pub fn to_string(&self) -> String {
        format!("{}.{}", self.base_name, self.ssa_number)
    }
}

#[derive(Clone, Debug, Serialize)]
pub enum Operand<T>
where
    T: std::fmt::Display,
{
    Variable(T),
    Temporary(T),
    UnsignedDecimalConstant(usize),
}

impl<T> Display for Operand<T>
where
    T: std::fmt::Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Variable(name) => {
                write!(f, "[V {}]", name)
            }
            Self::Temporary(name) => {
                write!(f, "[T {}]", name)
            }
            Self::UnsignedDecimalConstant(value) => {
                write!(f, "[C {}]", value)
            }
        }
    }
}

pub type GenericOperand<T> = Operand<T>;
pub type BasicOperand = Operand<String>;
pub type SsaOperand = Operand<SsaName>;
impl BasicOperand {
    pub fn to_ssa(self) -> SsaOperand {
        match self {
            Operand::Variable(name) => SsaOperand::Variable(SsaName::from_string(name)),
            Operand::Temporary(name) => SsaOperand::Temporary(SsaName::from_string(name)),
            Operand::UnsignedDecimalConstant(value) => SsaOperand::UnsignedDecimalConstant(value),
        }
    }
}

impl BasicOperand {
    pub fn new_as_variable(identifier: String) -> Self {
        Operand::Variable(identifier)
    }

    pub fn new_as_temporary(identifier: String) -> Self {
        Operand::Temporary(identifier)
    }

    pub fn new_as_unsigned_decimal_constant(constant: usize) -> Self {
        Operand::UnsignedDecimalConstant(constant)
    }

    pub fn type_(&self, context: &linearizer::walkcontext::WalkContext) -> Type {
        match self {
            Operand::Variable(name) => context
                .lookup_variable_by_name(name)
                .expect(format!("Use of undeclared variable {}", name).as_str())
                .type_(),
            Operand::Temporary(name) => context
                .lookup_variable_by_name(name)
                .expect(format!("Use of undeclared variable {}", name).as_str())
                .type_()
                .clone(),
            Operand::UnsignedDecimalConstant(value) => {
                if *value > (u32::MAX as usize) {
                    Type::new_u64(0)
                } else if *value > (u16::MAX as usize) {
                    Type::new_u32(0)
                } else if *value > (u8::MAX as usize) {
                    Type::new_u16(0)
                } else {
                    Type::new_u8(0)
                }
            }
        }
    }
}

impl SsaOperand {
    pub fn assign_ssa_number(&mut self, number: usize) {
        match self {
            Operand::Variable(variable) => variable.ssa_number = number,
            Operand::Temporary(temporary) => temporary.ssa_number = number,
            Operand::UnsignedDecimalConstant(_) => {
                panic!("assign_ssa_number called on Operand::UnsignedDecimalConstant")
            }
        }
    }
}

/*
 groupings of operands
*/

#[derive(Debug, Serialize)]
pub struct DualSourceOperands<T>
where
    T: std::fmt::Display,
{
    pub a: Operand<T>,
    pub b: Operand<T>,
}

impl<T> DualSourceOperands<T>
where
    T: std::fmt::Display,
{
    pub fn from(a: Operand<T>, b: Operand<T>) -> Self {
        DualSourceOperands { a, b }
    }
}

impl DualSourceOperands<String> {
    pub fn to_ssa(self) -> DualSourceOperands<SsaName> {
        DualSourceOperands::<SsaName> {
            a: self.a.to_ssa(),
            b: self.b.to_ssa(),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct BinaryArithmeticOperands<T>
where
    T: std::fmt::Display,
{
    pub destination: Operand<T>,
    pub sources: DualSourceOperands<T>,
}

impl<T> BinaryArithmeticOperands<T>
where
    T: std::fmt::Display,
{
    pub fn from(destination: Operand<T>, source_a: Operand<T>, source_b: Operand<T>) -> Self {
        BinaryArithmeticOperands {
            destination,
            sources: DualSourceOperands::from(source_a, source_b),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct SourceDestOperands<T>
where
    T: std::fmt::Display,
{
    pub destination: Operand<T>,
    pub source: Operand<T>,
}

#[derive(Debug, Serialize)]
pub enum JumpCondition<T>
where
    T: std::fmt::Display,
{
    Unconditional,
    Eq(DualSourceOperands<T>),
    NE(DualSourceOperands<T>),
    G(DualSourceOperands<T>),
    L(DualSourceOperands<T>),
    GE(DualSourceOperands<T>),
    LE(DualSourceOperands<T>),
}

impl<T> Display for JumpCondition<T>
where
    T: std::fmt::Display,
{
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
            Self::L(operands) => {
                write!(f, "jl({}, {})", operands.a, operands.b)
            }
            Self::G(operands) => {
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

impl JumpCondition<String> {
    pub fn to_ssa(self) -> JumpCondition<SsaName> {
        match self {
            JumpCondition::Unconditional => JumpCondition::Unconditional,
            JumpCondition::Eq(dual_source_operands) => {
                JumpCondition::Eq(dual_source_operands.to_ssa())
            }
            JumpCondition::NE(dual_source_operands) => {
                JumpCondition::NE(dual_source_operands.to_ssa())
            }
            JumpCondition::G(dual_source_operands) => {
                JumpCondition::G(dual_source_operands.to_ssa())
            }
            JumpCondition::L(dual_source_operands) => {
                JumpCondition::L(dual_source_operands.to_ssa())
            }
            JumpCondition::GE(dual_source_operands) => {
                JumpCondition::GE(dual_source_operands.to_ssa())
            }
            JumpCondition::LE(dual_source_operands) => {
                JumpCondition::LE(dual_source_operands.to_ssa())
            }
        }
    }
}

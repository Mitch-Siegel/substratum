use std::fmt::Display;

use serde::Serialize;

use crate::midend::{types::Type, WalkContext};

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
    pub fn to_string(&self) -> String {
        format!("{}.{}", self.base_name, self.ssa_number)
    }
}

#[derive(Clone, Debug, Serialize)]
pub enum IROperand<T>
where
    T: std::fmt::Display,
{
    Variable(T),
    Temporary(T),
    UnsignedDecimalConstant(usize),
}

impl<T> Display for IROperand<T>
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

pub type GenericOperand<T> = IROperand<T>;
pub type BasicOperand = IROperand<String>;
pub type SsaOperand = IROperand<SsaName>;

impl BasicOperand {
    pub fn new_as_variable(identifier: String) -> Self {
        IROperand::Variable(identifier)
    }

    pub fn new_as_temporary(identifier: String) -> Self {
        IROperand::Temporary(identifier)
    }

    pub fn new_as_unsigned_decimal_constant(constant: usize) -> Self {
        IROperand::UnsignedDecimalConstant(constant)
    }

    pub fn type_(&self, context: &WalkContext) -> Type {
        match self {
            IROperand::Variable(name) => context
                .lookup_variable_by_name(name)
                .expect(format!("Use of undeclared variable {}", name).as_str())
                .type_(),
            IROperand::Temporary(name) => context
                .lookup_variable_by_name(name)
                .expect(format!("Use of undeclared variable {}", name).as_str())
                .type_()
                .clone(),
            IROperand::UnsignedDecimalConstant(value) => {
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

/*
 groupings of operands
*/

#[derive(Debug, Serialize)]
pub struct DualSourceOperands<T>
where
    T: std::fmt::Display,
{
    pub a: IROperand<T>,
    pub b: IROperand<T>,
}

impl<T> DualSourceOperands<T>
where
    T: std::fmt::Display,
{
    pub fn from(a: IROperand<T>, b: IROperand<T>) -> Self {
        DualSourceOperands { a, b }
    }
}

#[derive(Debug, Serialize)]
pub struct BinaryArithmeticOperands<T>
where
    T: std::fmt::Display,
{
    pub destination: IROperand<T>,
    pub sources: DualSourceOperands<T>,
}

impl<T> BinaryArithmeticOperands<T>
where
    T: std::fmt::Display,
{
    pub fn from(destination: IROperand<T>, source_a: IROperand<T>, source_b: IROperand<T>) -> Self {
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
    pub destination: IROperand<T>,
    pub source: IROperand<T>,
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

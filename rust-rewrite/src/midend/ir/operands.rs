use std::fmt::Display;

use serde::Serialize;

use crate::{
    frontend::sourceloc::SourceLoc,
    midend::{linearizer, symtab::Variable, types::Type},
};

#[derive(Clone, Debug, Serialize)]
pub struct NamedOperand {
    base_name: String,
    ssa_number: Option<usize>,
}
impl Display for NamedOperand {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_string())
    }
}
impl NamedOperand {
    pub fn new_basic(base_name: String) -> Self {
        Self {
            base_name,
            ssa_number: None,
        }
    }

    pub fn name(&self) -> String {
        match self.ssa_number {
            Some(number) => {
                format!("{}.{}", self.base_name, number)
            }
            None => self.base_name.clone(),
        }
    }
}

#[derive(Clone, Debug, Serialize)]
pub enum Operand {
    Variable(NamedOperand),
    Temporary(NamedOperand),
    UnsignedDecimalConstant(usize),
}

impl Display for Operand {
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

impl Operand {
    pub fn new_as_variable(identifier: String) -> Self {
        Operand::Variable(NamedOperand::new_basic(identifier))
    }

    pub fn new_as_temporary(identifier: String) -> Self {
        Operand::Temporary(NamedOperand::new_basic(identifier))
    }

    pub fn new_as_unsigned_decimal_constant(constant: usize) -> Self {
        Operand::UnsignedDecimalConstant(constant)
    }

    pub fn type_(&self, context: &linearizer::walkcontext::WalkContext) -> Type {
        match self {
            Operand::Variable(operand) => context
                .lookup_variable_by_name(&operand.name())
                .expect(format!("Use of undeclared variable {}", operand.name()).as_str())
                .type_(),
            Operand::Temporary(operand) => context
                .lookup_variable_by_name(&operand.name())
                .expect(format!("Use of undeclared variable {}", operand.name()).as_str())
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

/*
 groupings of operands
*/

#[derive(Debug, Serialize)]
pub struct DualSourceOperands {
    pub a: Operand,
    pub b: Operand,
}

impl DualSourceOperands {
    pub fn from(a: Operand, b: Operand) -> Self {
        DualSourceOperands { a, b }
    }
}

#[derive(Debug, Serialize)]
pub struct BinaryArithmeticOperands {
    pub destination: Operand,
    pub sources: DualSourceOperands,
}

impl BinaryArithmeticOperands {
    pub fn from(destination: Operand, source_a: Operand, source_b: Operand) -> Self {
        BinaryArithmeticOperands {
            destination,
            sources: DualSourceOperands::from(source_a, source_b),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct SourceDestOperands {
    pub destination: Operand,
    pub source: Operand,
}

#[derive(Debug, Serialize)]
pub enum JumpCondition {
    Unconditional,
    Eq(DualSourceOperands),
    NE(DualSourceOperands),
    G(DualSourceOperands),
    L(DualSourceOperands),
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

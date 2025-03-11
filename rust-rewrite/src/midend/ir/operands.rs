use std::fmt::Display;

use serde::Serialize;

use crate::midend::{linearizer, types::Type};

#[derive(Clone, Debug, Serialize, Hash)]
pub struct NamedOperand {
    pub base_name: String,
    pub ssa_number: Option<usize>,
}

impl PartialEq for NamedOperand {
    fn eq(&self, other: &Self) -> bool {
        (self.base_name == other.base_name) && (self.ssa_number == other.ssa_number)
    }
}

impl Eq for NamedOperand {}

impl PartialOrd for NamedOperand {
    fn partial_cmp(&self, other: &Self) -> std::option::Option<std::cmp::Ordering> {
        Some(
            self.base_name
                .cmp(&other.base_name)
                .then(match self.ssa_number {
                    Some(self_ssa_number) => match other.ssa_number {
                        Some(other_ssa_number) => self_ssa_number.cmp(&other_ssa_number),
                        None => std::cmp::Ordering::Greater,
                    },
                    None => match other.ssa_number {
                        Some(_) => std::cmp::Ordering::Less,
                        None => std::cmp::Ordering::Equal,
                    },
                }),
        )
    }
}

impl Ord for NamedOperand {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.partial_cmp(other).unwrap()
    }
}

impl Display for NamedOperand {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self.ssa_number {
            Some(number) => {
                write!(f, "{}.{}", self.base_name, number)
            }
            None => write!(f, "{}", self.base_name),
        }
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

#[derive(Clone, Debug, Serialize, Hash)]
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

impl PartialEq for Operand {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Variable(var_1), Self::Variable(var_2)) => var_1 == var_2,
            (Self::Temporary(temp_1), Self::Temporary(temp_2)) => temp_1 == temp_2,
            (
                Self::UnsignedDecimalConstant(unsigned_decimal_constant_1),
                Self::UnsignedDecimalConstant(unsigned_decimal_constant_2),
            ) => unsigned_decimal_constant_1 == unsigned_decimal_constant_2,
            _ => false,
        }
    }
}

impl Eq for Operand {}

impl PartialOrd for Operand {
    fn partial_cmp(&self, other: &Self) -> std::option::Option<std::cmp::Ordering> {
        match (self, other) {
            (Operand::Variable(var_self), Operand::Variable(var_other)) => {
                Some(var_self.cmp(var_other))
            }
            (Operand::Temporary(temp_self), Operand::Temporary(temp_other)) => {
                Some(temp_self.cmp(temp_other))
            }
            (
                Operand::UnsignedDecimalConstant(value_self),
                Operand::UnsignedDecimalConstant(value_other),
            ) => Some(value_self.cmp(value_other)),
            (_, _) => None,
        }
    }
}

impl Ord for Operand {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        let partial_result = self.partial_cmp(other);

        match partial_result {
            Some(ordering) => ordering,
            None => match (self, other) {
                (Operand::Variable(var_self), Operand::Temporary(temp_other)) => {
                    var_self.cmp(temp_other)
                }
                (Operand::Variable(_), Operand::UnsignedDecimalConstant(_)) => {
                    std::cmp::Ordering::Greater
                }
                (Operand::Temporary(var_self), Operand::Variable(temp_other)) => {
                    var_self.cmp(temp_other)
                }
                (Operand::Temporary(_), Operand::UnsignedDecimalConstant(_)) => {
                    std::cmp::Ordering::Greater
                }
                (Operand::UnsignedDecimalConstant(_), Operand::Variable(_)) => {
                    std::cmp::Ordering::Less
                }
                (Operand::UnsignedDecimalConstant(_), Operand::Temporary(_)) => {
                    std::cmp::Ordering::Less
                }

                (Operand::Variable(_), Operand::Variable(_))
                | (Operand::Temporary(_), Operand::Temporary(_))
                | (Operand::UnsignedDecimalConstant(_), Operand::UnsignedDecimalConstant(_)) => {
                    panic!("Non-covered case in Operand::cmp")
                }
            },
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

#[derive(Debug, Serialize, Clone)]
pub struct DualSourceOperands {
    pub a: Operand,
    pub b: Operand,
}

impl DualSourceOperands {
    pub fn from(a: Operand, b: Operand) -> Self {
        DualSourceOperands { a, b }
    }
}

#[derive(Debug, Serialize, Clone)]
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

#[derive(Debug, Serialize, Clone)]
pub struct SourceDestOperands {
    pub destination: Operand,
    pub source: Operand,
}

#[derive(Debug, Serialize, Clone)]
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

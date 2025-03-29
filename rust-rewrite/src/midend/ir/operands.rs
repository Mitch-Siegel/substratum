use std::fmt::Display;

use serde::Serialize;

use crate::midend::{linearizer, types::Type};

#[derive(Clone, Debug, Serialize, Hash)]
pub struct OperandName {
    pub base_name: String,
    pub ssa_number: Option<usize>,
}

impl PartialEq for OperandName {
    fn eq(&self, other: &Self) -> bool {
        (self.base_name == other.base_name) && (self.ssa_number == other.ssa_number)
    }
}

impl Eq for OperandName {}

impl PartialOrd for OperandName {
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

impl Ord for OperandName {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.partial_cmp(other).unwrap()
    }
}

impl Display for OperandName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self.ssa_number {
            Some(number) => {
                write!(f, "{}.{}", self.base_name, number)
            }
            None => write!(f, "{}", self.base_name),
        }
    }
}

impl OperandName {
    pub fn new_basic(base_name: String) -> Self {
        Self {
            base_name,
            ssa_number: None,
        }
    }

    fn new_ssa(base_name: String, ssa_number: usize) -> Self {
        Self {
            base_name,
            ssa_number: Some(ssa_number),
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

    pub fn into_non_ssa(mut self) -> Self {
        self.ssa_number = None;
        self
    }
}

#[derive(Clone, Debug, Serialize, Hash)]
pub enum Operand {
    Variable(OperandName),
    Temporary(OperandName),
    UnsignedDecimalConstant(usize),
}

impl Display for Operand {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Variable(name) => {
                // write!(f, "[V {}]", name)
                write!(f, "{}", name)
            }
            Self::Temporary(name) => {
                // write!(f, "[T {}]", name)
                write!(f, "{}", name)
            }
            Self::UnsignedDecimalConstant(value) => {
                // write!(f, "[C {}]", value)
                write!(f, "{}", value)
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
        Operand::Variable(OperandName::new_basic(identifier))
    }

    pub fn new_as_temporary(identifier: String) -> Self {
        Operand::Temporary(OperandName::new_basic(identifier))
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

    pub fn get_name(&self) -> Option<&OperandName> {
        match self {
            Operand::Variable(operand_name) => Some(operand_name),
            Operand::Temporary(operand_name) => Some(operand_name),
            Operand::UnsignedDecimalConstant(_) => None,
        }
    }

    pub fn get_name_mut(&mut self) -> Option<&mut OperandName> {
        match self {
            Operand::Variable(operand_name) => Some(operand_name),
            Operand::Temporary(operand_name) => Some(operand_name),
            Operand::UnsignedDecimalConstant(_) => None,
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
    pub fn new(a: Operand, b: Operand) -> Self {
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
            sources: DualSourceOperands::new(source_a, source_b),
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

mod tests {
    use crate::midend::ir::OperandName;

    #[test]
    fn operand_name_ord() {
        // non-ssa operand names
        assert_eq!(
            OperandName::new_basic("a".into()),
            OperandName::new_basic("a".into())
        );
        assert_ne!(
            OperandName::new_basic("a".into()),
            OperandName::new_basic("b".into())
        );

        assert!(OperandName::new_basic("a".into()) < OperandName::new_basic("b".into()));

        // ssa operand names
        assert_eq!(
            OperandName::new_ssa("a".into(), 4),
            OperandName::new_ssa("a".into(), 4)
        );
        assert!(OperandName::new_ssa("a".into(), 4) < OperandName::new_ssa("a".into(), 5));
        assert!(OperandName::new_ssa("a".into(), 4) < OperandName::new_ssa("b".into(), 4));

        // mixed ssa and non-ssa
        assert!(OperandName::new_basic("a".into()) < OperandName::new_ssa("a".into(), 1));
        assert!(OperandName::new_ssa("a".into(), 1) > OperandName::new_basic("a".into()));
    }

    #[test]
    fn operand_name_into_non_ssa() {
        assert_eq!(
            OperandName::new_basic("a".into()).into_non_ssa(),
            OperandName::new_basic("a".into())
        );

        assert_eq!(
            OperandName::new_ssa("a".into(), 123).into_non_ssa(),
            OperandName::new_basic("a".into())
        );
    }
}

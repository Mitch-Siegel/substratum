use std::fmt::Display;

use serde::Serialize;

use crate::midend::{
    symtab::{self},
    types,
};

pub type ValueId = usize;

#[derive(Clone, Debug, Serialize, PartialEq, Eq, Hash)]
pub enum Operand {
    Variable(ValueId),
    Temporary(ValueId),
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
                (Operand::Variable(_), Operand::Temporary(_)) => std::cmp::Ordering::Greater,
                (Operand::Variable(_), Operand::UnsignedDecimalConstant(_)) => {
                    std::cmp::Ordering::Greater
                }
                (Operand::Temporary(_), Operand::Variable(_)) => std::cmp::Ordering::Less,
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
    pub fn new_as_variable(id: ValueId) -> Self {
        Operand::Variable(id)
    }

    pub fn new_as_temporary(id: ValueId) -> Self {
        Operand::Temporary(id)
    }

    pub fn new_as_unsigned_decimal_constant(constant: usize) -> Self {
        Operand::UnsignedDecimalConstant(constant)
    }

    pub fn value_id(&self) -> Option<&ValueId> {
        match self {
            Operand::Variable(operand_name) => Some(operand_name),
            Operand::Temporary(operand_name) => Some(operand_name),
            Operand::UnsignedDecimalConstant(_) => None,
        }
    }

    pub fn value_id_mut(&mut self) -> Option<&mut ValueId> {
        match self {
            Operand::Variable(value_id) => Some(value_id),
            Operand::Temporary(value_id) => Some(value_id),
            Operand::UnsignedDecimalConstant(_) => None,
        }
    }
}

/*
 groupings of operands
*/

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct DualSourceOperands {
    pub a: Operand,
    pub b: Operand,
}

impl DualSourceOperands {
    pub fn new(a: Operand, b: Operand) -> Self {
        DualSourceOperands { a, b }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
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

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct SourceDestOperands {
    pub destination: Operand,
    pub source: Operand,
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

pub type OrderedArgumentList = Vec<Operand>;

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
    pub return_value_to: Option<Operand>,
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
        return_value_to: Option<Operand>,
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
    pub receiver: Operand,
    pub call: FunctionCallOperands,
}

impl Display for MethodCallOperands {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.receiver, self.call)
    }
}

impl MethodCallOperands {
    pub fn new(
        receiver: Operand,
        method_name: &str,
        arguments: OrderedArgumentList,
        return_value_to: Option<Operand>,
    ) -> Self {
        Self {
            receiver,
            call: FunctionCallOperands::new(method_name, arguments, return_value_to),
        }
    }
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct FieldReadOperands {
    pub receiver: Operand,
    pub field_name: String,
    pub destination: Operand,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct FieldWriteOperands {
    pub receiver: Operand,
    pub field_name: String,
    pub source: Operand,
}

#[cfg(test)]
mod tests {
    use std::cmp::Ordering;

    use crate::midend::ir::*;

    #[test]
    fn operand_name_ord() {
        // non-ssa operand names
        assert_eq!(
            ValueId::new_basic("a".into()).cmp(&ValueId::new_basic("a".into())),
            Ordering::Equal
        );
        assert_eq!(
            ValueId::new_basic("a".into()).cmp(&ValueId::new_basic("b".into())),
            Ordering::Less
        );

        // ssa operand names
        assert_eq!(
            ValueId::new_ssa("a".into(), 4).cmp(&ValueId::new_ssa("a".into(), 4)),
            Ordering::Equal
        );
        assert_eq!(
            ValueId::new_ssa("a".into(), 4).cmp(&ValueId::new_ssa("a".into(), 5)),
            Ordering::Less
        );
        assert_eq!(
            ValueId::new_ssa("a".into(), 4).cmp(&ValueId::new_ssa("b".into(), 4)),
            Ordering::Less
        );

        // mixed ssa and non-ssa
        assert_eq!(
            ValueId::new_basic("a".into()).cmp(&ValueId::new_ssa("a".into(), 1)),
            Ordering::Less
        );
        assert_eq!(
            ValueId::new_ssa("a".into(), 1).cmp(&ValueId::new_basic("a".into())),
            Ordering::Greater
        );
    }

    #[test]
    fn operand_name_into_non_ssa() {
        assert_eq!(
            ValueId::new_basic("a".into()).into_non_ssa(),
            ValueId::new_basic("a".into())
        );

        assert_eq!(
            ValueId::new_ssa("a".into(), 123).into_non_ssa(),
            ValueId::new_basic("a".into())
        );
    }

    #[test]
    fn operand_eq() {
        // variable against other types
        assert_eq!(
            Operand::new_as_variable("asdf".into()),
            Operand::new_as_variable("asdf".into())
        );
        assert_ne!(
            Operand::new_as_variable("asdf".into()),
            Operand::new_as_temporary("asdf".into())
        );
        assert_ne!(
            Operand::new_as_variable("asdf".into()),
            Operand::new_as_unsigned_decimal_constant(12)
        );

        // temporary against other types
        assert_ne!(
            Operand::new_as_temporary("asdf".into()),
            Operand::new_as_variable("asdf".into())
        );
        assert_eq!(
            Operand::new_as_temporary("asdf".into()),
            Operand::new_as_temporary("asdf".into())
        );
        assert_ne!(
            Operand::new_as_temporary("asdf".into()),
            Operand::new_as_unsigned_decimal_constant(12)
        );

        // unsigned decimal constant against other types
        assert_ne!(
            Operand::new_as_unsigned_decimal_constant(12),
            Operand::new_as_variable("asdf".into())
        );
        assert_ne!(
            Operand::new_as_unsigned_decimal_constant(12),
            Operand::new_as_temporary("asdf".into())
        );
        assert_eq!(
            Operand::new_as_unsigned_decimal_constant(12),
            Operand::new_as_unsigned_decimal_constant(12)
        );
    }

    #[test]
    fn operand_get_name() {
        assert_eq!(
            Operand::new_as_variable("asdf".into()).value_id(),
            Some(&ValueId::new_basic("asdf".into()))
        );
        assert_eq!(
            Operand::new_as_temporary("asdf".into()).value_id(),
            Some(&ValueId::new_basic("asdf".into()))
        );
        assert_eq!(
            Operand::new_as_unsigned_decimal_constant(12).value_id(),
            None
        );
    }

    #[test]
    fn operand_get_name_mut() {
        assert_eq!(
            Operand::new_as_variable("asdf".into()).value_id_mut(),
            Some(&mut ValueId::new_basic("asdf".into()))
        );
        assert_eq!(
            Operand::new_as_temporary("asdf".into()).value_id_mut(),
            Some(&mut ValueId::new_basic("asdf".into()))
        );
        assert_eq!(
            Operand::new_as_unsigned_decimal_constant(12).value_id_mut(),
            None
        );
    }
}

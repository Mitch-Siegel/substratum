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
pub struct FieldReadOperands {
    pub receiver: ValueId,
    pub field_name: String,
    pub destination: ValueId,
}

#[derive(Debug, Serialize, PartialEq, Eq, Clone)]
pub struct FieldWriteOperands {
    pub receiver: ValueId,
    pub field_name: String,
    pub source: ValueId,
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
            ValueId::new_as_variable("asdf".into()),
            ValueId::new_as_variable("asdf".into())
        );
        assert_ne!(
            ValueId::new_as_variable("asdf".into()),
            ValueId::new_as_temporary("asdf".into())
        );
        assert_ne!(
            ValueId::new_as_variable("asdf".into()),
            ValueId::new_as_unsigned_decimal_constant(12)
        );

        // temporary against other types
        assert_ne!(
            ValueId::new_as_temporary("asdf".into()),
            ValueId::new_as_variable("asdf".into())
        );
        assert_eq!(
            ValueId::new_as_temporary("asdf".into()),
            ValueId::new_as_temporary("asdf".into())
        );
        assert_ne!(
            ValueId::new_as_temporary("asdf".into()),
            ValueId::new_as_unsigned_decimal_constant(12)
        );

        // unsigned decimal constant against other types
        assert_ne!(
            ValueId::new_as_unsigned_decimal_constant(12),
            ValueId::new_as_variable("asdf".into())
        );
        assert_ne!(
            ValueId::new_as_unsigned_decimal_constant(12),
            ValueId::new_as_temporary("asdf".into())
        );
        assert_eq!(
            ValueId::new_as_unsigned_decimal_constant(12),
            ValueId::new_as_unsigned_decimal_constant(12)
        );
    }

    #[test]
    fn operand_get_name() {
        assert_eq!(
            ValueId::new_as_variable("asdf".into()).get_name(),
            Some(&ValueId::new_basic("asdf".into()))
        );
        assert_eq!(
            ValueId::new_as_temporary("asdf".into()).get_name(),
            Some(&ValueId::new_basic("asdf".into()))
        );
        assert_eq!(
            ValueId::new_as_unsigned_decimal_constant(12).get_name(),
            None
        );
    }

    #[test]
    fn operand_get_name_mut() {
        assert_eq!(
            ValueId::new_as_variable("asdf".into()).get_name_mut(),
            Some(&mut ValueId::new_basic("asdf".into()))
        );
        assert_eq!(
            ValueId::new_as_temporary("asdf".into()).get_name_mut(),
            Some(&mut ValueId::new_basic("asdf".into()))
        );
        assert_eq!(
            ValueId::new_as_unsigned_decimal_constant(12).get_name_mut(),
            None
        );
    }
}

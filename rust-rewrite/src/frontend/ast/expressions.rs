use crate::{frontend::ast::*, midend};

pub mod arithmetic;
pub mod assignment;
pub mod block_expression;
pub mod calls;
pub mod field;
pub mod if_expression;
pub mod match_expression;
pub mod while_expression;

pub use arithmetic::{ArithmeticExpressionTree, ComparisonExpressionTree};
pub use assignment::AssignmentTree;
pub use block_expression::BlockExpressionTree;
pub use calls::MethodCallExpressionTree;
pub use field::FieldExpressionTree;
pub use if_expression::IfExpressionTree;
pub use match_expression::MatchExpressionTree;
pub use while_expression::WhileExpressionTree;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Expression {
    SelfLower,
    Identifier(String),
    UnsignedDecimalConstant(usize),
    Arithmetic(ArithmeticExpressionTree),
    Comparison(ComparisonExpressionTree),
    Assignment(AssignmentTree),
    If(Box<IfExpressionTree>),
    Match(Box<MatchExpressionTree>),
    While(Box<WhileExpressionTree>),
    FieldExpression(Box<FieldExpressionTree>),
    MethodCall(Box<MethodCallExpressionTree>),
}

impl Display for Expression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::SelfLower => write!(f, "self"),
            Self::Identifier(identifier) => write!(f, "{}", identifier),
            Self::UnsignedDecimalConstant(constant) => write!(f, "{}", constant),
            Self::Arithmetic(arithmetic_expression) => write!(f, "{}", arithmetic_expression),
            Self::Comparison(comparison_expression) => write!(f, "{}", comparison_expression),
            Self::Assignment(assignment_expression) => write!(f, "{}", assignment_expression),
            Self::If(if_expression) => write!(f, "{}", if_expression),
            Self::Match(match_expression) => write!(f, "{}", match_expression),
            Self::While(while_expression) => write!(f, "{}", while_expression),
            Self::FieldExpression(field_expression) => write!(f, "{}", field_expression),
            Self::MethodCall(method_call) => write!(f, "{}", method_call),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ExpressionTree {
    pub loc: SourceLocWithMod,
    pub expression: Expression,
}
impl ExpressionTree {
    pub fn new(loc: SourceLocWithMod, expression: Expression) -> Self {
        Self { loc, expression }
    }
}
impl Display for ExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.expression)
    }
}

impl midend::linearizer::ValueWalk for ExpressionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut midend::linearizer::FunctionWalkContext) -> midend::ir::ValueId {
        match self.expression {
            Expression::SelfLower => match context.self_variable() {
                Some(id) => *id,
                None => panic!(
                    "'self' expression is not valid ({}) (defpath {})",
                    self.loc,
                    context.def_path()
                ),
            },
            Expression::Identifier(ident) => {
                let (_, variable_path) = context
                    .lookup_with_path::<midend::symtab::Variable>(&ident)
                    .unwrap();
                *context.value_for_variable(&variable_path)
            }
            Expression::UnsignedDecimalConstant(constant) => {
                *context.value_id_for_constant(constant)
            }
            Expression::Arithmetic(arithmetic_operation) => arithmetic_operation.walk(context),
            Expression::Comparison(comparison_operation) => comparison_operation.walk(context),
            Expression::Assignment(assignment_expression) => assignment_expression.walk(context),
            Expression::If(if_expression) => if_expression.walk(context),
            Expression::Match(match_expression) => match_expression.walk(context),

            Expression::While(while_expression) => while_expression.walk(context),
            Expression::FieldExpression(field_expression) => {
                let (receiver, field) = field_expression.walk(context);
                let (field_type, field_name) = (field.type_.clone(), field.name.clone());
                let field_pointer_temp = context.next_temp(Some(field_type));
                let field_read_line = midend::ir::IrLine::new_get_field_pointer(
                    self.loc,
                    receiver.into(),
                    field_name,
                    field_pointer_temp.clone(),
                );
                context
                    .append_statement_to_current_block(field_read_line)
                    .unwrap();
                field_pointer_temp
            }
            Expression::MethodCall(method_call) => method_call.walk(context),
        }
    }
}

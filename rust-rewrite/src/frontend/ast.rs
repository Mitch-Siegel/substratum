use crate::{frontend::sourceloc::SourceLoc, midend};
use std::fmt::Display;

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum TranslationUnit {
    FunctionDeclaration(FunctionDeclarationTree),
    FunctionDefinition(FunctionDefinitionTree),
    StructDefinition(StructDefinitionTree),
}

impl Display for TranslationUnit {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::FunctionDeclaration(function_declaration) => {
                write!(f, "Function Declaration: {}", function_declaration)
            }
            Self::FunctionDefinition(function_definition) => {
                write!(f, "Function Definition: {}", function_definition)
            }
            Self::StructDefinition(struct_definition) => {
                write!(f, "Struct Definition: {}", struct_definition)
            }
        }
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct TranslationUnitTree {
    pub loc: SourceLoc,
    pub contents: TranslationUnit,
}
impl Display for TranslationUnitTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Translation Unit: {}", self.contents)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDeclarationTree {
    pub loc: SourceLoc,
    pub name: String,
    pub arguments: Vec<VariableDeclarationTree>,
    pub return_type: Option<TypenameTree>,
}
impl Display for FunctionDeclarationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut arg_string = String::from("");
        for argument in &self.arguments {
            arg_string.push_str(format!("{}\n", argument).as_str());
        }

        match &self.return_type {
            Some(typename_tree) => write!(
                f,
                "Function Declaration: {}({})->{}",
                self.name, arg_string, typename_tree
            ),
            None => write!(f, "Function Declaration: {}({})", self.name, arg_string),
        }
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDefinitionTree {
    pub prototype: FunctionDeclarationTree,
    pub body: CompoundExpressionTree,
}
impl Display for FunctionDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Function Definition: {}, {}", self.prototype, self.body)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StructDefinitionTree {
    pub loc: SourceLoc,
    pub name: String,
    pub fields: Vec<VariableDeclarationTree>,
}
impl Display for StructDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut fields = String::new();
        for field in &self.fields {
            fields += &field.to_string();
            fields += " ";
        }
        write!(f, "Struct Definition: {}: {}", self.name, fields)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct CompoundExpressionTree {
    pub loc: SourceLoc,
    pub statements: Vec<StatementTree>,
}
impl Display for CompoundExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut statement_string = String::from("");
        for statement in &self.statements {
            statement_string.push_str(format!("{}\n", statement).as_str());
        }
        write!(f, "Compound Expression: {}", statement_string)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct IfExpressionTree {
    pub loc: SourceLoc,
    pub condition: ExpressionTree,
    pub true_block: CompoundExpressionTree,
    pub false_block: Option<CompoundExpressionTree>,
}
impl Display for IfExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.false_block {
            Some(false_block) => write!(
                f,
                "if {}\n\t{{{}}} else {{{}}}",
                self.condition, self.true_block, false_block
            ),
            None => write!(f, "if {}\n\t{{{}}}", self.condition, self.true_block),
        }
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct WhileLoopTree {
    pub loc: SourceLoc,
    pub condition: ExpressionTree,
    pub body: CompoundExpressionTree,
}
impl Display for WhileLoopTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "while ({}) {}", self.condition, self.body)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Statement {
    VariableDeclaration(VariableDeclarationTree),
    Assignment(AssignmentTree),
    Expression(ExpressionTree),
}

impl Display for Statement {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::VariableDeclaration(variable_declaration) => {
                write!(f, "{}", variable_declaration)
            }
            Self::Assignment(assignment) => {
                write!(f, "{}", assignment)
            }
            Self::Expression(expression) => {
                write!(f, "{}", expression)
            }
        }
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StatementTree {
    pub loc: SourceLoc,
    pub statement: Statement,
}
impl Display for StatementTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.statement)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct VariableDeclarationTree {
    pub loc: SourceLoc,
    pub name: String,
    pub typename: TypenameTree,
}
impl Display for VariableDeclarationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.typename)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct AssignmentTree {
    pub loc: SourceLoc,
    pub assignee: ExpressionTree,
    pub value: ExpressionTree,
}
impl Display for AssignmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.assignee, self.value)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ArithmeticDualOperands {
    pub e1: Box<ExpressionTree>,
    pub e2: Box<ExpressionTree>,
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum ArithmeticExpressionTree {
    Add(ArithmeticDualOperands),
    Subtract(ArithmeticDualOperands),
    Multiply(ArithmeticDualOperands),
    Divide(ArithmeticDualOperands),
}
impl Display for ArithmeticExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Add(operands) => write!(f, "({} + {})", operands.e1, operands.e2),
            Self::Subtract(operands) => write!(f, "({} - {})", operands.e1, operands.e2),
            Self::Multiply(operands) => write!(f, "({} * {})", operands.e1, operands.e2),
            Self::Divide(operands) => write!(f, "({} / {})", operands.e1, operands.e2),
        }
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum ComparisonExpressionTree {
    LThan(ArithmeticDualOperands),
    GThan(ArithmeticDualOperands),
    LThanE(ArithmeticDualOperands),
    GThanE(ArithmeticDualOperands),
    Equals(ArithmeticDualOperands),
    NotEquals(ArithmeticDualOperands),
}
impl Display for ComparisonExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::LThan(operands) => write!(f, "({} < {})", operands.e1, operands.e2),
            Self::GThan(operands) => write!(f, "({} > {})", operands.e1, operands.e2),
            Self::LThanE(operands) => write!(f, "({} <= {})", operands.e1, operands.e2),
            Self::GThanE(operands) => write!(f, "({} >= {})", operands.e1, operands.e2),
            Self::Equals(operands) => write!(f, "({} == {})", operands.e1, operands.e2),
            Self::NotEquals(operands) => write!(f, "({} != {})", operands.e1, operands.e2),
        }
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Expression {
    Identifier(String),
    UnsignedDecimalConstant(usize),
    Arithmetic(ArithmeticExpressionTree),
    Comparison(ComparisonExpressionTree),
    If(Box<IfExpressionTree>),
}
impl Display for Expression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Identifier(identifier) => write!(f, "{}", identifier),
            Self::UnsignedDecimalConstant(constant) => write!(f, "{}", constant),
            Self::Arithmetic(arithmetic_expression) => write!(f, "{}", arithmetic_expression),
            Self::Comparison(comparison_expression) => write!(f, "{}", comparison_expression),
            Self::If(if_expression) => write!(f, "{}", if_expression),
        }
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ExpressionTree {
    pub loc: SourceLoc,
    pub expression: Expression,
}
impl Display for ExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.expression)
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct TypenameTree {
    pub loc: SourceLoc,
    pub type_: midend::types::Type,
}

impl Display for TypenameTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

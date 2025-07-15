use crate::{frontend::sourceloc::SourceLocWithMod, midend};
use std::fmt::Display;

use name_derive::{NameReflectable, ReflectName};

pub trait AstName {
    fn ast_name(&self) -> String;
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ModuleTree {
    pub module_path: Vec<String>,
    pub name: String,
    pub items: Vec<Item>,
}

impl Display for ModuleTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "Module {}", self.name)?;
        for item in &self.items {
            writeln!(f, " - {}", item)?;
        }
        Ok(())
    }
}

#[derive(Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Item {
    FunctionDeclaration(FunctionDeclarationTree),
    FunctionDefinition(FunctionDefinitionTree),
    StructDefinition(StructDefinitionTree),
    Implementation(ImplementationTree),
    Module(ModuleTree),
}

impl Display for Item {
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
            Self::Implementation(implementation) => {
                write!(f, "Implementation: {}", implementation)
            }
            Self::Module(module) => {
                write!(f, "Module: {}", module)
            }
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamTree {
    pub loc: SourceLocWithMod,
    pub name: String,
}
impl GenericParamTree {
    pub fn new(loc: SourceLocWithMod, name: String) -> Self {
        Self { loc, name }
    }
}
impl Display for GenericParamTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamsListTree {
    pub loc: SourceLocWithMod,
    pub params: Vec<GenericParamTree>,
}

impl GenericParamsListTree {
    pub fn new(loc: SourceLocWithMod, params: Vec<GenericParamTree>) -> Self {
        Self { loc, params }
    }
}
impl Display for GenericParamsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for param in &self.params {
            if !first {
                write!(f, ", ")?;
            } else {
                first = false;
            }

            write!(f, "{}", param)?;
        }

        Ok(())
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDeclarationTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub arguments: Vec<ArgumentDeclarationTree>,
    pub generic_params: Option<GenericParamsListTree>,
    pub return_type: Option<TypeTree>,
}
impl FunctionDeclarationTree {
    pub fn new(
        loc: SourceLocWithMod,
        name: String,
        arguments: Vec<ArgumentDeclarationTree>,
        generic_params: Option<GenericParamsListTree>,
        return_type: Option<TypeTree>,
    ) -> Self {
        Self {
            loc,
            name,
            arguments,
            generic_params,
            return_type,
        }
    }
}
impl Display for FunctionDeclarationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut arg_string = String::from("");
        for argument in &self.arguments {
            arg_string.push_str(format!("{}\n", argument).as_str());
        }

        let generic_params_string = match &self.generic_params {
            Some(params) => String::from(format!("<{}>", params)),
            None => String::new(),
        };

        match &self.return_type {
            Some(typename_tree) => write!(
                f,
                "Function Declaration: {}{}({})->{}",
                self.name, generic_params_string, arg_string, typename_tree
            ),
            None => write!(f, "Function Declaration: {}({})", self.name, arg_string),
        }
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDefinitionTree {
    pub prototype: FunctionDeclarationTree,
    pub body: CompoundExpressionTree,
}
impl FunctionDefinitionTree {
    pub fn new(prototype: FunctionDeclarationTree, body: CompoundExpressionTree) -> Self {
        Self { prototype, body }
    }
}
impl Display for FunctionDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Function Definition: {}, {}", self.prototype, self.body)
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StructFieldTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub type_: TypeTree,
}
impl StructFieldTree {
    pub fn new(loc: SourceLocWithMod, name: String, type_: TypeTree) -> Self {
        Self { loc, name, type_ }
    }
}
impl Display for StructFieldTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_)
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct IdentifierWithGenericsTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub generic_params: Option<GenericParamsListTree>,
}
impl IdentifierWithGenericsTree {
    pub fn new(
        loc: SourceLocWithMod,
        name: String,
        generic_params: Option<GenericParamsListTree>,
    ) -> Self {
        Self {
            loc,
            name,
            generic_params,
        }
    }
}
impl Display for IdentifierWithGenericsTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let generic_params_string = match &self.generic_params {
            Some(params) => String::from(format!("<{}>", params)),
            None => String::new(),
        };

        write!(f, "{}{}", self.name, generic_params_string)
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StructDefinitionTree {
    pub loc: SourceLocWithMod,
    pub name: IdentifierWithGenericsTree,
    pub fields: Vec<StructFieldTree>,
}
impl StructDefinitionTree {
    pub fn new(
        loc: SourceLocWithMod,
        name: IdentifierWithGenericsTree,
        fields: Vec<StructFieldTree>,
    ) -> Self {
        Self { loc, name, fields }
    }
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

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ImplementationTree {
    pub loc: SourceLocWithMod,
    pub generic_params: Option<GenericParamsListTree>,
    pub for_: IdentifierWithGenericsTree,
    pub items: Vec<FunctionDefinitionTree>,
}
impl ImplementationTree {
    pub fn new(
        loc: SourceLocWithMod,
        generic_params: Option<GenericParamsListTree>,
        for_: IdentifierWithGenericsTree,
        items: Vec<FunctionDefinitionTree>,
    ) -> Self {
        Self {
            loc,
            generic_params,
            for_,
            items,
        }
    }
}
impl Display for ImplementationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let generic_params_string = match &self.generic_params {
            Some(params) => String::from(format!("<{}>", params)),
            None => String::new(),
        };

        write!(f, "Impl{} {}", generic_params_string, self.for_).and_then(|_| {
            for item in &self.items {
                write!(f, "{}", item)?
            }
            Ok(())
        })
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct CompoundExpressionTree {
    pub loc: SourceLocWithMod,
    pub statements: Vec<StatementTree>,
}
impl CompoundExpressionTree {
    pub fn new(loc: SourceLocWithMod, statements: Vec<StatementTree>) -> Self {
        Self { loc, statements }
    }
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

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct IfExpressionTree {
    pub loc: SourceLocWithMod,
    pub condition: ExpressionTree,
    pub true_block: CompoundExpressionTree,
    pub false_block: Option<CompoundExpressionTree>,
}
impl IfExpressionTree {
    pub fn new(
        loc: SourceLocWithMod,
        condition: ExpressionTree,
        true_block: CompoundExpressionTree,
        false_block: Option<CompoundExpressionTree>,
    ) -> Self {
        Self {
            loc,
            condition,
            true_block,
            false_block,
        }
    }
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

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct WhileExpressionTree {
    pub loc: SourceLocWithMod,
    pub condition: ExpressionTree,
    pub body: CompoundExpressionTree,
}
impl WhileExpressionTree {
    pub fn new(
        loc: SourceLocWithMod,
        condition: ExpressionTree,
        body: CompoundExpressionTree,
    ) -> Self {
        Self {
            loc,
            condition,
            body,
        }
    }
}
impl Display for WhileExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "while ({}) {}", self.condition, self.body)
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct CallParamsTree {
    pub loc: SourceLocWithMod,
    pub params: Vec<ExpressionTree>,
}
impl CallParamsTree {
    pub fn new(loc: SourceLocWithMod, params: Vec<ExpressionTree>) -> Self {
        Self { loc, params }
    }
}
impl Display for CallParamsTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut params = String::new();
        for p in &self.params {
            if params.len() > 0 {
                params += &", ";
            }
            params += &format!("{}", p);
        }
        write!(f, "{}", params)
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct MethodCallExpressionTree {
    pub loc: SourceLocWithMod,
    pub receiver: ExpressionTree,
    pub called_method: String,
    pub params: CallParamsTree,
}
impl MethodCallExpressionTree {
    pub fn new(
        loc: SourceLocWithMod,
        receiver: ExpressionTree,
        called_method: String,
        params: CallParamsTree,
    ) -> Self {
        Self {
            loc,
            receiver,
            called_method,
            params,
        }
    }
}
impl Display for MethodCallExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}.{}({})",
            self.receiver, self.called_method, self.params
        )
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FieldExpressionTree {
    pub loc: SourceLocWithMod,
    pub receiver: ExpressionTree,
    pub field: String,
}
impl FieldExpressionTree {
    pub fn new(loc: SourceLocWithMod, receiver: ExpressionTree, field: String) -> Self {
        Self {
            loc,
            receiver,
            field,
        }
    }
}
impl Display for FieldExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}", self.receiver, self.field)
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Statement {
    VariableDeclaration(VariableDeclarationTree),
    Expression(ExpressionTree),
}
impl Display for Statement {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::VariableDeclaration(variable_declaration) => {
                write!(f, "{}", variable_declaration)
            }
            Self::Expression(expression) => {
                write!(f, "{}", expression)
            }
        }
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct StatementTree {
    pub loc: SourceLocWithMod,
    pub statement: Statement,
}
impl StatementTree {
    pub fn new(loc: SourceLocWithMod, statement: Statement) -> Self {
        Self { loc, statement }
    }
}
impl Display for StatementTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.statement)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct VariableDeclarationTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub type_: Option<TypeTree>,
    pub mutable: bool,
}
impl VariableDeclarationTree {
    pub fn new(
        loc: SourceLocWithMod,
        name: String,
        type_: Option<TypeTree>,
        mutable: bool,
    ) -> Self {
        Self {
            loc,
            name,
            type_,
            mutable,
        }
    }
}
impl Display for VariableDeclarationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.mutable {
            write!(f, "mut ")?;
        }
        match &self.type_ {
            Some(type_) => write!(f, "{}: {}", self.name, type_),
            None => write!(f, "{}", self.name),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ArgumentDeclarationTree {
    pub loc: SourceLocWithMod,
    pub name: String,
    pub type_: TypeTree,
    pub mutable: bool,
}
impl ArgumentDeclarationTree {
    pub fn new(loc: SourceLocWithMod, name: String, type_: TypeTree, mutable: bool) -> Self {
        Self {
            loc,
            name,
            type_,
            mutable,
        }
    }
}
impl Display for ArgumentDeclarationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.mutable {
            write!(f, "mut ")?;
        }

        write!(f, "{}: {}", self.name, self.type_)
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct AssignmentTree {
    pub loc: SourceLocWithMod,
    pub assignee: Box<ExpressionTree>,
    pub value: Box<ExpressionTree>,
}
impl AssignmentTree {
    pub fn new(loc: SourceLocWithMod, assignee: ExpressionTree, value: ExpressionTree) -> Self {
        Self {
            loc,
            assignee: Box::from(assignee),
            value: Box::from(value),
        }
    }
}
impl Display for AssignmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} = {}", self.assignee, self.value)
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ArithmeticDualOperands {
    pub e1: Box<ExpressionTree>,
    pub e2: Box<ExpressionTree>,
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
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

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
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

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
pub enum Expression {
    Identifier(String),
    UnsignedDecimalConstant(usize),
    Arithmetic(ArithmeticExpressionTree),
    Comparison(ComparisonExpressionTree),
    Assignment(AssignmentTree),
    If(Box<IfExpressionTree>),
    While(Box<WhileExpressionTree>),
    FieldExpression(Box<FieldExpressionTree>),
    MethodCall(Box<MethodCallExpressionTree>),
}
impl Display for Expression {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Identifier(identifier) => write!(f, "{}", identifier),
            Self::UnsignedDecimalConstant(constant) => write!(f, "{}", constant),
            Self::Arithmetic(arithmetic_expression) => write!(f, "{}", arithmetic_expression),
            Self::Comparison(comparison_expression) => write!(f, "{}", comparison_expression),
            Self::Assignment(assignment_expression) => write!(f, "{}", assignment_expression),
            Self::If(if_expression) => write!(f, "{}", if_expression),
            Self::While(while_expression) => write!(f, "{}", while_expression),
            Self::FieldExpression(field_expression) => write!(f, "{}", field_expression),
            Self::MethodCall(method_call) => write!(f, "{}", method_call),
        }
    }
}

#[derive(ReflectName, Debug, PartialEq, serde::Serialize, serde::Deserialize)]
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

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct TypeTree {
    pub loc: SourceLocWithMod,
    pub type_: midend::types::Type,
}
impl TypeTree {
    pub fn new(loc: SourceLocWithMod, type_: midend::types::Type) -> Self {
        Self { loc, type_ }
    }
}
impl Display for TypeTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.type_)
    }
}

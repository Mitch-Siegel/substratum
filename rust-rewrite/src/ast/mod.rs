use crate::lexer::SourceLoc;
use std::fmt::Display;

pub enum TranslationUnit {
    FunctionDeclaration(FunctionDeclarationTree),
    FunctionDefinition(FunctionDefinitionTree),
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
        }
    }
}

pub struct TranslationUnitTree {
    pub loc: SourceLoc,
    pub contents: TranslationUnit,
}
impl Display for TranslationUnitTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Translation Unit: {}", self.contents)
    }
}

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

pub struct FunctionDefinitionTree {
    pub prototype: FunctionDeclarationTree,
    pub body: CompoundStatementTree,
}
impl Display for FunctionDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Function Definition: {}, {}", self.prototype, self.body)
    }
}

pub struct CompoundStatementTree {
    pub loc: SourceLoc,
    pub statements: Vec<StatementTree>,
}
impl Display for CompoundStatementTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut statement_string = String::from("");
        for statement in &self.statements {
            statement_string.push_str(format!("{}\n", statement).as_str());
        }
        write!(f, "Compound Statement: {}", statement_string)
    }
}

pub enum Statement {
    VariableDeclaration(VariableDeclarationTree),
}
impl Display for Statement {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::VariableDeclaration(variable_declaration) => {
                write!(f, "{}", variable_declaration)
            }
        }
    }
}

pub struct StatementTree {
    pub loc: SourceLoc,
    pub statement: Statement,
}
impl Display for StatementTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.statement)
    }
}

pub struct VariableDeclarationTree {
    pub loc: SourceLoc,
    pub name: String,
    pub typename: TypenameTree,
}
impl Display for VariableDeclarationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} {}", self.typename, self.name)
    }
}

pub struct TypenameTree {
    pub loc: SourceLoc,
    pub name: String,
}

impl Display for TypenameTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{
        Ast, IdentifierTree, TypeTree, expressions::BlockExpressionTree,
        generics::OptionalGenericParamsListTree,
    },
    loc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ArgumentDeclarationTree {
    pub name: IdentifierTree,
    pub type_: TypeTree,
    pub mutable: bool,
}

impl Ast for ArgumentDeclarationTree {
    fn span(&self) -> loc::Span {
        self.name.span().merge(&self.type_.span()).unwrap()
    }
}

impl fmt::Display for ArgumentDeclarationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.mutable {
            write!(f, "mut ")?;
        }

        write!(f, "{}: {}", self.name, self.type_)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FunctionDeclarationTree {
    pub(crate) fn_keyword: loc::Span,
    pub name: IdentifierTree,
    pub generic_params: OptionalGenericParamsListTree,
    pub arguments: Vec<ArgumentDeclarationTree>,
    pub args_close_paren: loc::Span,
    pub return_type: Option<TypeTree>,
}

impl Ast for FunctionDeclarationTree {
    fn span(&self) -> loc::Span {
        let mut loc = self.name.span().merge(&self.args_close_paren).unwrap();
        if let Some(return_type) = &self.return_type {
            loc = loc.merge(&return_type.span()).unwrap();
        }
        loc
    }
}

impl fmt::Display for FunctionDeclarationTree {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut arg_string = String::new();
        for argument in &self.arguments {
            arg_string.push_str(format!("{argument}\n").as_str());
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

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FunctionDefinitionTree {
    pub prototype: FunctionDeclarationTree,
    pub body: BlockExpressionTree,
}

// TODO: get this returning the Function rather than inserting automatically
impl Ast for FunctionDefinitionTree {
    fn span(&self) -> loc::Span {
        self.prototype.span().merge(&self.body.span()).unwrap()
    }
}

impl fmt::Display for FunctionDefinitionTree {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Function Definition: {}, {}", self.prototype, self.body)
    }
}

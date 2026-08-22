use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    ast::{
        Ast, IdentifierTree, TypeTree, expressions::BlockExpressionTree,
        generics::OptionalGenericParamsListTree,
    },
    sourceloc,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ArgumentDeclarationTree {
    pub name: IdentifierTree,
    pub type_: TypeTree,
    pub mutable: bool,
}

impl Ast for ArgumentDeclarationTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc().merge(&self.type_.loc()).unwrap()
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
    pub(crate) fn_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: OptionalGenericParamsListTree,
    pub arguments: Vec<ArgumentDeclarationTree>,
    pub args_close_paren_loc: sourceloc::SourceSpan,
    pub return_type: Option<TypeTree>,
}

impl Ast for FunctionDeclarationTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = self.name.loc().merge(&self.args_close_paren_loc).unwrap();
        if let Some(return_type) = &self.return_type {
            loc = loc.merge(&return_type.loc()).unwrap();
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
    fn loc(&self) -> sourceloc::SourceSpan {
        self.prototype.loc().merge(&self.body.loc()).unwrap()
    }
}

impl fmt::Display for FunctionDefinitionTree {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Function Definition: {}, {}", self.prototype, self.body)
    }
}

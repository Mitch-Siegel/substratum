use crate::frontend::ast::*;

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

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDeclarationTree {
    pub loc: SourceLocWithMod,
    pub name: generics::IdentifierWithGenericsTree,
    pub arguments: Vec<ArgumentDeclarationTree>,
    pub return_type: Option<TypeTree>,
}
impl FunctionDeclarationTree {
    pub fn new(
        loc: SourceLocWithMod,
        name: generics::IdentifierWithGenericsTree,
        arguments: Vec<ArgumentDeclarationTree>,
        return_type: Option<TypeTree>,
    ) -> Self {
        Self {
            loc,
            name,
            arguments,
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

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDefinitionTree {
    pub prototype: FunctionDeclarationTree,
    pub body: expressions::BlockExpressionTree,
}
impl FunctionDefinitionTree {
    pub fn new(prototype: FunctionDeclarationTree, body: expressions::BlockExpressionTree) -> Self {
        Self { prototype, body }
    }
}
impl Display for FunctionDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Function Definition: {}, {}", self.prototype, self.body)
    }
}

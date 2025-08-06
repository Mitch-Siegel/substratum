use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct ArgumentDeclarationTree {
    pub loc: SourceLoc,
    pub name: String,
    pub type_: TypeTree,
    pub mutable: bool,
}
impl ArgumentDeclarationTree {
    pub fn new(loc: SourceLoc, name: String, type_: TypeTree, mutable: bool) -> Self {
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

impl ReturnWalk<midend::symtab::Variable> for ArgumentDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut impl midend::linearizer::DefContext) -> midend::symtab::Variable {
        let variable_type: midend::types::Syntactic = self.type_.walk(context);

        let declared_argument =
            midend::symtab::Variable::new(self.name.clone(), Some(variable_type));
        declared_argument
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDeclarationTree {
    pub loc: SourceLoc,
    pub name: generics::IdentifierWithGenericsTree,
    pub arguments: Vec<ArgumentDeclarationTree>,
    pub return_type: Option<TypeTree>,
}
impl FunctionDeclarationTree {
    pub fn new(
        loc: SourceLoc,
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

impl ReturnWalk<midend::symtab::FunctionPrototype> for FunctionDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(
        self,
        context: &mut impl midend::linearizer::DefContext,
    ) -> midend::symtab::FunctionPrototype {
        let (string_name, generic_params) = self.name.walk(());

        let arguments = self
            .arguments
            .into_iter()
            .map(|arg| arg.walk(context))
            .collect();

        let return_type = match self.return_type {
            Some(type_) => type_.walk(context),
            None => midend::types::Syntactic::Unit,
        };

        midend::symtab::FunctionPrototype::new(string_name, generic_params, arguments, return_type)
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

impl CustomReturnWalk<midend::linearizer::BasicDefContext, midend::linearizer::BasicDefContext>
    for FunctionDefinitionTree
{
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(
        self,
        mut context: midend::linearizer::BasicDefContext,
    ) -> midend::linearizer::BasicDefContext {
        let declared_prototype = self.prototype.walk(&mut context);

        let mut function_context =
            midend::linearizer::FunctionWalkContext::new(context, declared_prototype).unwrap();

        self.body.walk(&mut function_context);

        function_context.into()
    }
}

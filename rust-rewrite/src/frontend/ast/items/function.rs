use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
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

impl Display for ArgumentDeclarationTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.mutable {
            write!(f, "mut ")?;
        }

        write!(f, "{}: {}", self.name, self.type_)
    }
}

impl midend::treewalk::Treewalk<midend::symtab::Variable> for ArgumentDeclarationTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> midend::symtab::Variable {
        let arg_type: midend::types::Syntactic = self
            .type_
            .linearize(ctx)
            .expect("argument types may not be '_'");

        midend::symtab::Variable::new(self.name.linearize(ctx), Some(arg_type))
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDeclarationTree {
    pub fn_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: Option<generics::GenericParamsListTree>,
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

impl midend::treewalk::Treewalk<midend::symtab::FunctionPrototype> for FunctionDeclarationTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        for arg in &self.arguments {
            ctx.declare(midend::symtab::DefPathComponent::Variable(
                arg.name.value.clone(),
            ))
            .unwrap();
        }
    }

    fn linearize(
        self,
        ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> midend::symtab::FunctionPrototype {
        let generic_params = match self.generic_params {
            Some(params) => params.linearize(ctx),
            None => Vec::new(),
        };

        let arguments = self
            .arguments
            .into_iter()
            .map(|arg| arg.linearize(ctx))
            .collect();

        let return_type = match self.return_type {
            Some(type_) => type_
                .linearize(ctx)
                .expect("function return types may not be '_'"),
            None => midend::types::Syntactic::Unit,
        };

        midend::symtab::FunctionPrototype::new(
            self.name.linearize(ctx),
            generic_params,
            arguments,
            return_type,
        )
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

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDefinitionTree {
    pub prototype: FunctionDeclarationTree,
    pub body: expressions::BlockExpressionTree,
}

// TODO: get this returning the Function rather than inserting automatically
impl Ast for FunctionDefinitionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.prototype.loc().merge(&self.body.loc()).unwrap()
    }
}

impl Display for FunctionDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Function Definition: {}, {}", self.prototype, self.body)
    }
}

impl midend::treewalk::Treewalk<()> for FunctionDefinitionTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        let function_component = midend::symtab::DefPathComponent::Function(
            midend::symtab::FunctionName::new(self.prototype.name.value.clone()),
        );

        ctx.declare(function_component.clone()).unwrap();
        ctx.push_def_path(function_component.clone()).unwrap();

        self.prototype.collect_symbols(ctx);
        self.body.collect_symbols(ctx);

        ctx.pop_def_path(function_component).unwrap();
    }

    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> () {
        let declared_prototype = self.prototype.linearize(ctx);
        let function_name = declared_prototype.name.clone();

        ctx.create_function(declared_prototype).unwrap();
        self.body.linearize(ctx);
        ctx.finish_function(function_name).unwrap();
    }
}

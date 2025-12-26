use crate::{frontend::ast::*, midend};

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

impl midend::treewalk::Linearize for ArgumentDeclarationTree {
    type Data = midend::symtab::Variable;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let maybe_arg_type;
        (maybe_arg_type, ctx) = self.type_.linearize_same_path(ctx)?;
        let arg_type = maybe_arg_type.expect("argument types may not be '_'");

        let (name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result(midend::symtab::Variable::new(name, Some(arg_type)))
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

impl midend::treewalk::Collect for FunctionDeclarationTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        for arg in &self.arguments {
            ctx.declare_value(arg.name.value.clone())?;
        }

        Ok(ctx.take())
    }
}

impl midend::treewalk::Linearize for FunctionDeclarationTree {
    type Data = midend::symtab::values::function::FunctionPrototype;
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let generic_params;
        (generic_params, ctx) = match self.generic_params {
            Some(params) => params.linearize_same_path(ctx)?,
            None => (Vec::new(), ctx),
        };

        let mut arguments = Vec::new();

        for arg in self.arguments {
            let linearized_arg;
            (linearized_arg, ctx) = arg.linearize_same_path(ctx)?;
            arguments.push(linearized_arg);
        }

        let return_type;
        (return_type, ctx) = match self.return_type {
            Some(type_) => {
                let maybe_return_type;
                (maybe_return_type, ctx) = type_.linearize_same_path(ctx)?;
                let return_type = maybe_return_type.expect("function return types may not be '_'");
                (return_type, ctx)
            }
            None => (midend::types::Syntactic::Unit, ctx),
        };

        let (name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result(midend::symtab::values::function::FunctionPrototype::new(
            name,
            generic_params,
            arguments,
            return_type,
        ))
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

impl midend::treewalk::Collect for FunctionDefinitionTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        let function_path = ctx.declare_value(self.prototype.name.value.clone())?;

        ctx = ctx.with_path(function_path);

        ctx = self.prototype.collect_same_path(ctx)?;
        self.body.collect_symbols(ctx)
    }
}

impl midend::treewalk::Linearize for FunctionDefinitionTree {
    type Data = ();
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let declared_prototype;
        (declared_prototype, ctx) = self.prototype.linearize_same_path(ctx)?;
        let function_name = declared_prototype.name.clone();

        ctx.create_function(declared_prototype).unwrap();
        let (return_value, mut ctx) = self.body.linearize(ctx)?;
        ctx.finish_function(function_name).unwrap();
        ctx.into_result(())
    }
}

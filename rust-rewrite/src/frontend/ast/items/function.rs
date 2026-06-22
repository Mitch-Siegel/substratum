use crate::{
    frontend::ast::*,
    midend::{self},
};

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

impl midend::treewalk::Linearize<midend::symtab::ValuePath> for ArgumentDeclarationTree {
    type Data = midend::symtab::values::Variable;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: midend::treewalk::ValueLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let maybe_arg_type;
        (maybe_arg_type, ctx) = self.type_.linearize(ctx)?;
        let arg_type = maybe_arg_type.expect("argument types may not be '_'");

        let (name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result(midend::symtab::values::Variable::new(name, Some(arg_type)))
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct FunctionDeclarationTree {
    pub fn_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: generics::OptionalGenericParamsListTree,
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

impl midend::treewalk::Collect<midend::symtab::TypePath> for FunctionDeclarationTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        for arg in &self.arguments {
            ctx.declare_value(arg.name.value.clone())?;
        }

        Ok(ctx.take())
    }
}

impl midend::treewalk::Linearize<midend::symtab::TypePath> for FunctionDeclarationTree {
    type Data = midend::symtab::values::function::FunctionPrototype;
    fn linearize_inner(
        self,
        ctx: midend::treewalk::TypeLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let (generic_params, ctx) = self.generic_params.linearize(ctx)?;

        let mut function_ctx = ctx
            .with_child_value(self.name.value.clone())
            .expect("unable to create function context");

        let mut arguments = Vec::new();

        for arg in self.arguments {
            let linearized_arg;
            (linearized_arg, function_ctx) = arg.linearize(function_ctx)?;
            arguments.push(linearized_arg);
        }

        let return_type;
        (return_type, function_ctx) = match self.return_type {
            Some(type_) => {
                let maybe_return_type;
                (maybe_return_type, function_ctx) = type_.linearize(function_ctx)?;
                let return_type = maybe_return_type.expect("function return types may not be '_'");
                (return_type, function_ctx)
            }
            None => (midend::types::Syntactic::Unit, function_ctx),
        };

        let name: String;
        (name, function_ctx) = <ast::IdentifierTree as midend::treewalk::Linearize<
            midend::symtab::ValuePath,
        >>::linearize(self.name, function_ctx)?;

        function_ctx.into_result(midend::symtab::values::function::FunctionPrototype::new(
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

impl midend::treewalk::Collect<midend::symtab::TypePath> for FunctionDefinitionTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        let _function_path = ctx.declare_value(self.prototype.name.value.clone())?;

        let ctx = self.prototype.collect_in_place(ctx)?;

        let ctx = ctx
            .with_child_value(self.prototype.name.value.clone())
            .expect("invalid path for function");
        self.body.collect_symbols(ctx)
    }
}

impl midend::treewalk::Linearize<midend::symtab::TypePath> for FunctionDefinitionTree {
    type Data = midend::symtab::values::Function;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: midend::treewalk::TypeLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let declared_prototype;
        (declared_prototype, ctx) = self.prototype.linearize(ctx)?;
        let function_name = declared_prototype.name.clone();

        ctx.create_function(declared_prototype).unwrap();

        let ctx = ctx
            .with_child_value(function_name.clone())
            .expect("invalid path for function");
        let (return_value, mut ctx) = self.body.linearize(ctx)?;
        let function = ctx.finish_function(function_name, return_value)?;
        ctx.into_result(function)
    }
}

use crate::{
    frontend::ast::{
        expressions, generics, sourceloc, types::TypeNoBoundsTree, Ast, Display, IdentifierTree,
        LinearizeResult, NameReflectable, ReflectName, TypeTree,
    },
    midend::{
        self,
        symtab::{self, TypePath, ValueOwner},
        treewalk::{self, PathedCtxTrait},
    },
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct ArgumentDeclarationTree {
    pub name: IdentifierTree,
    pub(crate) type_: TypeTree,
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

impl<P>
    midend::treewalk::Linearize<
        midend::treewalk::UnpathedLinearizeCtx,
        P,
        treewalk::LinearizeCtx<P>,
    > for ArgumentDeclarationTree
where
    P: midend::symtab::Path,
    TypeTree: treewalk::Linearize<
        treewalk::UnpathedLinearizeCtx,
        P,
        treewalk::LinearizeCtx<P>,
        Data = midend::types::Syntactic,
    >,
{
    type Data = midend::symtab::values::Variable;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: treewalk::LinearizeCtx<P>,
    ) -> treewalk::LinearizeResult<Self::Data, midend::treewalk::UnpathedLinearizeCtx> {
        let arg_type: midend::types::Syntactic;
        (arg_type, ctx) = self.type_.linearize(ctx)?;

        let (name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result(midend::symtab::values::Variable::new(name, Some(arg_type)))
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct FunctionDeclarationTree {
    pub(crate) fn_keyword_loc: sourceloc::SourceSpan,
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

impl midend::treewalk::Collect<TypePath> for FunctionDeclarationTree {
    fn collect_inner(
        &self,
        ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        let mut ctx = ctx.with_child_value(self.name.value.clone());
        for arg in &self.arguments {
            ctx.declare_value(arg.name.value.clone())?;
        }

        ctx.into_result()
    }
}

impl<P, C> treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for FunctionDeclarationTree
where
    P: symtab::Path + symtab::ValueOwner,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
    ArgumentDeclarationTree: treewalk::Linearize<
        treewalk::UnpathedLinearizeCtx,
        symtab::ValuePath,
        treewalk::PathedCtx<treewalk::UnpathedLinearizeCtx, symtab::ValuePath>,
        Data = symtab::values::Variable,
    >,
    TypeNoBoundsTree: treewalk::Linearize<
        treewalk::UnpathedLinearizeCtx,
        symtab::ValuePath,
        treewalk::PathedCtx<treewalk::UnpathedLinearizeCtx, symtab::ValuePath>,
        Data = midend::types::Syntactic,
    >,
{
    type Data = midend::symtab::values::function::FunctionPrototype;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> midend::treewalk::LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let (generic_params, ctx) = self.generic_params.linearize(ctx)?;

        let mut function_ctx = ctx.with_child_value(self.name.value.clone());
        let mut arguments = Vec::new();

        for arg in self.arguments {
            let linearized_arg;
            (linearized_arg, function_ctx) = arg.linearize(function_ctx)?;
            function_ctx.define_value(midend::symtab::Value::LocalBinding(
                midend::symtab::values::LocalBinding::FunctionParam(linearized_arg.clone()),
            ))?;
            arguments.push(linearized_arg);
        }

        let return_type;
        (return_type, function_ctx) = match self.return_type {
            Some(type_) => {
                let return_type;
                (return_type, function_ctx) = type_.linearize(function_ctx)?;
                (return_type, function_ctx)
            }
            None => (midend::types::Syntactic::Unit, function_ctx),
        };

        let name: String;
        (name, function_ctx) = self.name.linearize(function_ctx)?;

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

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct FunctionDefinitionTree {
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
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        let _function_path = ctx.declare_value(self.prototype.name.value.clone())?;

        let ctx = self.prototype.collect_symbols(ctx)?;

        let ctx = ctx.with_child_value(self.prototype.name.value.clone());
        self.body.collect_inner(ctx)
    }
}

impl<P>
    treewalk::Linearize<
        treewalk::UnpathedLinearizeCtx,
        P,
        treewalk::PathedCtx<treewalk::UnpathedLinearizeCtx, P>,
    > for FunctionDefinitionTree
where
    P: symtab::Path + symtab::ValueOwner,
{
    type Data = symtab::values::Function;
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(
        self,
        mut ctx: treewalk::PathedCtx<treewalk::UnpathedLinearizeCtx, P>,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let declared_prototype;
        (declared_prototype, ctx) = self.prototype.linearize(ctx)?;
        let function_name = declared_prototype.name.clone();

        let function_path = ctx.path().clone().with_child_value(function_name);
        let unit_type = ctx.semantic_type_for_syntactic(&midend::types::Syntactic::Unit)?;
        let arg_def_paths: Vec<symtab::ValuePath> = declared_prototype
            .arguments
            .iter()
            .map(|arg| function_path.clone().with_child_value(arg.name.clone()))
            .collect();

        let unpathed_function_ctx = treewalk::UnpathedFunctionLinearizeCtx::new(
            ctx,
            declared_prototype,
            unit_type,
            &arg_def_paths,
        );

        let function_ctx = unpathed_function_ctx.into_pathed_ctx(function_path);
        let (return_value_id, function_ctx) = self.body.linearize(function_ctx)?;

        function_ctx.finalize(return_value_id)

        // let function_ctx = treewalk::FunctionLinearizeCtx::new()
        // ctx.create_function(declared_prototype).unwrap();

        // let ctx = ctx
        // .with_child_value(function_name.clone());
        // let (return_value, mut ctx) = self.body.linearize(ctx)?;
        // let function = ctx.finish_function(function_name, return_value)?;
        // ctx.into_result(function)
    }
}

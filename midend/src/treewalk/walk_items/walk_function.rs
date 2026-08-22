use frontend::ast::{self, Ast};

use crate::{
    symtab::{self, ValueOwner},
    treewalk::{
        Collect, CollectResult, Linearize, LinearizeCtx, LinearizeResult, PathedCtx,
        PathedCtxTrait, PathedLinearizeCtxTrait, TypeCollectCtx, UnpathedFunctionLinearizeCtx,
        UnpathedLinearizeCtx,
    },
    types,
};

impl<P> Linearize<UnpathedLinearizeCtx, P, LinearizeCtx<P>>
    for ast::items::function::ArgumentDeclarationTree
where
    P: symtab::Path,
    ast::TypeTree: Linearize<UnpathedLinearizeCtx, P, LinearizeCtx<P>, Data = types::Syntactic>,
{
    type Data = symtab::values::Variable;
    fn linearize_inner(
        self,
        mut ctx: LinearizeCtx<P>,
    ) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let arg_type: types::Syntactic;
        (arg_type, ctx) = self.type_.linearize(ctx)?;

        let (name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result(symtab::values::Variable::new(name, Some(arg_type)))
    }
}

impl Collect<symtab::TypePath> for ast::items::FunctionDeclarationTree {
    fn collect_inner(&self, ctx: TypeCollectCtx) -> CollectResult {
        let mut ctx = ctx.with_child_value(self.name.value.clone());
        for arg in &self.arguments {
            ctx.declare_value(arg.name.value.clone())?;
        }

        ctx.into_result()
    }
}

impl<P, C> Linearize<UnpathedLinearizeCtx, P, C> for ast::items::FunctionDeclarationTree
where
    P: symtab::Path + symtab::ValueOwner,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
    ast::items::function::ArgumentDeclarationTree: Linearize<
            UnpathedLinearizeCtx,
            symtab::ValuePath,
            PathedCtx<UnpathedLinearizeCtx, symtab::ValuePath>,
            Data = symtab::values::Variable,
        >,
    ast::types::TypeNoBoundsTree: Linearize<
            UnpathedLinearizeCtx,
            symtab::ValuePath,
            PathedCtx<UnpathedLinearizeCtx, symtab::ValuePath>,
            Data = types::Syntactic,
        >,
{
    type Data = symtab::values::function::FunctionPrototype;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let (generic_params, ctx) = self.generic_params.linearize(ctx)?;

        let mut function_ctx = ctx.with_child_value(self.name.value.clone());
        let mut arguments = Vec::new();

        for arg in self.arguments {
            let linearized_arg;
            (linearized_arg, function_ctx) = arg.linearize(function_ctx)?;
            function_ctx.define_value(symtab::Value::LocalBinding(
                symtab::values::LocalBinding::FunctionParam(linearized_arg.clone()),
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
            None => (types::Syntactic::Unit, function_ctx),
        };

        let name: String;
        (name, function_ctx) = self.name.linearize(function_ctx)?;

        function_ctx.into_result(symtab::values::function::FunctionPrototype::new(
            name,
            generic_params,
            arguments,
            return_type,
        ))
    }
}

impl Collect<symtab::TypePath> for ast::items::FunctionDefinitionTree {
    fn collect_inner(&self, mut ctx: TypeCollectCtx) -> CollectResult {
        let _function_path = ctx.declare_value(self.prototype.name.value.clone())?;

        let ctx = self.prototype.collect_symbols(ctx)?;

        let ctx = ctx.with_child_value(self.prototype.name.value.clone());
        self.body.collect_inner(ctx)
    }
}

impl<P> Linearize<UnpathedLinearizeCtx, P, PathedCtx<UnpathedLinearizeCtx, P>>
    for ast::items::FunctionDefinitionTree
where
    P: symtab::Path + symtab::ValueOwner,
{
    type Data = symtab::values::Function;
    #[trace::instrument(skip(self), level = "trace", fields(tree_name = Self::name()))]
    fn linearize_inner(
        self,
        mut ctx: PathedCtx<UnpathedLinearizeCtx, P>,
    ) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let declared_prototype;
        (declared_prototype, ctx) = self.prototype.linearize(ctx)?;
        let function_name = declared_prototype.name.clone();

        let function_path = ctx.path().clone().with_child_value(function_name);
        let unit_type = ctx.semantic_type_for_syntactic(&types::Syntactic::Unit)?;
        let arg_def_paths: Vec<symtab::ValuePath> = declared_prototype
            .arguments
            .iter()
            .map(|arg| function_path.clone().with_child_value(arg.name.clone()))
            .collect();

        let unpathed_function_ctx =
            UnpathedFunctionLinearizeCtx::new(ctx, declared_prototype, unit_type, &arg_def_paths);

        let function_ctx = unpathed_function_ctx.into_pathed_ctx(function_path);
        let (return_value_id, function_ctx) = self.body.linearize(function_ctx)?;

        function_ctx.finalize(return_value_id)

        // let function_ctx = FunctionLinearizeCtx::new()
        // ctx.create_function(declared_prototype).unwrap();

        // let ctx = ctx
        // .with_child_value(function_name.clone());
        // let (return_value, mut ctx) = self.body.linearize(ctx)?;
        // let function = ctx.finish_function(function_name, return_value)?;
        // ctx.into_result(function)
    }
}

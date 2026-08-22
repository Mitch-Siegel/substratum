use std::collections::BTreeSet;

use frontend::{ast, sourceloc};

use crate::{
    symtab,
    treewalk::{
        Collect, CollectResult, Linearize, LinearizeResult, PathedCtxTrait,
        PathedLinearizeCtxTrait, TypeCollectCtx, UnpathedFunctionLinearizeCtx,
        UnpathedLinearizeCtx, UnpathedLinearizeCtxTrait,
    },
    types,
};

fn linearize_params_list_ctxless(
    params: ast::GenericParamsListTree,
) -> Vec<(sourceloc::SourceSpan, types::GenericParam)> {
    let generic_params_vec = params.into_vec_with_locs();
    generic_params_vec
        .into_iter()
        .map(|(param_name, loc)| {
            (
                loc,
                types::type_interner::GenericParam::TypeParam(param_name.value),
            )
        })
        .collect()
}

// symbol collection (type and value)
impl Collect<symtab::TypePath> for ast::GenericParamTree {
    fn collect_inner(&self, mut ctx: TypeCollectCtx) -> CollectResult {
        ctx.declare_type(self.name.value.clone())?;
        ctx.into_result()
    }
}

// linearization (type and value)
impl<P, C> Linearize<UnpathedLinearizeCtx, P, C> for ast::GenericParamTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
{
    type Data = String;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let (name, ctx) = self.name.linearize(ctx)?;
        ctx.into_result(name)
    }
}

impl Collect<symtab::TypePath> for ast::GenericParamsListTree {
    fn collect_inner(&self, mut ctx: TypeCollectCtx) -> CollectResult {
        for param in &self.params {
            ctx = param.collect_symbols(ctx)?;
        }

        ctx.into_result()
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::GenericParamsListTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = types::GenericParamsList;
    #[trace::instrument(skip(self, ctx), level = "trace")]
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let mut generic_params_set = BTreeSet::<types::GenericParam>::new();

        let ctxless = linearize_params_list_ctxless(self);

        let mut params_list = types::GenericParamsList::new();

        for (loc, param) in ctxless {
            assert!(
                generic_params_set.insert(param.clone()),
                "duplicate generic parameter {} @ {}",
                param,
                loc.start()
            );

            params_list.push(param);
        }

        ctx.into_result(params_list)
    }
}

impl Collect<symtab::TypePath> for ast::OptionalGenericParamsListTree {
    fn collect_inner(&self, ctx: TypeCollectCtx) -> CollectResult {
        if let Some(params) = &self.maybe_params {
            params.collect_symbols(ctx)?.into_result()
        } else {
            ctx.into_result()
        }
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::OptionalGenericParamsListTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = types::GenericParamsList;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (params, ctx) = match self.maybe_params {
            Some(params) => {
                let (params_result, ctx) = params.linearize(ctx)?;
                (params_result, ctx)
            }
            None => (types::GenericParamsList::new(), ctx),
        };

        ctx.into_result(params)
    }
}

impl<P, C> Linearize<UnpathedLinearizeCtx, P, C> for ast::GenericArgsListTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
    ast::types::TypeNoBoundsTree: Linearize<UnpathedLinearizeCtx, P, C, Data = types::Syntactic>,
{
    type Data = Vec<types::ParamSubst>;
    #[trace::instrument(skip(self), level = "trace")]
    fn linearize_inner(self, mut ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let mut generic_args = Vec::<types::ParamSubst>::new();
        for arg in self.args {
            let param_type;
            (param_type, ctx) = arg.linearize(ctx)?;

            let param = match param_type {
                types::Syntactic::GenericParam(param_name) => {
                    types::ParamSubst::Dependent(types::GenericParam::TypeParam(param_name))
                }
                _ => types::ParamSubst::Concrete(
                    ctx.semantic_type_for_syntactic(&param_type).unwrap(),
                ),
            };
            generic_args.push(param);
        }

        ctx.into_result(generic_args)
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C> for ast::GenericArgsListTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
    ast::types::TypeNoBoundsTree:
        Linearize<UnpathedFunctionLinearizeCtx, P, C, Data = Option<types::Syntactic>>,
{
    type Data = Vec<types::ParamSubst>;
    #[trace::instrument(skip(self), level = "trace")]
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let mut generic_args = Vec::<types::ParamSubst>::new();
        for arg in self.args {
            let maybe_param_type: Option<types::Syntactic>;
            (maybe_param_type, ctx) = arg.linearize(ctx)?;
            // FUTURE: implement generic param type inference
            let param_type = maybe_param_type.expect("generic params must have a type");

            let param = match param_type {
                types::Syntactic::GenericParam(param_name) => {
                    types::ParamSubst::Dependent(types::GenericParam::TypeParam(param_name))
                }
                _ => types::ParamSubst::Concrete(
                    ctx.semantic_type_for_syntactic(&param_type).unwrap(),
                ),
            };
            generic_args.push(param);
        }

        ctx.into_result(generic_args)
    }
}

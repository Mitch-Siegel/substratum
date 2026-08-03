use crate::{
    frontend::ast::{
        midend, sourceloc, treewalk, types::TypeNoBoundsTree, Ast, Display, IdentifierTree,
        LinearizeResult, NameReflectable, ReflectName, TypeTree,
    },
    midend::{
        symtab::{self, TypePath},
        treewalk::PathedCtxTrait,
    },
};
use std::collections::BTreeSet;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct GenericParamTree {
    pub name: IdentifierTree,
}

impl Ast for GenericParamTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc()
    }
}

// symbol collection (type and value)
impl midend::treewalk::Collect<TypePath> for GenericParamTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.declare_type(self.name.value.clone())?;
        ctx.into_result()
    }
}

// linearization (type and value)
impl<P, C> treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for GenericParamTree
where
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
{
    type Data = String;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let (name, ctx) = self.name.linearize(ctx)?;
        ctx.into_result(name)
    }
}

impl Display for GenericParamTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct GenericParamsListTree {
    pub open_angle_bracket_loc: sourceloc::SourceSpan,
    pub params: Vec<GenericParamTree>,
    pub close_angle_bracket_loc: sourceloc::SourceSpan,
}

impl GenericParamsListTree {
    pub(crate) fn into_vec(self) -> Vec<IdentifierTree> {
        self.params.into_iter().map(|param| param.name).collect()
    }

    pub(crate) fn into_vec_with_locs(self) -> Vec<(IdentifierTree, sourceloc::SourceSpan)> {
        self.params
            .into_iter()
            .map(|param| {
                let param_loc = param.loc();
                (param.name, param_loc)
            })
            .collect()
    }

    pub(crate) fn linearize_ctxless(
        self,
    ) -> Vec<(sourceloc::SourceSpan, midend::types::GenericParam)> {
        let generic_params_vec = self.into_vec_with_locs();
        generic_params_vec
            .into_iter()
            .map(|(param_name, loc)| {
                (
                    loc,
                    midend::types::type_interner::GenericParam::TypeParam(param_name.value),
                )
            })
            .collect()
    }
}

impl Ast for GenericParamsListTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_angle_bracket_loc
            .clone()
            .merge(&self.close_angle_bracket_loc)
            .unwrap()
    }
}

impl Display for GenericParamsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for param in &self.params {
            if first {
                first = false;
            } else {
                write!(f, ", ")?;
            }

            write!(f, "{param}")?;
        }

        Ok(())
    }
}

impl midend::treewalk::Collect<TypePath> for GenericParamsListTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        for param in &self.params {
            ctx = param.collect_symbols(ctx)?;
        }

        ctx.into_result()
    }
}

impl<U, P, C> midend::treewalk::Linearize<U, P, C> for GenericParamsListTree
where
    U: treewalk::UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = midend::types::GenericParamsList;
    #[tracing::instrument(skip(self, ctx), level = "trace")]
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let mut generic_params_set = BTreeSet::<midend::types::GenericParam>::new();

        let ctxless = self.linearize_ctxless();

        let mut params_list = midend::types::GenericParamsList::new();

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

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct OptionalGenericParamsListTree {
    pub start_loc: sourceloc::SourceLoc,
    pub maybe_params: Option<GenericParamsListTree>,
}

impl Ast for OptionalGenericParamsListTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        if let Some(params) = &self.maybe_params {
            params.loc()
        } else {
            self.start_loc.clone().into()
        }
    }
}

impl Display for OptionalGenericParamsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.maybe_params {
            Some(p) => write!(f, "{p}"),
            None => Ok(()),
        }
    }
}

impl midend::treewalk::Collect<TypePath> for OptionalGenericParamsListTree {
    fn collect_inner(
        &self,
        ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        if let Some(params) = &self.maybe_params {
            params.collect_symbols(ctx)?.into_result()
        } else {
            ctx.into_result()
        }
    }
}

impl<U, P, C> treewalk::Linearize<U, P, C> for OptionalGenericParamsListTree
where
    U: treewalk::UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = midend::types::GenericParamsList;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (params, ctx) = match self.maybe_params {
            Some(params) => {
                let (params_result, ctx) = params.linearize(ctx)?;
                (params_result, ctx)
            }
            None => (midend::types::GenericParamsList::new(), ctx),
        };

        ctx.into_result(params)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub(crate) struct GenericArgsListTree {
    pub open_angle_bracket_loc: sourceloc::SourceSpan,
    pub args: Vec<TypeTree>,
    pub close_angle_bracket_loc: sourceloc::SourceSpan,
}

impl Ast for GenericArgsListTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_angle_bracket_loc
            .clone()
            .merge(&self.close_angle_bracket_loc)
            .unwrap()
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for GenericArgsListTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
    TypeNoBoundsTree:
        treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C, Data = midend::types::Syntactic>,
{
    type Data = Vec<midend::types::ParamSubst>;
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let mut generic_args = Vec::<midend::types::ParamSubst>::new();
        for arg in self.args {
            let param_type;
            (param_type, ctx) = arg.linearize(ctx)?;

            let param = match param_type {
                midend::types::Syntactic::GenericParam(param_name) => {
                    midend::types::ParamSubst::Dependent(midend::types::GenericParam::TypeParam(
                        param_name,
                    ))
                }
                _ => midend::types::ParamSubst::Concrete(
                    ctx.semantic_type_for_syntactic(&param_type).unwrap(),
                ),
            };
            generic_args.push(param);
        }

        ctx.into_result(generic_args)
    }
}

impl<P, C> midend::treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C>
    for GenericArgsListTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = P,
    >,
    TypeNoBoundsTree: treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        P,
        C,
        Data = Option<midend::types::Syntactic>,
    >,
{
    type Data = Vec<midend::types::ParamSubst>;
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let mut generic_args = Vec::<midend::types::ParamSubst>::new();
        for arg in self.args {
            let maybe_param_type: Option<midend::types::Syntactic>;
            (maybe_param_type, ctx) = arg.linearize(ctx)?;
            // FUTURE: implement generic param type inference
            let param_type = maybe_param_type.expect("generic params must have a type");

            let param = match param_type {
                midend::types::Syntactic::GenericParam(param_name) => {
                    midend::types::ParamSubst::Dependent(midend::types::GenericParam::TypeParam(
                        param_name,
                    ))
                }
                _ => midend::types::ParamSubst::Concrete(
                    ctx.semantic_type_for_syntactic(&param_type).unwrap(),
                ),
            };
            generic_args.push(param);
        }

        ctx.into_result(generic_args)
    }
}

impl Display for GenericArgsListTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut first = true;
        for param in &self.args {
            if first {
                first = false;
            } else {
                write!(f, ", ")?;
            }

            write!(f, "{param}")?;
        }

        Ok(())
    }
}

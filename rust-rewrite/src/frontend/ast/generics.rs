use crate::frontend::ast::*;
use std::collections::BTreeSet;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamTree {
    pub name: IdentifierTree,
}

impl Ast for GenericParamTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.name.loc()
    }
}

// symbol collection (type and value)
impl midend::treewalk::Collect<midend::symtab::TypePath> for GenericParamTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.declare_type(self.name.value.clone())?;
        Ok(ctx.take())
    }
}

impl midend::treewalk::Collect<midend::symtab::ValuePath> for GenericParamTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.declare_type(self.name.value.clone())?;
        Ok(ctx.take())
    }
}

// linearization (type and value)
impl midend::treewalk::Linearize<midend::symtab::RawPath> for GenericParamTree {
    type Data = String;
    fn linearize_inner(
        self,
        ctx: midend::treewalk::RawLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        self.name.linearize(ctx)
    }
}

impl Display for GenericParamTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamsListTree {
    pub open_angle_bracket_loc: sourceloc::SourceSpan,
    pub params: Vec<GenericParamTree>,
    pub close_angle_bracket_loc: sourceloc::SourceSpan,
}

impl GenericParamsListTree {
    pub fn into_vec(self) -> Vec<IdentifierTree> {
        self.params.into_iter().map(|param| param.name).collect()
    }

    pub fn into_vec_with_locs(self) -> Vec<(IdentifierTree, sourceloc::SourceSpan)> {
        self.params
            .into_iter()
            .map(|param| {
                let param_loc = param.loc();
                (param.name, param_loc)
            })
            .collect()
    }

    pub fn linearize_ctxless(self) -> Vec<(sourceloc::SourceSpan, midend::types::GenericParam)> {
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
            if !first {
                write!(f, ", ")?;
            } else {
                first = false;
            }

            write!(f, "{}", param)?;
        }

        Ok(())
    }
}

impl midend::treewalk::Collect<midend::symtab::TypePath> for GenericParamsListTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        for param in &self.params {
            ctx = <GenericParamTree as midend::treewalk::Collect<midend::symtab::TypePath>>::collect_in_place::<midend::symtab::TypePath>(param, ctx)?;
        }

        Ok(ctx.take())
    }
}

impl midend::treewalk::Collect<midend::symtab::ValuePath> for GenericParamsListTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        for param in &self.params {
            ctx = <GenericParamTree as midend::treewalk::Collect<midend::symtab::ValuePath>>::collect_in_place::<midend::symtab::ValuePath>(param, ctx)?;
        }

        Ok(ctx.take())
    }
}

impl midend::treewalk::Linearize<midend::symtab::RawPath> for GenericParamsListTree {
    type Data = midend::types::GenericParamsList;
    #[tracing::instrument(skip(self, ctx), level = "trace")]
    fn linearize_inner(
        self,
        ctx: midend::treewalk::RawLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let mut generic_params_set = BTreeSet::<midend::types::GenericParam>::new();

        let ctxless = self.linearize_ctxless();

        let mut params_list = midend::types::GenericParamsList::new();

        for (loc, param) in ctxless {
            if !generic_params_set.insert(param.clone()) {
                panic!("duplicate generic parameter {} @ {}", param, loc.start())
            }

            params_list.push(param);
        }

        ctx.into_result(params_list)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct OptionalGenericParamsListTree {
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
            Some(p) => write!(f, "{}", p),
            None => Ok(()),
        }
    }
}

impl midend::treewalk::Collect<midend::symtab::TypePath> for OptionalGenericParamsListTree {
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        if let Some(params) = &self.maybe_params {
            params.collect_symbols(ctx)
        } else {
            Ok(ctx.take())
        }
    }
}

impl midend::treewalk::Collect<midend::symtab::ValuePath> for OptionalGenericParamsListTree {
    fn collect_symbols(
        &self,
        ctx: midend::treewalk::ValueCollectCtx,
    ) -> midend::treewalk::CollectResult {
        if let Some(params) = &self.maybe_params {
            params.collect_symbols(ctx)
        } else {
            Ok(ctx.take())
        }
    }
}

impl midend::treewalk::Linearize<midend::symtab::RawPath> for OptionalGenericParamsListTree {
    type Data = midend::types::GenericParamsList;
    fn linearize_inner(
        self,
        ctx: midend::treewalk::RawLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let (params, ctx) = match self.maybe_params {
            Some(params) => {
                let (params_result, ctx) = params.linearize(ctx)?;
                (params_result, ctx)
            }
            None => (midend::types::GenericParamsList::new(), ctx.take()),
        };

        ctx.into_result(params)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericArgsListTree {
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

impl midend::treewalk::Linearize<midend::symtab::RawPath> for GenericArgsListTree {
    type Data = Vec<midend::types::ParamSubst>;
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize_inner(
        self,
        mut ctx: midend::treewalk::RawLinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let mut generic_args = Vec::<midend::types::ParamSubst>::new();
        for arg in self.args {
            let maybe_type;
            (maybe_type, ctx) = arg.linearize_in_place(ctx)?;
            let param_type = maybe_type.expect("generic params must have a type");

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
            if !first {
                write!(f, ", ")?;
            } else {
                first = false;
            }

            write!(f, "{}", param)?;
        }

        Ok(())
    }
}

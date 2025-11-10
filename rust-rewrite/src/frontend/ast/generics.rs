use crate::frontend::ast::*;
use std::collections::BTreeSet;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamTree {
    pub loc: SourceLoc,
    pub name: String,
}
impl GenericParamTree {
    pub fn new(loc: SourceLoc, name: String) -> Self {
        Self { loc, name }
    }
}
impl Display for GenericParamTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericParamsListTree {
    pub loc: SourceLoc,
    pub params: Vec<GenericParamTree>,
}

impl GenericParamsListTree {
    pub fn new(loc: SourceLoc, params: Vec<GenericParamTree>) -> Self {
        Self { loc, params }
    }

    pub fn as_vec(self) -> Vec<String> {
        self.params.into_iter().map(|param| param.name).collect()
    }

    pub fn as_vec_with_locs(self) -> Vec<(String, SourceLoc)> {
        self.params
            .into_iter()
            .map(|param| (param.name, param.loc))
            .collect()
    }

    pub fn linearize_ctxless(self) -> Vec<(SourceLoc, midend::types::GenericParam)> {
        let generic_params_vec = self.as_vec_with_locs();
        generic_params_vec
            .into_iter()
            .map(|(param, loc)| {
                (
                    loc,
                    midend::types::type_interner::GenericParam::TypeParam(param),
                )
            })
            .collect()
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

impl treewalk::Linearize<midend::types::GenericParamsList> for GenericParamsListTree {
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize(self, _: &mut treewalk::LinearizeCtx) -> midend::types::GenericParamsList {
        let mut generic_params_set = BTreeSet::<midend::types::GenericParam>::new();

        let ctxless = self.linearize_ctxless();

        ctxless
            .into_iter()
            .map(|(loc, param)| {
                if !generic_params_set.insert(param.clone()) {
                    panic!("duplicate generic parameter {} @ {}", param, loc)
                }

                param
            })
            .collect::<midend::types::GenericParamsList>()
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct GenericArgsListTree {
    pub loc: SourceLoc,
    pub args: Vec<TypeTree>,
}

impl GenericArgsListTree {
    pub fn new(loc: SourceLoc, args: Vec<TypeTree>) -> Self {
        Self { loc, args }
    }
}

impl midend::treewalk::Linearize<Vec<midend::types::ParamSubst>> for GenericArgsListTree {
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> Vec<midend::types::ParamSubst> {
        let generic_args: Vec<midend::types::ParamSubst> = self
            .args
            .into_iter()
            .map(|param| {
                let param_type = param.linearize(ctx);
                match param_type {
                    midend::types::Syntactic::GenericParam(param_name) => {
                        midend::types::ParamSubst::Dependent(
                            midend::types::GenericParam::TypeParam(param_name),
                        )
                    }
                    _ => midend::types::ParamSubst::Concrete(
                        ctx.semantic_type_for_syntactic(
                            param_type,
                            midend::types::ParamSubstMap::empty(),
                        )
                        .unwrap(),
                    ),
                }
            })
            .collect();

        generic_args
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

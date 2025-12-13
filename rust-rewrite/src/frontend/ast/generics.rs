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

impl midend::treewalk::Treewalk<String> for GenericParamTree {
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> String {
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
    pub fn as_vec(self) -> Vec<IdentifierTree> {
        self.params.into_iter().map(|param| param.name).collect()
    }

    pub fn as_vec_with_locs(self) -> Vec<(IdentifierTree, sourceloc::SourceSpan)> {
        self.params
            .into_iter()
            .map(|param| {
                let param_loc = param.loc();
                (param.name, param_loc)
            })
            .collect()
    }

    pub fn linearize_ctxless(self) -> Vec<(sourceloc::SourceSpan, midend::types::GenericParam)> {
        let generic_params_vec = self.as_vec_with_locs();
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

impl midend::treewalk::Treewalk<midend::types::GenericParamsList> for GenericParamsListTree {
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize(self, _: &mut midend::treewalk::LinearizeCtx) -> midend::types::GenericParamsList {
        let mut generic_params_set = BTreeSet::<midend::types::GenericParam>::new();

        let ctxless = self.linearize_ctxless();

        ctxless
            .into_iter()
            .map(|(loc, param)| {
                if !generic_params_set.insert(param.clone()) {
                    panic!("duplicate generic parameter {} @ {}", param, loc.start())
                }

                param
            })
            .collect::<midend::types::GenericParamsList>()
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

impl midend::treewalk::Treewalk<Vec<midend::types::ParamSubst>> for GenericArgsListTree {
    #[tracing::instrument(skip(self), level = "trace")]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> Vec<midend::types::ParamSubst> {
        let generic_args: Vec<midend::types::ParamSubst> = self
            .args
            .into_iter()
            .map(|param| {
                let param_type = param
                    .linearize(ctx)
                    .expect("generic params must have a type");
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

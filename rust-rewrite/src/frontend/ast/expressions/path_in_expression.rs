use serde::{Deserialize, Serialize};

use crate::{
    frontend::{ast::*, *},
    midend::{self, treewalk::Treewalk},
    trace,
};

enum PathExprSegmentAction {
    Super,
    Ident(String, Option<GenericArgsListTree>),
    SelfLower,
    SelfUpper,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum PathIdentSegment {
    Ident(IdentifierTree),
    Super(sourceloc::SourceSpan),
    SelfLower(sourceloc::SourceSpan),
    SelfUpper(sourceloc::SourceSpan),
}

impl Ast<()> for PathIdentSegment {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Ident(ident) => ident.loc(),
            Self::Super(loc) => loc.clone(),
            Self::SelfUpper(loc) => loc.clone(),
            Self::SelfLower(loc) => loc.clone(),
        }
    }
}

impl midend::treewalk::Treewalk<()> for PathIdentSegment {
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> () {
        unreachable!()
    }
}

impl std::fmt::Display for PathIdentSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PathIdentSegment::Ident(ident) => write!(f, "{}", ident),
            PathIdentSegment::Super(_) => write!(f, "super"),
            PathIdentSegment::SelfLower(_) => write!(f, "self"),
            PathIdentSegment::SelfUpper(_) => write!(f, "Self"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathExprSegmentTree {
    pub ident: PathIdentSegment,
    pub generic_args: Option<ast::generics::GenericArgsListTree>,
}

impl Ast<PathExprSegmentAction> for PathExprSegmentTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match &self.generic_args {
            Some(generic_args) => self.ident.loc().merge(&generic_args.loc()).unwrap(),
            None => self.ident.loc(),
        }
    }
}

impl midend::treewalk::Treewalk<PathExprSegmentAction> for PathExprSegmentTree {
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> PathExprSegmentAction {
        match self.ident {
            PathIdentSegment::Super(_) => {
                self.expect_no_generics().unwrap();
                PathExprSegmentAction::Super
            }
            PathIdentSegment::Ident(ident) => {
                PathExprSegmentAction::Ident(ident.linearize(ctx), self.generic_args)
            }
            PathIdentSegment::SelfLower(_) => {
                self.expect_no_generics().unwrap();
                PathExprSegmentAction::SelfLower
                
            },
            PathIdentSegment::SelfUpper(_) => {
                self.expect_no_generics().unwrap();
                PathExprSegmentAction::SelfUpper
            }
        }
    }
}

impl PathExprSegmentTree {
    fn expect_no_generics(self) -> Result<(), String> {
        match self.generic_args {
            Some(args) => Err(format!(
                "found generic args at {}, expected none",
                args.loc().start()
            )),
            None => Ok(()),
        }
    }
}

impl std::fmt::Display for PathExprSegmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.ident)?;
        if let Some(args) = &self.generic_args {
            write!(f, "<{}>", args)?;
        }
        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathInExpressionTree {
    pub segments: Vec<PathExprSegmentTree>,
}

impl Ast<midend::ir::ValueId> for PathInExpressionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut seg_iter = self.segments.iter();
        let mut loc_span = seg_iter
            .next()
            .expect("PathInExpressionTree must have at least one segment")
            .loc();

        while let Some(segment) = seg_iter.next() {
            loc_span = loc_span.merge(&segment.loc()).unwrap();
        }

        loc_span
    }
}

impl midend::treewalk::Treewalk<midend::ir::ValueId> for PathInExpressionTree {
    fn collect_symbols(&self, ctx: &mut midend::treewalk::CollectCtx) {
        ()
    }

    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let _span = trace::span_auto_debug!(
            "treewalk::linearize for PathInexpressionTree @",
            "{:?}",
            self.loc()
        );
        let mut expr_path = midend::symtab::DefPath::empty();
        let mut expr_path_loc: sourceloc::SourceSpan = self.loc().start().into();
        let mut segments = self.segments.into_iter();
        while let Some(segment) = segments.next() {
            trace::warning!("{}", expr_path);
            let segment_loc = segment.loc();
            let must_end = match segment.linearize(ctx) {
                PathExprSegmentAction::Super => {
                    match expr_path.pop() {
                        Some(_) => (),
                        None => panic!(
                            "path segment 'Super' on invalid/empty path at {}",
                            segment_loc.start()
                        ),
                    }
                    false
                },
                PathExprSegmentAction::Ident(name, maybe_generics) => {
                    let must_end = walk_ident_segment(name, &mut expr_path, ctx).unwrap();
                    record_monomorphization(ctx, &expr_path, maybe_generics);
                    must_end
                },
                PathExprSegmentAction::SelfLower => {
                    unimplemented!("'self' in path expression ({}) not implemented", segment_loc);
                },
                PathExprSegmentAction::SelfUpper => {
                    unimplemented!("'Self' in path expression ({}) not implemented", segment_loc);
                }
            };

            if must_end && (segments.size_hint().0 > 0) {
                        panic!(
                            "path {} at {} has additional unexpected segment(s): {}@{}",
                            expr_path,
                            expr_path_loc,
                            expr_path.last().name(),
                            segment_loc,
                        )
                    }

            expr_path_loc = expr_path_loc.merge(&segment_loc).unwrap();
            trace::trace!("iteration end path: {}", expr_path);
        }
        midend::ir::ValueId::new(1)
    }
}

impl std::fmt::Display for PathInExpressionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut iter = self.segments.iter().peekable();
        while let Some(segment) = iter.next() {
            write!(f, "{}", segment)?;
            if iter.peek().is_some() {
                write!(f, "::")?;
            }
        }
        Ok(())
    }
}

fn walk_ident_segment(
    ident: String,
    expr_path: &mut midend::symtab::DefPath,
    ctx: &mut midend::treewalk::LinearizeCtx,
) -> Result<bool, String> {
    let function_component = midend::symtab::DefPathComponent::Function(
        midend::symtab::FunctionName::new(ident.clone()),
    );

    let type_component =
        midend::symtab::DefPathComponent::Type(midend::types::Syntactic::Named(ident.clone()));

    let variable_component = midend::symtab::DefPathComponent::Variable(ident.clone());

    let function_result = match expr_path.clone().with_component(function_component.clone()) {
        Ok(function_path) => {
            match ctx
                .symtab()
                .lookup_under::<midend::symtab::symbol::Function>(ctx.def_path(), function_path)
            {
                Ok(s) => Some(s.0),
                _ => None,
            }
        }
        Err(_) => None,
    };

    let type_result = match expr_path.clone().with_component(type_component.clone()) {
        Ok(type_path) => {
            match ctx
                .symtab()
                .lookup_under::<midend::symtab::symbol::TypeDefinition>(ctx.def_path(), type_path)
            {
                Ok(s) => Some(s.0),
                _ => None,
            }
        }
        Err(_) => None,
    };

    let variable_result = match expr_path.clone().with_component(variable_component.clone()) {
        Ok(variable_path) => {
            match ctx
                .symtab()
                .lookup_under::<midend::symtab::symbol::Variable>(ctx.def_path(), variable_path)
            {
                Ok(s) => Some(s.0),
                _ => None,
            }
        }
        Err(_) => None,
    };

    let mut results: Vec<midend::symtab::DefPath> =
        vec![type_result, function_result, variable_result]
            .into_iter()
            .flatten()
            .collect();

    results.sort_by(|path_a, path_b| path_a.len().cmp(&path_b.len()));

    match results.pop() {
        Some(mut path) => {
            let last = path.pop().unwrap();
            let must_end_path = match last {
                midend::symtab::DefPathComponent::Variable(_)
                | midend::symtab::DefPathComponent::Function(_) => true,
                _ => false,
            };
            expr_path.push(last).unwrap();
            Ok(must_end_path)
        }
        None => Err(format!(
            "cannot find symbol {} in scope {}",
            ident, expr_path
        )),
    }
}

fn record_monomorphization(
    ctx: &mut midend::treewalk::LinearizeCtx,
    path: &midend::symtab::DefPath,
    maybe_generics: Option<ast::generics::GenericArgsListTree>,
) {
    // TODO: checking for correct number of params
    let generics = match maybe_generics {
        Some(g) => g.linearize(ctx),
        None => return,
    };

    use midend::symtab::DefPathComponent;

    let generic_params = match path.last() {
        DefPathComponent::Type(_) => ctx
            .lookup_at::<midend::symtab::TypeDefinition>(path)
            .unwrap()
            .generic_params()
            .clone(),
        DefPathComponent::Function(_) => ctx
            .lookup_at::<midend::symtab::Function>(path)
            .unwrap()
            .prototype
            .generic_params
            .clone(),
        _ => panic!(),
    };

    // bare-minimum assertion that param counts are correct
    if generics.len() != generic_params.len() {
        panic!(
            "expected {} params for {} ({:?}), only found {}",
            generic_params.len(),
            path,
            generic_params,
            generics.len()
        );
    }

    let substs =
        midend::types::ParamSubstMap::new(generic_params.into_iter().zip(generics.into_iter()));
    ctx.symtab_mut()
        .types
        .record_monomorphization(path.clone(), substs)
        .unwrap();
}
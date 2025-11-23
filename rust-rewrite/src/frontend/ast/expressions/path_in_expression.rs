use serde::{Deserialize, Serialize};

use crate::{
    frontend::{ast::*, *},
    midend::treewalk::Linearize,
    trace,
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum PathIdentSegment {
    Ident(SourceLoc, String),
    Super(SourceLoc),
    SelfLower(SourceLoc),
    SelfUpper(SourceLoc),
}

impl PathIdentSegment {
    pub fn loc(&self) -> &SourceLoc {
        match self {
            Self::Ident(loc, _) => loc,
            Self::Super(loc) => loc,
            Self::SelfUpper(loc) => loc,
            Self::SelfLower(loc) => loc,
        }
    }
}

impl std::fmt::Display for PathIdentSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PathIdentSegment::Ident(_, ident) => write!(f, "{}", ident),
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

impl PathExprSegmentTree {
    pub fn loc(&self) -> &SourceLoc {
        self.ident.loc()
    }
}

impl PathExprSegmentTree {
    fn expect_no_generics(self) -> Result<(), String> {
        match self.generic_args {
            Some(args) => Err(format!("found generic args at {}, expected none", args.loc)),
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
    pub loc: SourceLoc,
    pub segments: Vec<PathExprSegmentTree>,
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
    ctx: &mut treewalk::LinearizeCtx,
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
    ctx: &mut treewalk::LinearizeCtx,
    path: &midend::symtab::DefPath,
    maybe_generics: Option<ast::generics::GenericArgsListTree>,
) {
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

impl treewalk::Linearize<midend::ir::ValueId> for PathInExpressionTree {
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let _span = trace::span_auto_debug!(
            "treewalk::linearize for PathInexpressionTree @",
            "{}",
            self.loc
        );
        let mut expr_path = midend::symtab::DefPath::empty();
        let mut segments = self.segments.into_iter();
        while let Some(segment) = segments.next() {
            trace::warning!("{}", expr_path);
            match segment.ident {
                PathIdentSegment::Super(_) => {
                    match expr_path.pop() {
                        Some(_) => (),
                        None => panic!(
                            "path segment 'Super' on invalid/empty path at {}",
                            segment.loc()
                        ),
                    }
                    segment.expect_no_generics().unwrap();
                }
                PathIdentSegment::Ident(_, name) => {
                    let must_end = walk_ident_segment(name, &mut expr_path, ctx).unwrap();
                    record_monomorphization(ctx, &expr_path, segment.generic_args);
                    if must_end && (segments.size_hint().0 > 0) {
                        panic!(
                            "path {} (ends with {}) has additional unexpected segments",
                            expr_path,
                            expr_path.last().name()
                        )
                    }
                }
                PathIdentSegment::SelfLower(loc) => {
                    let must_end =
                        walk_ident_segment(String::from("self"), &mut expr_path, ctx).unwrap();
                    record_monomorphization(ctx, &expr_path, segment.generic_args);
                    if must_end && (segments.size_hint().0 > 0) {
                        panic!(
                            "path {} at {} (ends with {}) has additional unexpected segments",
                            expr_path,
                            loc,
                            expr_path.last().name()
                        )
                    }
                }
                PathIdentSegment::SelfUpper(loc) => {
                    unimplemented!("path segment 'Self' at {} not supported", loc);
                }
            }
            trace::warning!("iteration end path: {}", expr_path);
        }
        midend::ir::ValueId::new(1)
    }
}

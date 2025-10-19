use serde::{Deserialize, Serialize};

use crate::frontend::{ast::*, *};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum PathIdentSegment {
    Ident(String),
    Super,
    SelfLower,
    SelfUpper,
}

impl std::fmt::Display for PathIdentSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PathIdentSegment::Ident(ident) => write!(f, "{}", ident),
            PathIdentSegment::Super => write!(f, "super"),
            PathIdentSegment::SelfLower => write!(f, "self"),
            PathIdentSegment::SelfUpper => write!(f, "Self"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathExprSegmentTree {
    pub loc: SourceLoc,
    pub ident: PathIdentSegment,
    pub generic_args: Option<ast::generics::GenericArgsListTree>,
}

impl PathExprSegmentTree {
    fn expect_no_generics(&self) -> Result<(), String> {
        match &self.generic_args {
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
) -> Result<(), String> {
    let mut search_def_path = ctx.def_path().clone();

    loop {
        if let Ok(search_subpath) = search_def_path.clone().join(expr_path.clone()) {
            let function_component = midend::symtab::DefPathComponent::Function(
                midend::symtab::FunctionName::new(ident.clone()),
            );
            if let Ok(function_path) = search_subpath
                .clone()
                .with_component(function_component.clone())
            {
                if let Ok(_) = ctx.symtab().lookup_decl_at(&function_path) {
                    expr_path.push(function_component).unwrap();
                    return Ok(());
                }
            }

            let variable_component = midend::symtab::DefPathComponent::Variable(ident.clone());
            if let Ok(variable_path) = search_subpath
                .clone()
                .with_component(variable_component.clone())
            {
                if let Ok(_) = ctx.symtab().lookup_decl_at(&variable_path) {
                    expr_path.push(variable_component).unwrap();
                    return Ok(());
                }
            }
        }

        if search_def_path.len() == 0 {
            break;
        } else {
            search_def_path.pop().unwrap();
        }
    }

    Err(format!(
        "cannot find function or variable {} in scope {}",
        ident, expr_path
    ))
}

impl treewalk::Linearize<midend::ir::ValueId> for PathInExpressionTree {
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
        let mut expr_path = midend::symtab::DefPath::empty();
        for segment in self.segments.into_iter() {
            match segment.ident {
                PathIdentSegment::Super => {
                    match expr_path.pop() {
                        Some(_) => (),
                        None => panic!(
                            "path segment 'Super' on invalid/empty path at {}",
                            segment.loc
                        ),
                    }
                    segment.expect_no_generics().unwrap();
                }
                PathIdentSegment::Ident(name) => {
                    walk_ident_segment(name, &mut expr_path, ctx).unwrap();
                }
                PathIdentSegment::SelfLower => {
                    walk_ident_segment(String::from("self"), &mut expr_path, ctx).unwrap();
                }
                PathIdentSegment::SelfUpper => {}
            }
        }
        midend::ir::ValueId::new(1)
    }
}

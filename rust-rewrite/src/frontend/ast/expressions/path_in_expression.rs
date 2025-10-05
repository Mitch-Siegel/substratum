use serde::{Deserialize, Serialize};

use crate::frontend::{ast::*, *};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum PathIdentSegment {
    Ident(String),
    Super,
}

impl std::fmt::Display for PathIdentSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            PathIdentSegment::Ident(ident) => write!(f, "{}", ident),
            PathIdentSegment::Super => write!(f, "super"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathIdentSegmentTree {
    pub loc: SourceLoc,
    pub ident: PathIdentSegment,
}

impl std::fmt::Display for PathIdentSegmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.ident)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathExprSegmentTree {
    pub loc: SourceLoc,
    pub ident_tree: PathIdentSegmentTree,
    pub generic_args: Option<ast::generics::GenericParamsListTree>,
}

impl std::fmt::Display for PathExprSegmentTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.ident_tree)?;
        if let Some(args) = &self.generic_args {
            write!(f, "<{}>", args)?;
        }
        Ok(())
    }
}

impl
    midend::linearizer::CustomWalk<
        (
            &mut midend::linearizer::WalkContext,
            &mut midend::symtab::DefPath,
        ),
        (),
    > for PathExprSegmentTree
{
    fn walk(
        self,
        (_ctx, current_path_expr): (
            &mut midend::linearizer::WalkContext,
            &mut midend::symtab::DefPath,
        ),
    ) -> () {
        match self.ident_tree.ident {
            PathIdentSegment::Ident(ident) => {
                let next_component = if let Some(_generic_args) = self.generic_args {
                    midend::symtab::DefPathComponent::Type(midend::types::Syntactic::Named(ident))
                } else {
                    midend::symtab::DefPathComponent::Module(midend::symtab::ModuleName::new(ident))
                };

                current_path_expr.push(next_component).unwrap();
            }
            PathIdentSegment::Super => {
                current_path_expr.pop().expect(&format!(
                    "pathidentsegment 'super' at {} is operating on empty path",
                    self.loc
                ));
            }
        }
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

pub enum ResolvedPath {
    _Local(midend::ir::ValueId),       // a binding to a local value
    _General(midend::symtab::DefPath), // a relative defpath to look up, potentially scoped under the context's current
    // def path or any of its parent paths
    _Global(midend::symtab::DefPath), // an absolute defpath to look up
}

impl midend::linearizer::CustomWalk<&mut midend::linearizer::WalkContext, ResolvedPath>
    for PathInExpressionTree
{
    fn walk(self, _ctx: &mut midend::linearizer::WalkContext) -> ResolvedPath {
        unimplemented!();

        /*
        let mut expr_path = ctx.def_path().clone();
        let first = &self.segments[0].ident_tree.ident;
        match first {
            PathIdentSegment::Super => {
                expr_path.pop().unwrap();
                for segment in self.segments {
                    segment.walk((ctx, &mut expr_path));
                }

                ResolvedPath::General(expr_path)
            }
            PathIdentSegment::Ident(name) => {
                if self.segments.len() == 1 {
                    ResolvedPath::Local(ctx.lookup::<symtab::Variable>(name)?)
                } else {
                    unimplemented!()
                }
                ResolvedPath::Local(
                ctx.lookup::<midend::symtab::Variable>(name))

                let mut defpath = ctx.def_path().clone();
                for segment in self.segments {
                    segment.walk((ctx, &mut defpath));
                }
                ResolvedPath::General(defpath)
            }
        }*/
    }
}

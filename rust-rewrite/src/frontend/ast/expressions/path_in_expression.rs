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
    pub generic_args: Option<ast::generics::GenericArgsListTree>,
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

impl treewalk::Linearize<midend::ir::ValueId> for PathInExpressionTree {
    fn linearize(self, _ctx: &mut treewalk::LinearizeCtx) -> midend::ir::ValueId {
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

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

use crate::{
    frontend::ast::*,
    midend::{self, treewalk::PathableContext},
};

pub enum PathSegmentAction<T> {
    Crate(Option<T>),
    Super(Option<T>),
    Ident(String, Option<T>),
    SelfLower(Option<T>),
    SelfUpper(Option<T>),
}

impl<T> std::fmt::Display for PathSegmentAction<T>
where
    T: std::fmt::Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let maybe_data = match self {
            Self::Crate(d) => {
                write!(f, "Crate")?;
                d
            }
            Self::Super(d) => {
                write!(f, "Super")?;
                d
            }
            Self::Ident(i, d) => {
                write!(f, "{}", i)?;
                d
            }
            Self::SelfLower(d) => {
                write!(f, "self")?;
                d
            }
            Self::SelfUpper(d) => {
                write!(f, "Self")?;
                d
            }
        };

        if let Some(data) = maybe_data {
            write!(f, "::{}", data)?;
        }
        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum IdentSegment {
    Ident(IdentifierTree),
    Super(sourceloc::SourceSpan),
    SelfLower(sourceloc::SourceSpan),
    SelfUpper(sourceloc::SourceSpan),
}

impl Ast for IdentSegment {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Ident(ident) => ident.loc(),
            Self::Super(loc) => loc.clone(),
            Self::SelfUpper(loc) => loc.clone(),
            Self::SelfLower(loc) => loc.clone(),
        }
    }
}

impl std::fmt::Display for IdentSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            IdentSegment::Ident(ident) => write!(f, "{}", ident),
            IdentSegment::Super(_) => write!(f, "super"),
            IdentSegment::SelfLower(_) => write!(f, "self"),
            IdentSegment::SelfUpper(_) => write!(f, "Self"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathSegmentTree<T>
where
    T: Ast,
{
    pub ident: IdentSegment,
    pub data: Option<T>,
}

impl<T> Ast for PathSegmentTree<T>
where
    T: Ast,
{
    fn loc(&self) -> sourceloc::SourceSpan {
        match &self.data {
            Some(data) => self.ident.loc().merge(&data.loc()).unwrap(),
            None => self.ident.loc(),
        }
    }
}

impl<T> midend::treewalk::Linearize for PathSegmentTree<T>
where
    T: Ast,
{
    type Data = PathSegmentAction<T>;
    fn linearize(
        self,
        ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<PathSegmentAction<T>> {
        let (action, ctx) = match self.ident {
            IdentSegment::Super(_) => (PathSegmentAction::Super(self.data), ctx.take()),
            IdentSegment::Ident(ident) => {
                let (name, ctx) = ident.linearize(ctx)?;
                (PathSegmentAction::Ident(name, self.data), ctx)
            }
            IdentSegment::SelfLower(_) => (PathSegmentAction::SelfLower(self.data), ctx.take()),
            IdentSegment::SelfUpper(_) => (PathSegmentAction::SelfUpper(self.data), ctx.take()),
        };

        ctx.into_result(action)
    }
}

impl<T> std::fmt::Display for PathSegmentTree<T>
where
    T: Ast + Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.ident)?;
        if let Some(args) = &self.data {
            write!(f, "<{}>", args)?;
        }
        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PathTree<T>
where
    T: Ast,
{
    pub starts_global: Option<sourceloc::SourceSpan>,
    pub segments: Vec<PathSegmentTree<T>>,
}

impl<T> Ast for PathTree<T>
where
    T: Ast,
{
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut seg_iter = self.segments.iter();
        let mut loc_span = seg_iter
            .next()
            .expect("PathTree must have at least one segment")
            .loc();

        while let Some(segment) = seg_iter.next() {
            loc_span = loc_span.merge(&segment.loc()).unwrap();
        }

        loc_span
    }
}

impl<T> Display for PathTree<T>
where
    T: Ast + Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.starts_global.is_some() {
            write!(f, "::")?;
        }

        let mut first = true;
        for segment in &self.segments {
            if first {
                write!(f, "{}", segment)?;
                first = false;
            } else {
                write!(f, "::{}", segment)?;
            }
        }

        Ok(())
    }
}

mod path_walk {
    use crate::{
        frontend::ast::path::*,
        midend::{self, symtab::Symtab},
    };
    use std::collections::HashMap;

    #[derive(Debug)]
    pub enum PathWalkError {
        NoSuper,
        SuperInvalid,
        AlreadyDidExclFirst,
    }

    #[derive(Debug)]
    pub struct PathWalkCtx<T>
    where
        T: std::fmt::Debug,
    {
        context_segments: Vec<midend::symtab::PathSegment>,
        walked_segments: Vec<midend::symtab::PathSegment>,
        walked_segment_data: Vec<Option<T>>,
        did_excl_first: bool,
    }

    impl<T> PathWalkCtx<T>
    where
        T: std::fmt::Debug,
    {
        pub fn new(context_segments: Vec<midend::symtab::PathSegment>) -> Self {
            Self {
                context_segments,
                walked_segments: Vec::new(),
                walked_segment_data: Vec::new(),
                did_excl_first: false,
            }
        }

        pub fn do_crate(&mut self) -> Result<(), PathWalkError> {
            if !self.did_excl_first {
                self.context_segments.clear();
                self.did_excl_first = true;
                Ok(())
            } else {
                Err(PathWalkError::AlreadyDidExclFirst)
            }
        }

        pub fn do_super(&mut self) -> Result<midend::symtab::PathSegment, PathWalkError> {
            if self.walked_segments.len() > 0 {
                return Err(PathWalkError::SuperInvalid);
            }

            match self.context_segments.pop() {
                Some(segment) => Ok(segment),
                None => Err(PathWalkError::NoSuper),
            }
        }

        pub fn do_self_upper(&mut self) -> Result<(), PathWalkError> {
            unimplemented!();
        }

        pub fn _do_self_lower(&mut self) -> Result<(), PathWalkError> {
            unimplemented!();
        }

        pub fn add_segment(&mut self, segment: midend::symtab::PathSegment, maybe_data: Option<T>) {
            self.walked_segments.push(segment);
            self.walked_segment_data.push(maybe_data);
        }

        pub fn finish(
            self,
            symtab: &Box<midend::symtab::SymbolTable>,
            segment_name: String,
            maybe_data: Option<T>,
        ) -> FinishedPathWalk<T> {
            let mut prefix_segments = self.context_segments;
            let mut pathed_data = HashMap::new();
            for (segment, maybe_data) in self
                .walked_segments
                .into_iter()
                .zip(self.walked_segment_data.into_iter())
            {
                if let Some(data) = maybe_data {
                    let data_path =
                        midend::symtab::DefPath::new(prefix_segments.clone(), segment.clone());
                    pathed_data.insert(data_path, data);
                }

                prefix_segments.push(segment);
            }

            let type_path = midend::symtab::DefPath::new(
                prefix_segments.clone(),
                midend::symtab::PathSegment::Type(segment_name.clone()),
            );

            let value_path = midend::symtab::DefPath::new(
                prefix_segments.clone(),
                midend::symtab::PathSegment::Value(segment_name.clone()),
            );

            let macro_path = midend::symtab::DefPath::new(
                prefix_segments.clone(),
                midend::symtab::PathSegment::Macro(segment_name.clone()),
            );

            let type_path = match symtab.lookup_at(&type_path) {
                Ok(_) => Some(type_path),
                Err(_) => None,
            };

            let value_path = match symtab.lookup_at(&value_path) {
                Ok(_) => Some(value_path),
                Err(_) => None,
            };

            let macro_path = match symtab.lookup_at(&macro_path) {
                Ok(_) => Some(macro_path),
                Err(_) => None,
            };

            FinishedPathWalk {
                prefix_segments,
                pathed_data,
                last_segment_data: maybe_data,
                type_path,
                value_path,
                macro_path,
            }
        }
    }
}

use path_walk::PathWalkCtx;

#[derive(Debug)]
pub struct FinishedPathWalk<T>
where
    T: std::fmt::Debug,
{
    prefix_segments: Vec<midend::symtab::PathSegment>,
    pathed_data: HashMap<midend::symtab::DefPath, T>,
    last_segment_data: Option<T>,
    type_path: Option<midend::symtab::DefPath>,
    value_path: Option<midend::symtab::DefPath>,
    macro_path: Option<midend::symtab::DefPath>,
}

impl<T> FinishedPathWalk<T>
where
    T: std::fmt::Debug,
{
    pub fn as_type(self) -> Result<midend::symtab::DefPath, ()> {
        unimplemented!();
    }

    pub fn as_value(self) -> Result<midend::symtab::DefPath, ()> {
        unimplemented!();
    }

    pub fn as_macro(self) -> Result<midend::symtab::DefPath, ()> {
        unimplemented!();
    }
}

#[derive(Debug)]
enum PathWalkState<T>
where
    T: std::fmt::Debug,
{
    Start(PathWalkCtx<T>),
    StartGlobal(PathWalkCtx<T>),
    LeadingLowerSupers(PathWalkCtx<T>),
    RequireIdent(PathWalkCtx<T>),
    Finished(FinishedPathWalk<T>),
}

impl<T> PathWalkState<T>
where
    T: Ast + std::fmt::Display + std::fmt::Debug,
{
    fn error(action: PathSegmentAction<T>, loc: sourceloc::SourceSpan) -> ! {
        panic!(
            "path segment {} is not allowed in this position ({})",
            action, loc
        );
    }

    fn start(context_segments: Vec<midend::symtab::PathSegment>) -> Self {
        Self::Start(PathWalkCtx::new(context_segments))
    }

    fn start_global() -> Self {
        Self::StartGlobal(PathWalkCtx::new(Vec::new()))
    }

    fn do_ident(
        mut ctx: PathWalkCtx<T>,
        ident: String,
        maybe_data: Option<T>,
        size_hint: usize,
        symtab: &Box<midend::symtab::SymbolTable>,
    ) -> Self {
        if size_hint > 0 {
            ctx.add_segment(midend::symtab::PathSegment::Type(ident), maybe_data);
            Self::RequireIdent(ctx)
        } else {
            let finished = ctx.finish(symtab, ident, maybe_data);
            Self::Finished(finished)
        }
    }

    fn transition(
        self,
        action: PathSegmentAction<T>,
        size_hint: usize,
        loc: sourceloc::SourceSpan,
        symtab: &Box<midend::symtab::SymbolTable>,
    ) -> Result<Self, String> {
        match self {
            PathWalkState::Start(mut ctx) => match action {
                PathSegmentAction::Crate(_) => {
                    ctx.do_crate().unwrap();
                    Ok(Self::RequireIdent(ctx))
                }
                PathSegmentAction::Super(_) => {
                    ctx.do_super().unwrap();
                    Ok(Self::LeadingLowerSupers(ctx))
                }
                PathSegmentAction::SelfUpper(_) => {
                    ctx.do_self_upper().unwrap();
                    Ok(Self::LeadingLowerSupers(ctx))
                }
                PathSegmentAction::Ident(ident, maybe_data) => {
                    Ok(Self::do_ident(ctx, ident, maybe_data, size_hint, symtab))
                }
                _ => Self::error(action, loc),
            },
            PathWalkState::StartGlobal(ctx) => match action {
                PathSegmentAction::Ident(ident, maybe_data) => {
                    Ok(Self::do_ident(ctx, ident, maybe_data, size_hint, symtab))
                }
                _ => Self::error(action, loc),
            },
            PathWalkState::LeadingLowerSupers(_ctx) => {
                unimplemented!()
                //Self::leading_lower_supers(segment, size_hint, ctx)
            }
            PathWalkState::RequireIdent(_ctx) => unimplemented!(),
            PathWalkState::Finished(_) => Err(String::from("already finished!")),
        }
    }

    pub fn finish(self) -> Result<FinishedPathWalk<T>, String> {
        match self {
            PathWalkState::Finished(state) => Ok(state),
            other => Err(format!("unfinished path walk in sate {:?}", other).into()),
        }
    }
}

impl<T> midend::treewalk::Linearize for PathTree<T>
where
    T: Ast + std::fmt::Display + std::fmt::Debug,
{
    type Data = FinishedPathWalk<T>;
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let ctx_path = ctx.path().clone();
        let ctx_segments = ctx_path.clone().into_iter().collect::<Vec<_>>();
        let mut walk_state = if self.starts_global.is_some() {
            PathWalkState::<T>::start_global()
        } else {
            PathWalkState::<T>::start(ctx_segments)
        };

        let mut segments = self.segments.into_iter();

        while let Some(segment) = segments.next() {
            let segment_loc = segment.loc();
            let (action, unpathed_ctx) = segment.linearize(ctx)?;
            let symtab = unpathed_ctx.take();
            walk_state = walk_state
                .transition(action, segments.size_hint().0, segment_loc, &symtab)
                .unwrap();
            ctx = midend::treewalk::UnpathedLinearizeCtx::new(symtab).with_path(ctx_path.clone());
        }

        let finished = walk_state.finish().unwrap();

        ctx.into_result(finished)
    }
}

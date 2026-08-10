use serde::{Deserialize, Serialize};
use std::collections::HashMap;

use crate::{
    frontend::ast::{sourceloc, treewalk, Ast, Display, IdentifierTree, LinearizeResult},
    midend::{self, treewalk::linearize_context::UnpathedLinearizeCtxTrait},
};

pub(crate) enum PathSegmentAction<T> {
    _Crate(Option<T>),
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
            Self::_Crate(d) => {
                write!(f, "Crate")?;
                d
            }
            Self::Super(d) => {
                write!(f, "Super")?;
                d
            }
            Self::Ident(i, d) => {
                write!(f, "{i}")?;
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
            write!(f, "::{data}")?;
        }
        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub(crate) enum IdentSegment {
    Ident(IdentifierTree),
    Super(sourceloc::SourceSpan),
    SelfLower(sourceloc::SourceSpan),
    SelfUpper(sourceloc::SourceSpan),
}

impl Ast for IdentSegment {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::Ident(ident) => ident.loc(),
            Self::Super(loc) | Self::SelfUpper(loc) | Self::SelfLower(loc) => loc.clone(),
        }
    }
}

impl std::fmt::Display for IdentSegment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Ident(ident) => write!(f, "{ident}"),
            Self::Super(_) => write!(f, "super"),
            Self::SelfLower(_) => write!(f, "self"),
            Self::SelfUpper(_) => write!(f, "Self"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub(crate) struct PathSegmentTree<T>
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

impl<T, U, P, C> midend::treewalk::Linearize<U, P, C> for PathSegmentTree<T>
where
    T: Ast,
    U: UnpathedLinearizeCtxTrait,
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = PathSegmentAction<T>;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (action, ctx) = match self.ident {
            IdentSegment::Super(_) => (PathSegmentAction::Super(self.data), ctx),
            IdentSegment::Ident(ident) => {
                let (name, ctx) = ident.linearize(ctx)?;
                (PathSegmentAction::Ident(name, self.data), ctx)
            }
            IdentSegment::SelfLower(_) => (PathSegmentAction::SelfLower(self.data), ctx),
            IdentSegment::SelfUpper(_) => (PathSegmentAction::SelfUpper(self.data), ctx),
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
            write!(f, "<{args}>")?;
        }
        Ok(())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub(crate) struct PathTree<T>
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

        for segment in seg_iter {
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
                write!(f, "{segment}")?;
                first = false;
            } else {
                write!(f, "::{segment}")?;
            }
        }

        Ok(())
    }
}

mod path_walk {
    use crate::{
        frontend::ast::path::{sourceloc, FinishedPathWalk},
        midend::{self},
    };
    use std::collections::HashMap;

    #[derive(Debug)]
    pub(crate) enum PathWalkError {
        NoSuper,
        SuperInvalid,
        AlreadyDidExclFirst,
    }

    #[derive(Debug)]
    pub(crate) struct PathWalkCtx<T>
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
        pub(crate) fn new(context_segments: Vec<midend::symtab::PathSegment>) -> Self {
            Self {
                context_segments,
                walked_segments: Vec::new(),
                walked_segment_data: Vec::new(),
                did_excl_first: false,
            }
        }

        pub(crate) fn do_crate(&mut self) -> Result<(), PathWalkError> {
            if self.did_excl_first {
                Err(PathWalkError::AlreadyDidExclFirst)
            } else {
                self.context_segments.clear();
                self.did_excl_first = true;
                Ok(())
            }
        }

        pub(crate) fn do_super(&mut self) -> Result<midend::symtab::PathSegment, PathWalkError> {
            if !self.walked_segments.is_empty() {
                return Err(PathWalkError::SuperInvalid);
            }

            // pull all scopes out implicitly
            while let Some(midend::symtab::PathSegment::Scope(_)) = self.context_segments.pop() {}

            match self.context_segments.pop() {
                Some(segment) => Ok(segment),
                None => Err(PathWalkError::NoSuper),
            }
        }

        pub(crate) fn do_self_upper(&self) -> Result<(), PathWalkError> {
            unimplemented!();
        }

        pub(crate) fn _do_self_lower(&self) -> Result<(), PathWalkError> {
            unimplemented!();
        }

        pub(crate) fn add_segment(
            &mut self,
            segment: midend::symtab::PathSegment,
            maybe_data: Option<T>,
        ) {
            self.walked_segments.push(segment);
            self.walked_segment_data.push(maybe_data);
        }

        pub(crate) fn finish(
            self,
            loc: &sourceloc::SourceSpan,
            symtab: &impl midend::symtab::Symtab,
            segment_name: String,
            maybe_data: Option<T>,
        ) -> FinishedPathWalk<T> {
            let context_path =
                midend::symtab::MaybeEmptyPath::from_segments(self.context_segments.into_iter());
            let mut pathed_data = HashMap::new();

            let mut built_data_path = context_path.clone();
            let mut built_walked_path = midend::symtab::MaybeEmptyPath::new();
            for (segment, maybe_data) in self
                .walked_segments
                .into_iter()
                .zip(self.walked_segment_data.into_iter())
            {
                built_walked_path = built_walked_path.with_segment(segment.clone()).unwrap();
                built_data_path = built_data_path.with_segment(segment.clone()).unwrap();
                if let Some(data) = maybe_data {
                    pathed_data.insert(built_data_path.clone().try_into().unwrap(), data);
                }
            }

            let type_path = midend::symtab::RawPath::new(
                built_walked_path.clone().into(),
                midend::symtab::PathSegment::Type(segment_name.clone()),
            );

            let value_path = midend::symtab::RawPath::new(
                built_walked_path.clone().into(),
                midend::symtab::PathSegment::Value(segment_name.clone()),
            );

            let macro_path = midend::symtab::RawPath::new(
                built_walked_path.into(),
                midend::symtab::PathSegment::Macro(segment_name.clone()),
            );

            let type_path: Option<midend::symtab::TypePath> =
                match symtab.lookup_def(context_path.clone().into(), type_path.clone()) {
                    Ok(_) => Some(type_path.into()),
                    Err(_) => None,
                };

            let value_path: Option<midend::symtab::ValuePath> =
                match symtab.lookup_def(context_path.clone().into(), value_path.clone()) {
                    Ok(_) => Some(value_path.into()),
                    Err(_) => None,
                };

            let macro_path: Option<midend::symtab::MacroPath> = match symtab.lookup_at(&macro_path)
            {
                Ok(_) => Some(macro_path.into()),
                Err(_) => None,
            };

            FinishedPathWalk {
                loc: loc.clone(),
                prefix_segments: context_path.into(),
                pathed_data,
                last_ident: segment_name,
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
pub(crate) struct FinishedPathWalk<T>
where
    T: std::fmt::Debug,
{
    loc: sourceloc::SourceSpan,
    prefix_segments: Vec<midend::symtab::PathSegment>,
    pathed_data: HashMap<midend::symtab::RawPath, T>,
    last_ident: String,
    last_segment_data: Option<T>,
    type_path: Option<midend::symtab::TypePath>,
    value_path: Option<midend::symtab::ValuePath>,
    macro_path: Option<midend::symtab::MacroPath>,
}

pub(crate) struct PathWithSegmentData<P: midend::symtab::Path, D> {
    pub path: P,
    pub data: HashMap<midend::symtab::RawPath, D>,
}

impl<T> FinishedPathWalk<T>
where
    T: std::fmt::Debug,
{
    fn handle_last_segment_data(
        mut pathed_data: HashMap<midend::symtab::RawPath, T>,
        path: &impl midend::symtab::Path,
        maybe_data: Option<T>,
    ) -> HashMap<midend::symtab::RawPath, T> {
        if let Some(data) = maybe_data {
            pathed_data.insert(path.clone().into(), data);
        }
        pathed_data
    }

    pub(crate) fn into_type(
        self,
    ) -> Result<PathWithSegmentData<midend::symtab::TypePath, T>, String> {
        let path: midend::symtab::TypePath = self.type_path.ok_or(format!(
            "path {} (@{}) is not valid as type",
            midend::symtab::RawPath::new(
                self.prefix_segments,
                midend::symtab::PathSegment::Type(self.last_ident)
            ),
            self.loc,
        ))?;
        let data = Self::handle_last_segment_data(self.pathed_data, &path, self.last_segment_data);

        Ok(PathWithSegmentData { path, data })
    }

    pub(crate) fn into_value(
        self,
    ) -> Result<PathWithSegmentData<midend::symtab::ValuePath, T>, String> {
        let path: midend::symtab::ValuePath = self.value_path.ok_or(format!(
            "path {} (@{}) is not valid as value",
            midend::symtab::RawPath::new(
                self.prefix_segments,
                midend::symtab::PathSegment::Value(self.last_ident)
            ),
            self.loc,
        ))?;
        let data = Self::handle_last_segment_data(self.pathed_data, &path, self.last_segment_data);

        Ok(PathWithSegmentData { path, data })
    }

    pub(crate) fn _into_macro(
        self,
    ) -> Result<PathWithSegmentData<midend::symtab::MacroPath, T>, String> {
        let path: midend::symtab::MacroPath = self.macro_path.ok_or(format!(
            "path {} (@{}) is not valid as macro",
            midend::symtab::RawPath::new(
                self.prefix_segments,
                midend::symtab::PathSegment::Macro(self.last_ident)
            ),
            self.loc,
        ))?;
        let data = Self::handle_last_segment_data(self.pathed_data, &path, self.last_segment_data);

        Ok(PathWithSegmentData { path, data })
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
    Finished(Box<FinishedPathWalk<T>>),
}

impl<T> PathWalkState<T>
where
    T: Ast + std::fmt::Display + std::fmt::Debug,
{
    fn error(action: &PathSegmentAction<T>, loc: &sourceloc::SourceSpan) -> ! {
        panic!("path segment {action} is not allowed in this position ({loc})");
    }

    fn start(context_segments: Vec<midend::symtab::PathSegment>) -> Self {
        Self::Start(PathWalkCtx::new(context_segments))
    }

    fn start_global() -> Self {
        Self::StartGlobal(PathWalkCtx::new(Vec::new()))
    }

    /// parameters
    /// loc: the span of the entire path including the identifier being handled
    fn do_ident(
        mut ctx: PathWalkCtx<T>,
        loc: &sourceloc::SourceSpan,
        ident: String,
        maybe_data: Option<T>,
        size_hint: usize,
        symtab: &impl midend::symtab::Symtab,
    ) -> Self {
        if size_hint > 0 {
            ctx.add_segment(midend::symtab::TypeSegment(ident).into(), maybe_data);
            Self::RequireIdent(ctx)
        } else {
            let finished = ctx.finish(loc, symtab, ident, maybe_data);
            Self::Finished(Box::new(finished))
        }
    }

    fn transition(
        self,
        path_span: &sourceloc::SourceSpan,
        action: PathSegmentAction<T>,
        size_hint: usize,
        loc: &sourceloc::SourceSpan,
        symtab: &impl midend::symtab::Symtab,
    ) -> Result<Self, String> {
        match self {
            Self::Start(mut ctx) => match action {
                PathSegmentAction::_Crate(_) => {
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
                PathSegmentAction::Ident(ident, maybe_data) => Ok(Self::do_ident(
                    ctx, path_span, ident, maybe_data, size_hint, symtab,
                )),
                PathSegmentAction::SelfLower(_) => Self::error(&action, loc),
            },
            Self::StartGlobal(ctx) => match action {
                PathSegmentAction::Ident(ident, maybe_data) => Ok(Self::do_ident(
                    ctx, path_span, ident, maybe_data, size_hint, symtab,
                )),
                _ => Self::error(&action, loc),
            },
            Self::LeadingLowerSupers(_ctx) => {
                unimplemented!()
                //Self::leading_lower_supers(segment, size_hint, ctx)
            }
            Self::RequireIdent(_ctx) => unimplemented!(),
            Self::Finished(_) => Err(String::from("already finished!")),
        }
    }

    pub(crate) fn finish(self) -> Result<FinishedPathWalk<T>, String> {
        match self {
            Self::Finished(state) => Ok(*state),
            other => Err(format!("unfinished path walk in sate {other:?}")),
        }
    }
}

impl<T, U, P, C> midend::treewalk::Linearize<U, P, C> for PathTree<T>
where
    T: Ast + std::fmt::Display + std::fmt::Debug,
    U: UnpathedLinearizeCtxTrait,
    P: midend::symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = FinishedPathWalk<T>;
    fn linearize_inner(self, mut ctx: C) -> LinearizeResult<Self::Data, U> {
        let ctx_segments = ctx
            .path()
            .clone()
            .into_iter()
            .collect::<Vec<midend::symtab::PathSegment>>();
        let mut walk_state = if self.starts_global.is_some() {
            PathWalkState::<T>::start_global()
        } else {
            PathWalkState::<T>::start(ctx_segments)
        };

        let mut path_span: sourceloc::SourceSpan = self.loc().start().into();
        let mut segments = self.segments.into_iter();

        while let Some(segment) = segments.next() {
            dbg!(&walk_state);
            let segment_loc = segment.loc();
            path_span = path_span.merge(&segment_loc).unwrap();
            let action: PathSegmentAction<T>;
            (action, ctx) = segment.linearize(ctx)?;
            walk_state = walk_state
                .transition(
                    &path_span,
                    action,
                    segments.size_hint().0,
                    &segment_loc,
                    ctx.unpathed(),
                )
                .unwrap();
            dbg!(&walk_state);
        }

        let finished = walk_state.finish().unwrap();

        ctx.into_result(finished)
    }
}

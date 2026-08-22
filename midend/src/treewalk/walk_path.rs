use std::{collections::HashMap, fmt};

use frontend::{
    ast::{self, Ast},
    sourceloc,
};

use crate::{
    symtab,
    treewalk::{Linearize, LinearizeResult, PathedLinearizeCtxTrait, UnpathedLinearizeCtxTrait},
};

pub(crate) enum PathSegmentAction<T> {
    _Crate(Option<T>),
    Super(Option<T>),
    Ident(String, Option<T>),
    SelfLower(Option<T>),
    SelfUpper(Option<T>),
}

impl<T> fmt::Display for PathSegmentAction<T>
where
    T: fmt::Display,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> fmt::Result {
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

impl<T, U, P, C> Linearize<U, P, C> for ast::path::PathSegmentTree<T>
where
    T: ast::Ast,
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = PathSegmentAction<T>;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (action, ctx) = match self.ident {
            ast::path::IdentSegment::Super(_) => (PathSegmentAction::Super(self.data), ctx),
            ast::path::IdentSegment::Ident(ident) => {
                let (name, ctx) = ident.linearize(ctx)?;
                (PathSegmentAction::Ident(name, self.data), ctx)
            }
            ast::path::IdentSegment::SelfLower(_) => (PathSegmentAction::SelfLower(self.data), ctx),
            ast::path::IdentSegment::SelfUpper(_) => (PathSegmentAction::SelfUpper(self.data), ctx),
        };

        ctx.into_result(action)
    }
}

mod path_walk {
    use frontend::sourceloc;

    use super::{FinishedPathWalk, symtab};
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
        context_segments: Vec<symtab::PathSegment>,
        walked_segments: Vec<symtab::PathSegment>,
        walked_segment_data: Vec<Option<T>>,
        did_excl_first: bool,
    }

    impl<T> PathWalkCtx<T>
    where
        T: std::fmt::Debug,
    {
        pub(crate) fn new(context_segments: Vec<symtab::PathSegment>) -> Self {
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

        pub(crate) fn do_super(&mut self) -> Result<symtab::PathSegment, PathWalkError> {
            if !self.walked_segments.is_empty() {
                return Err(PathWalkError::SuperInvalid);
            }

            // pull all scopes out implicitly
            while let Some(symtab::PathSegment::Scope(_)) = self.context_segments.pop() {}

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

        pub(crate) fn add_segment(&mut self, segment: symtab::PathSegment, maybe_data: Option<T>) {
            self.walked_segments.push(segment);
            self.walked_segment_data.push(maybe_data);
        }

        pub(crate) fn finish(
            self,
            loc: &sourceloc::SourceSpan,
            symtab: &impl symtab::Symtab,
            segment_name: String,
            maybe_data: Option<T>,
        ) -> FinishedPathWalk<T> {
            let context_path =
                symtab::MaybeEmptyPath::from_segments(self.context_segments.into_iter());
            let mut pathed_data = HashMap::new();

            let mut built_data_path = context_path.clone();
            let mut built_walked_path = symtab::MaybeEmptyPath::new();
            for (segment, maybe_data) in self
                .walked_segments
                .into_iter()
                .zip(self.walked_segment_data)
            {
                built_walked_path = built_walked_path.with_segment(segment.clone()).unwrap();
                built_data_path = built_data_path.with_segment(segment.clone()).unwrap();
                if let Some(data) = maybe_data {
                    pathed_data.insert(built_data_path.clone().try_into().unwrap(), data);
                }
            }

            let type_path = symtab::RawPath::new(
                built_walked_path.clone().into(),
                symtab::PathSegment::Type(segment_name.clone()),
            );

            let value_path = symtab::RawPath::new(
                built_walked_path.clone().into(),
                symtab::PathSegment::Value(segment_name.clone()),
            );

            let macro_path = symtab::RawPath::new(
                built_walked_path.into(),
                symtab::PathSegment::Macro(segment_name.clone()),
            );

            let type_path: Option<symtab::TypePath> =
                match symtab.lookup_decl(context_path.clone().into(), type_path.clone()) {
                    Ok(_) => Some(type_path.into()),
                    Err(_) => None,
                };

            let value_path: Option<symtab::ValuePath> =
                match symtab.lookup_decl(context_path.clone().into(), value_path.clone()) {
                    Ok(_) => Some(value_path.into()),
                    Err(_) => None,
                };

            let macro_path: Option<symtab::MacroPath> = match symtab.lookup_at(&macro_path) {
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

pub(crate) struct PathWithSegmentData<P: symtab::Path, D> {
    pub path: P,
    pub data: HashMap<symtab::RawPath, D>,
}

impl<T> FinishedPathWalk<T>
where
    T: std::fmt::Debug,
{
    fn handle_last_segment_data(
        mut pathed_data: HashMap<symtab::RawPath, T>,
        path: &impl symtab::Path,
        maybe_data: Option<T>,
    ) -> HashMap<symtab::RawPath, T> {
        if let Some(data) = maybe_data {
            pathed_data.insert(path.clone().into(), data);
        }
        pathed_data
    }

    pub(crate) fn into_type(self) -> Result<PathWithSegmentData<symtab::TypePath, T>, String> {
        let path: symtab::TypePath = self.type_path.ok_or(format!(
            "path {} (@{}) is not valid as type",
            symtab::RawPath::new(
                self.prefix_segments,
                symtab::PathSegment::Type(self.last_ident)
            ),
            self.loc,
        ))?;
        let data = Self::handle_last_segment_data(self.pathed_data, &path, self.last_segment_data);

        Ok(PathWithSegmentData { path, data })
    }

    pub(crate) fn into_value(self) -> Result<PathWithSegmentData<symtab::ValuePath, T>, String> {
        let path: symtab::ValuePath = self.value_path.ok_or(format!(
            "path {} (@{}) is not valid as value",
            symtab::RawPath::new(
                self.prefix_segments,
                symtab::PathSegment::Value(self.last_ident)
            ),
            self.loc,
        ))?;
        let data = Self::handle_last_segment_data(self.pathed_data, &path, self.last_segment_data);

        Ok(PathWithSegmentData { path, data })
    }

    pub(crate) fn _into_macro(self) -> Result<PathWithSegmentData<symtab::MacroPath, T>, String> {
        let path: symtab::MacroPath = self.macro_path.ok_or(format!(
            "path {} (@{}) is not valid as macro",
            symtab::RawPath::new(
                self.prefix_segments,
                symtab::PathSegment::Macro(self.last_ident)
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
    T: ast::Ast + fmt::Display + fmt::Debug,
{
    fn error(action: &PathSegmentAction<T>, loc: &sourceloc::SourceSpan) -> ! {
        panic!("path segment {action} is not allowed in this position ({loc})");
    }

    fn start(context_segments: Vec<symtab::PathSegment>) -> Self {
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
        symtab: &impl symtab::Symtab,
    ) -> Self {
        if size_hint > 0 {
            ctx.add_segment(symtab::TypeSegment(ident).into(), maybe_data);
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
        symtab: &impl symtab::Symtab,
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

impl<T, U, P, C> Linearize<U, P, C> for ast::PathTree<T>
where
    T: ast::Ast + fmt::Display + fmt::Debug,
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data = FinishedPathWalk<T>;
    fn linearize_inner(self, mut ctx: C) -> LinearizeResult<Self::Data, U> {
        let ctx_segments = ctx
            .path()
            .clone()
            .into_iter()
            .collect::<Vec<symtab::PathSegment>>();
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

#[derive(Debug)]
pub(crate) struct FinishedPathWalk<T>
where
    T: std::fmt::Debug,
{
    loc: sourceloc::SourceSpan,
    prefix_segments: Vec<symtab::PathSegment>,
    pathed_data: HashMap<symtab::RawPath, T>,
    last_ident: String,
    last_segment_data: Option<T>,
    type_path: Option<symtab::TypePath>,
    value_path: Option<symtab::ValuePath>,
    macro_path: Option<symtab::MacroPath>,
}

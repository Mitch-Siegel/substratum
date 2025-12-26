use crate::{frontend::ast::*, midend};
use serde::{Deserialize, Serialize};

pub enum PathSegmentAction<T> {
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
        mut ctx: midend::treewalk::LinearizeCtx,
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

pub struct LinearizedPathTree<T> {
    pub segments: Vec<(String, Option<T>)>,
}

impl<T> LinearizedPathTree<T> {
    fn new() -> Self {
        Self {
            segments: Vec::new(),
        }
    }

    fn with_component(mut self, component: String, maybe_data: Option<T>) -> Self {
        self.segments.push((component, maybe_data));

        self
    }

    pub fn map_data<OnData>(self, _on_data: OnData) -> ()
//Result<midend::symtab::RawPath, String>
    //where
    //    OnData: FnMut(&midend::symtab::RawPath, Option<T>),
    {
        unimplemented!();
        /*
        let mut search_path = self.path.clone();
        while search_path.len() > 0 {
            on_data(&search_path, self.segment_data.remove(&search_path));
            search_path.pop().unwrap();
        }

        match self.segment_data.len() {
            0 => Ok(self.path),
            other => Err(format!(
                "{} entries unaccounted for in segment data for path {}",
                other, self.path
            )),
        }
        */
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

pub fn walk_middle_ident_segment(
    _segment_loc: &sourceloc::SourceSpan,
    _ident: String,
    _expr_path: midend::symtab::DefPath,
) -> Result<midend::symtab::DefPath, String> {
    unimplemented!();
    /*
    let type_component = midend::symtab::DefPathComponent::Type(ident);
    match expr_path.with_component(type_component) {
        Ok(new_path) => Ok(new_path),
        Err(e) => Err(e.to_string()),
    }
    */
}

pub fn walk_ident_segment(
    _segment_loc: &sourceloc::SourceSpan,
    _ident: midend::symtab::PathSegment,
    _size_hint: usize,
    _expr_path: midend::symtab::DefPath,
    _ctx: &mut midend::treewalk::LinearizeCtx,
) -> Result<midend::symtab::DefPath, String> {
    unimplemented!();
    /*
    if size_hint == 0 {
        expr_path = expr_path.with_component(ident).unwrap();
    } else {
        walk_middle_ident_segment(segment_loc, ident.raw(), expr_path)
    }
    */
}

enum PathWalkState<T> {
    Start,
    StartGlobal,
    LeadingLowerSupers(LinearizedPathTree<T>),
    RequireIdent(LinearizedPathTree<T>),
}

impl<T> PathWalkState<T>
where
    T: Ast + std::fmt::Display,
{
    fn error(action: PathSegmentAction<T>, loc: sourceloc::SourceSpan) -> ! {
        panic!(
            "path segment {} is not allowed in this position ({})",
            action, loc
        );
    }

    fn start(
        _segment: PathSegmentTree<T>,
        _size_hint: usize,
        _ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> Result<Self, String> {
        unimplemented!();
        /*
        let segment_loc = segment.loc();
        let empty_path = LinearizedPathTree::new();
        let next_state = match segment.linearize(ctx) {
            PathSegmentAction::Ident(ident, maybe_data) => {
                let mut path = walk_ident_segment(
                    &segment_loc,
                    size_hint,
                    ident,
                    empty_path.path.clone(),
                    ctx,
                )?;

                PathWalkState::LeadingLowerSupers(
                    empty_path
                        .with_component(path.pop().unwrap(), maybe_data)
                        .unwrap(),
                )
            }
            PathSegmentAction::SelfLower(maybe_data) => PathWalkState::LeadingLowerSupers(
                empty_path
                    .with_component(
                        midend::symtab::DefPathComponent::Type(midend::types::Syntactic::_Self),
                        maybe_data,
                    )
                    .unwrap(),
            ),
            other => Self::error(other, segment_loc),
        };

        Ok(next_state)
        */
    }

    fn start_global(
        _segment: PathSegmentTree<T>,
        _size_hint: usize,
        _ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> Result<Self, String> {
        unimplemented!();
        /*
        let segment_loc = segment.loc();
        let empty_path = LinearizedPathTree::new();
        let next_state = match segment.linearize(ctx) {
            PathSegmentAction::Ident(ident, maybe_data) => {
                let mut path = walk_ident_segment(
                    &segment_loc,
                    size_hint,
                    ident,
                    empty_path.path.clone(),
                    ctx,
                )?;

                PathWalkState::RequireIdent(
                    empty_path
                        .with_component(path.pop().unwrap(), maybe_data)
                        .unwrap(),
                )
            }
            other => Self::error(other, segment_loc),
        };

        Ok(next_state)
        */
    }

    fn leading_lower_supers(
        _segment: PathSegmentTree<T>,
        _size_hint: usize,
        _state: LinearizedPathTree<T>,
        _ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> Result<Self, String> {
        unimplemented!();
        /*
        let segment_loc = segment.loc();
        let next_state = match segment.linearize(ctx) {
            PathSegmentAction::Ident(ident, maybe_data) => {
                let mut path =
                    walk_ident_segment(&segment_loc, size_hint, ident, state.path.clone(), ctx)?;

                PathWalkState::RequireIdent(
                    state
                        .with_component(path.pop().unwrap(), maybe_data)
                        .unwrap(),
                )
            }
            PathSegmentAction::Super(maybe_data) => {
                PathWalkState::LeadingLowerSupers(state.into_super(maybe_data, segment_loc)?)
            }
            other => Self::error(other, segment_loc),
        };

        Ok(next_state)
        */
    }

    fn require_ident(
        _segment: PathSegmentTree<T>,
        _size_hint: usize,
        _state: LinearizedPathTree<T>,
        _ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> Result<Self, String> {
        unimplemented!();
        /*
        let segment_loc = segment.loc();
        let next_state = match segment.linearize(ctx) {
            PathSegmentAction::Ident(ident, maybe_data) => {
                let mut path =
                    walk_ident_segment(&segment_loc, size_hint, ident, state.path.clone(), ctx)?;

                PathWalkState::RequireIdent(
                    state
                        .with_component(path.pop().unwrap(), maybe_data)
                        .unwrap(),
                )
            }
            other => Self::error(other, segment_loc),
        };

        Ok(next_state)
        */
    }

    fn transition(
        self,
        segment: PathSegmentTree<T>,
        size_hint: usize,
        ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> Result<Self, String> {
        match self {
            PathWalkState::Start => Self::start(segment, size_hint, ctx),
            PathWalkState::StartGlobal => Self::start_global(segment, size_hint, ctx),
            PathWalkState::LeadingLowerSupers(state) => {
                Self::leading_lower_supers(segment, size_hint, state, ctx)
            }
            PathWalkState::RequireIdent(state) => {
                Self::require_ident(segment, size_hint, state, ctx)
            }
        }
    }
}

impl<T> midend::treewalk::Linearize for PathTree<T>
where
    T: Ast + std::fmt::Display,
{
    type Data = LinearizedPathTree<T>;
    fn linearize(
        self,
        mut ctx: midend::treewalk::LinearizeCtx,
    ) -> midend::treewalk::LinearizeResult<Self::Data> {
        let mut walk_state = if self.starts_global.is_some() {
            PathWalkState::<T>::StartGlobal
        } else {
            PathWalkState::<T>::Start
        };

        let mut segments = self.segments.into_iter();
        while let Some(segment) = segments.next() {
            walk_state = walk_state
                .transition(segment, segments.size_hint().0, &mut ctx)
                .unwrap();
        }

        unimplemented!();
    }
}

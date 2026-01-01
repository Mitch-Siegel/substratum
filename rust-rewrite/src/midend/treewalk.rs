use crate::{frontend, midend::*, trace};

pub mod collect_ctx;
pub mod function_linearize_context;
pub mod linearize_context;

pub use collect_ctx::UnpathedCollectCtx;
pub use function_linearize_context::FunctionLinearizeCtx;
pub use linearize_context::UnpathedLinearizeCtx;

pub struct PathedCtx<T>
where
    T: symtab::Symtab,
{
    unpathed: T,
    path: symtab::DefPath,
}

impl<T> std::ops::Deref for PathedCtx<T>
where
    T: symtab::Symtab,
{
    type Target = T;
    fn deref(&self) -> &Self::Target {
        &self.unpathed
    }
}

impl<T> std::ops::DerefMut for PathedCtx<T>
where
    T: symtab::Symtab,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.unpathed
    }
}

impl<T> PathedCtx<T>
where
    T: symtab::Symtab,
{
    pub fn take(self) -> T {
        self.unpathed
    }

    pub fn path(&self) -> &symtab::DefPath {
        &self.path
    }

    pub fn with_path(mut self, path: symtab::DefPath) -> Self {
        self.path = path;
        self
    }

    pub fn with_segment(mut self, segment: symtab::PathSegment) -> Result<Self, symtab::PathError> {
        self.path = self.path.with_segment(segment)?;
        Ok(self)
    }

    #[tracing::instrument(skip(self), level = "debug", fields(path = self.path.to_string()))]
    pub fn declare_type(&mut self, name: String) -> Result<symtab::DefPath, symtab::SymbolError> {
        let full_path = self
            .path
            .clone()
            .with_segment(symtab::PathSegment::Type(name))?;
        self.unpathed.declare(full_path)
    }

    #[tracing::instrument(skip(self), level = "debug", fields(path = self.path.to_string()))]
    pub fn declare_value(&mut self, name: String) -> Result<symtab::DefPath, symtab::SymbolError> {
        let full_path = self
            .path
            .clone()
            .with_segment(symtab::PathSegment::Value(name))?;
        self.unpathed.declare(full_path)
    }

    #[tracing::instrument(skip(self), level = "debug", fields(path = self.path.to_string()))]
    pub fn define_type<S>(&mut self, symbol: S) -> Result<symtab::DefPath, symtab::SymbolError>
    where
        S: std::fmt::Debug,
        symtab::Type: From<S>,
    {
        self.unpathed
            .define(self.path.clone(), symtab::Type::from(symbol).into())
    }

    #[tracing::instrument(skip(self), level = "debug", fields(path = self.path.to_string()))]
    pub fn define_value<S>(&mut self, symbol: S) -> Result<symtab::DefPath, symtab::SymbolError>
    where
        S: std::fmt::Debug,
        symtab::Value: From<S>,
    {
        self.unpathed
            .define(self.path.clone(), symtab::Value::from(symbol).into())
    }
}

pub trait PathableContext
where
    Self: Sized + symtab::Symtab,
{
    fn with_path(self, path: symtab::DefPath) -> PathedCtx<Self> {
        PathedCtx {
            unpathed: self,
            path,
        }
    }
}

pub enum CollectError {
    Symbol(symtab::SymbolError),
}

impl From<symtab::SymbolError> for CollectError {
    fn from(value: symtab::SymbolError) -> Self {
        Self::Symbol(value)
    }
}

impl std::fmt::Debug for CollectError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Symbol(s) => write!(f, "symbol error: {:?}", s),
        }
    }
}

pub type CollectCtx = PathedCtx<UnpathedCollectCtx>;
pub type CollectResult = Result<UnpathedCollectCtx, CollectError>;

pub trait Collect {
    fn collect_symbols(&self, ctx: CollectCtx) -> CollectResult;

    fn collect_same_path(&self, ctx: CollectCtx) -> Result<CollectCtx, CollectError> {
        let old_path = ctx.path().clone();
        Ok(self.collect_symbols(ctx)?.with_path(old_path))
    }
}

#[derive(Debug)]
pub enum LinearizeError {
    Symbol(symtab::SymbolError),
}

impl From<symtab::SymbolError> for LinearizeError {
    fn from(value: symtab::SymbolError) -> Self {
        Self::Symbol(value)
    }
}

pub type LinearizeCtx = PathedCtx<UnpathedLinearizeCtx>;
pub type LinearizeResult<T> = Result<(T, UnpathedLinearizeCtx), LinearizeError>;

impl LinearizeCtx {
    pub fn into_result<T>(self, result_data: T) -> LinearizeResult<T> {
        Ok((result_data, self.unpathed))
    }
}

pub trait Linearize
where
    Self: Sized,
{
    type Data;
    fn linearize(self, ctx: LinearizeCtx) -> LinearizeResult<Self::Data>;

    fn linearize_same_path(
        self,
        ctx: LinearizeCtx,
    ) -> Result<(Self::Data, LinearizeCtx), LinearizeError> {
        let old_path = ctx.path().clone();
        let (data, unpathed) = self.linearize(ctx)?;
        Ok((data, unpathed.with_path(old_path)))
    }
}

pub fn module_prefix_segments(module: &frontend::ast::ModuleTree) -> Vec<symtab::PathSegment> {
    let segments = module
        .module_path
        .iter()
        .map(|segment| symtab::PathSegment::Type(segment.clone()))
        .collect::<Vec<_>>();
    let (_, segments) = segments.split_last().unwrap();

    println!("module prefix segments: {:?}", segments);
    segments.to_owned()
}

pub fn walk(program: Vec<frontend::ast::ModuleTree>) -> Box<symtab::SymbolTable> {
    let mut symtab = Box::new(symtab::SymbolTable::new());

    trace::debug!("collect symbols");

    for module in &program {
        let prefix_segments = module_prefix_segments(module);
        let collect_ctx = UnpathedCollectCtx::new(symtab);

        symtab = module
            .collect_from_prefix_segments(collect_ctx, prefix_segments)
            .unwrap()
            .take();
    }

    for decl in symtab.decls() {
        println!("{}", decl);
    }

    trace::debug!("linearize");

    for module in program {
        let prefix_segments = module_prefix_segments(&module);

        trace::debug!(
            "walk module \"{}\": {:?} (prefix segments {:?})",
            module.name,
            module.module_path,
            prefix_segments
        );
        let linearize_ctx = UnpathedLinearizeCtx::new(symtab);
        let (_, ctx) = module
            .linearize_from_prefix_segments(linearize_ctx, prefix_segments)
            .unwrap();
        symtab = ctx.take();
    }

    symtab
}

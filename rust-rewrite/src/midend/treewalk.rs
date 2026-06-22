use crate::{
    frontend,
    midend::{
        symtab::{self, TypeOwner, ValueOwner},
        *,
    },
    trace,
};

use std::collections::BTreeSet;

pub mod collect_ctx;
pub mod function_linearize_context;
pub mod linearize_context;

pub use collect_ctx::UnpathedCollectCtx;
pub use function_linearize_context::FunctionLinearizeCtx;
pub use linearize_context::UnpathedLinearizeCtx;

pub struct PathedCtx<T, P>
where
    T: symtab::Symtab,
    P: symtab::Path,
{
    unpathed: T,
    path: P,
}

impl<T, P> PathedCtx<T, P>
where
    T: symtab::Symtab,
    P: symtab::Path,
{
    pub fn semantic_type_for_syntactic(
        &self,
        ty_: &types::Syntactic,
    ) -> Result<types::Semantic, symtab::SymbolError> {
        self.unpathed
            .semantic_type_for_syntactic(&self.path, types::ParamSubstMap::empty(), ty_)
    }
}

// impl<T, P> std::ops::Deref for PathedCtx<T, P>
// where
//     T: symtab::Symtab,
//     P: symtab::Path,
// {
//     type Target = T;
//     fn deref(&self) -> &Self::Target {
//         &self.unpathed
//     }
// }

// impl<T, P> std::ops::DerefMut for PathedCtx<T, P>
// where
//     T: symtab::Symtab,
//     P: symtab::Path,
// {
//     fn deref_mut(&mut self) -> &mut Self::Target {
//         &mut self.unpathed
//     }
// }

impl<T, P> PathedCtx<T, P>
where
    T: symtab::Symtab,
    P: symtab::Path,
{
    pub fn take(self) -> T {
        self.unpathed
    }

    pub fn path(&self) -> &P {
        &self.path
    }

    pub fn into_raw(self) -> PathedCtx<T, symtab::RawPath> {
        PathedCtx {
            unpathed: self.unpathed,
            path: self.path.into(),
        }
    }
}

impl<T, P> PathedCtx<T, P>
where
    T: symtab::Symtab,
    P: symtab::TypeOwner,
{
    fn with_child_type(self, child: String) -> PathedCtx<T, symtab::TypePath> {
        let new_path = self.path.with_child_type(child);
        PathedCtx {
            path: new_path,
            unpathed: self.unpathed,
        }
    }

    fn declare_type(&mut self, name: String) -> Result<symtab::TypePath, symtab::SymbolError> {
        self.unpathed
            .declare_type(self.path.clone().with_child_type(name))
    }

    fn define_type(
        &mut self,
        type_: symtab::Type,
    ) -> Result<symtab::TypePath, symtab::SymbolError> {
        self.unpathed.define_type(self.path, type_)
    }
}

impl<T, P> PathedCtx<T, P>
where
    T: symtab::Symtab,
    P: symtab::ValueOwner,
{
    fn with_child_value(self, child: String) -> PathedCtx<T, symtab::ValuePath> {
        let new_path = self.path.with_child_value(child);
        PathedCtx {
            path: new_path,
            unpathed: self.unpathed,
        }
    }

    fn declare_value(&mut self, name: String) -> Result<symtab::ValuePath, symtab::SymbolError> {
        self.unpathed
            .declare_value(self.path.clone().with_child_value(name))
    }

    fn define_value(
        &mut self,
        type_: symtab::Value,
    ) -> Result<symtab::ValuePath, symtab::SymbolError> {
        self.unpathed.define_value(self.path, type_)
    }
}

impl<T, P> PathedCtx<T, P>
where
    T: symtab::Symtab,
    P: symtab::ImplOwner,
{
    pub fn create_impl(
        &mut self,
        for_type: types::Syntactic,
    ) -> Result<symtab::ImplPath, symtab::SymbolError> {
        let (_, for_type_path) = self.unpathed.lookup_type_def(&symtab::TypePath::new(
            Some(self.path.clone()),
            for_type.clone().to_string(),
        ))?;

        self.unpathed
            .create_impl(self.path.clone().into(), for_type_path)
    }
}

impl<T> From<PathedCtx<T, symtab::TypePath>> for PathedCtx<T, symtab::RawPath>
where
    T: symtab::Symtab,
{
    fn from(value: PathedCtx<T, symtab::TypePath>) -> Self {
        PathedCtx {
            unpathed: value.unpathed,
            path: value.path.into(),
        }
    }
}

impl<T> From<PathedCtx<T, symtab::ValuePath>> for PathedCtx<T, symtab::RawPath>
where
    T: symtab::Symtab,
{
    fn from(value: PathedCtx<T, symtab::ValuePath>) -> Self {
        PathedCtx {
            unpathed: value.unpathed,
            path: value.path.into(),
        }
    }
}

impl<P> std::fmt::Debug for PathedCtx<UnpathedLinearizeCtx, P>
where
    P: symtab::Path,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "PathedLinearizeCtx({:?})", self.path)
    }
}

pub struct PathedFunctionContext<T, P>
where
    T: symtab::Symtab,
    P: symtab::Path,
{
    unpathed: T,
    path: P,
    current_function: symtab::values::Function,
}

impl<T, P> PathedFunctionContext<T, P>
where
    T: symtab::Symtab,
    P: symtab::Path,
{
    fn function_mut(&mut self) -> &mut symtab::values::Function {
        &self.current_function
    }
}

pub trait PathableContext
where
    Self: Sized + symtab::Symtab,
{
    fn with_path<P>(self, path: P) -> PathedCtx<Self, P>
    where
        P: symtab::Path,
    {
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

type CollectCtx<T> = PathedCtx<UnpathedCollectCtx, T>;
pub type TypeCollectCtx = CollectCtx<symtab::TypePath>;
pub type ValueCollectCtx = CollectCtx<symtab::ValuePath>;
pub type CollectResult = Result<UnpathedCollectCtx, CollectError>;

pub trait Collect<P>
where
    P: symtab::Path,
{
    fn collect_symbols(&self, ctx: CollectCtx<P>) -> CollectResult;

    /// call collect_symbols(), but return a CollectCtx with the same path as the one passed in
    fn collect_in_place<P2>(&self, ctx: CollectCtx<P2>) -> Result<CollectCtx<P2>, CollectError>
    where
        P2: symtab::Path,
        CollectCtx<P>: From<CollectCtx<P2>>,
    {
        let old_path: P2 = ctx.path().clone();
        let new_ctx: CollectCtx<P> = CollectCtx::<P>::from(ctx);
        let unpathed = self.collect_symbols(new_ctx)?;
        Ok(unpathed.with_path(old_path))
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

pub type LinearizeCtx<P> = PathedCtx<UnpathedLinearizeCtx, P>;
pub type RawLinearizeCtx = LinearizeCtx<symtab::RawPath>;
pub type TypeLinearizeCtx = LinearizeCtx<symtab::TypePath>;
pub type ValueLinearizeCtx = LinearizeCtx<symtab::ValuePath>;
pub type ImplLinearizeCtx = LinearizeCtx<symtab::ImplPath>;
pub type LinearizeResult<T> = Result<(T, UnpathedLinearizeCtx), LinearizeError>;

impl<P> LinearizeCtx<P>
where
    P: symtab::Path,
{
    pub fn into_result<T>(self, result_data: T) -> LinearizeResult<T> {
        Ok((result_data, self.unpathed))
    }
}

impl<P> std::fmt::Display for LinearizeCtx<P>
where
    P: symtab::Path,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "LinearizeCtx({})", self.path)
    }
}

pub trait Linearize<P>
where
    Self: Sized,
    P: symtab::Path,
{
    type Data;
    fn linearize_inner(self, ctx: LinearizeCtx<P>) -> LinearizeResult<Self::Data>;

    /// call linearize(), but return a LinearizeCtx with the same path as the one passed in
    /// works as long as we can convert a LinearizeCtx<P2> to a LinearizeCtx<P>
    fn linearize<P2>(
        self,
        ctx: LinearizeCtx<P2>,
    ) -> Result<(Self::Data, LinearizeCtx<P2>), LinearizeError>
    where
        P2: symtab::Path,
        LinearizeCtx<P>: From<LinearizeCtx<P2>>,
    {
        let old_path: P2 = ctx.path().clone();
        let new_ctx: LinearizeCtx<P> = LinearizeCtx::<P>::from(ctx);
        let (data, unpathed) = self.linearize_inner(new_ctx)?;
        Ok((data, unpathed.with_path(old_path)))
    }

    fn linearize_with_type<P2>(
        self,
        ctx: LinearizeCtx<P2>,
        segment: symtab::TypeSegment,
    ) -> Result<(Self::Data, LinearizeCtx<P2>), LinearizeError>
    where
        P2: symtab::Path,
        LinearizeCtx<P>: From<LinearizeCtx<P2>>,
    {
        let old_path: P2 = ctx.path().clone();
        let new_ctx: LinearizeCtx<P> = LinearizeCtx::<P>::from(ctx);
        let (data, unpathed) = self.linearize_inner(new_ctx)?;
        Ok((data, unpathed.with_path(old_path)))
    }
}

pub fn module_path(module: &frontend::ast::ModuleTree) -> symtab::TypePath {
    let mut module_path_segments = module.module_path.iter();

    // for now, assume that modules are type-only pathed
    // FUTURE: support module declarations within functions (value namespace)
    let mut wip_path = symtab::TypePath::new(
        None::<symtab::TypePath>,
        module_path_segments.next().unwrap().to_string(),
    );
    for segment in module_path_segments {
        wip_path = wip_path.with_child_type(segment.clone());
    }

    wip_path
}

pub fn walk(program: BTreeSet<frontend::ast::ModuleTree>) -> symtab::SymbolTable {
    let mut symtab = symtab::SymbolTable::new();

    trace::debug!("collect symbols");

    for module in &program {
        let prefix_segments = module_path(module);
        let collect_ctx = UnpathedCollectCtx::new(symtab);

        symtab = module
            .collect_from_parent_path(collect_ctx, prefix_segments)
            .unwrap()
            .take();
    }

    for decl in symtab.decls() {
        println!("{}", decl);
    }

    trace::debug!("linearize");

    for module in program {
        let prefix_segments = module_path(&module);

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

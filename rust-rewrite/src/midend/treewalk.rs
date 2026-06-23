use crate::{
    frontend, midend::{
        symtab::{self, Symtab, TypeOwner, ValueOwner}, types::ParamSubstMap, *,
    }, trace,
};

use std::collections::BTreeSet;

pub mod collect_ctx;
pub mod linearize_context;

pub use collect_ctx::UnpathedCollectCtx;
// pub use function_linearize_context::FunctionLinearizeCtx;
pub use linearize_context::{
    FunctionLinearizeCtx, ImplLinearizeCtx, Linearize, LinearizeCtx, LinearizeError,
    LinearizeResult, PathedLinearizeCtxTrait, RawLinearizeCtx, TypeLinearizeCtx,
    UnpathedFunctionLinearizeCtx, UnpathedLinearizeCtx, ValueFunctionLinearizeCtx,
    ValueLinearizeCtx,
};

pub trait UnpathedCtxTrait: symtab::Symtab + Sized {
    fn with_path<P: symtab::Path>(self, path: P) -> PathedCtx<Self, P>;
}

pub struct PathedCtx<U: UnpathedCtxTrait, P: symtab::Path> {
    unpathed: U,
    path: P,
}

pub trait PathedCtxTrait: std::fmt::Debug {
    type Unpathed: UnpathedCtxTrait;
    type Path: symtab::Path;

    fn unpathed(&self) -> &Self::Unpathed;
    fn unpathed_mut(&mut self) -> &mut Self::Unpathed;
    fn path(&self) -> &Self::Path;

    // ===== type handling =====
    fn with_child_type(self, name: String) -> PathedCtx<Self::Unpathed, symtab::TypePath>
    where
        Self::Path: symtab::TypeOwner;

    fn declare_type(&mut self, name: String) -> Result<symtab::TypePath, symtab::SymbolError>
    where
        Self::Path: symtab::TypeOwner,
    {
        let child_type_path = self.path().clone().with_child_type(name);
        self.unpathed_mut().declare_type(child_type_path)
    }

    fn define_type(&mut self, type_: symtab::Type) -> Result<symtab::TypePath, symtab::SymbolError>
    where
        Self::Path: symtab::TypeOwner,
    {
        let cur_path = self.path().clone();
        self.unpathed_mut().define_type(cur_path, type_)
    }

    // ===== value handling =====
    fn with_child_value(self, name: String) -> PathedCtx<Self::Unpathed, symtab::ValuePath>
    where
        Self::Path: ValueOwner;

    fn declare_value(&mut self, name: String) -> Result<symtab::ValuePath, symtab::SymbolError>
    where
        Self::Path: symtab::ValueOwner,
    {
        let child_value_path = self.path().clone().with_child_value(name);
        self.unpathed_mut().declare_value(child_value_path)
    }

    fn define_value(
        &mut self,
        type_: symtab::Value,
    ) -> Result<symtab::ValuePath, symtab::SymbolError>
    where
        Self::Path: symtab::ValueOwner,
    {
        let cur_path = self.path().clone();
        self.unpathed_mut().define_value(cur_path, type_)
    }

    fn semantic_type_for_syntactic(&self, ty_: &types::Syntactic) -> Result<types::Semantic, symtab::SymbolError>{
        self.unpathed().semantic_type_for_syntactic(self.path(), ParamSubstMap::empty(), ty_)
    }

    fn create_impl(&mut self, for_type: types::Syntactic) -> Result<symtab::ImplId, symtab::SymbolError> {
        unimplemented!()
        // let for_type = self.unpathed_mut().semantic_type_for_syntactic(search_def_path, ParamSubstMap::empty(), for_type)?;
        // self.unpathed_mut().create_impl(self.path().clone().into(), for_type)
    }
}

impl<U, P> std::fmt::Debug for PathedCtx<U, P>
where
    U: UnpathedCtxTrait,
    P: symtab::Path,
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}@{}", std::any::type_name::<U>(), self.path)
    }
}

impl<U, P> PathedCtxTrait for PathedCtx<U, P>
where
    U: UnpathedCtxTrait,
    P: symtab::Path,
{
    type Unpathed = U;
    type Path = P;

    fn unpathed(&self) -> &Self::Unpathed {
        &self.unpathed
    }
    fn unpathed_mut(&mut self) -> &mut Self::Unpathed {
        &mut self.unpathed
    }

    fn path(&self) -> &Self::Path {
        &self.path
    }

    fn with_child_type(self, name: String) -> PathedCtx<U, symtab::TypePath>
    where
        Self::Path: symtab::TypeOwner,
    {
        PathedCtx {
            unpathed: self.unpathed,
            path: self.path.with_child_type(name),
        }
    }

    fn with_child_value(self, name: String) -> PathedCtx<U, symtab::ValuePath>
    where
        Self::Path: symtab::ValueOwner,
    {
        PathedCtx {
            unpathed: self.unpathed,
            path: self.path.with_child_value(name),
        }
    }
}

impl<U, P> PathedCtx<U, P>
where
    U: UnpathedCtxTrait,
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

impl<U> From<PathedCtx<U, symtab::TypePath>> for PathedCtx<U, symtab::RawPath>
where
    U: UnpathedCtxTrait,
{
    fn from(value: PathedCtx<U, symtab::TypePath>) -> Self {
        PathedCtx {
            unpathed: value.unpathed,
            path: value.path.into(),
        }
    }
}

impl<U> From<PathedCtx<U, symtab::ValuePath>> for PathedCtx<U, symtab::RawPath>
where
    U: UnpathedCtxTrait,
{
    fn from(value: PathedCtx<U, symtab::ValuePath>) -> Self {
        PathedCtx {
            unpathed: value.unpathed,
            path: value.path.into(),
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

pub type CollectCtx<P: symtab::Path> = PathedCtx<UnpathedCollectCtx, P>;
pub type TypeCollectCtx = CollectCtx<symtab::TypePath>;
pub type ValueCollectCtx = CollectCtx<symtab::ValuePath>;
pub type CollectResult = Result<UnpathedCollectCtx, CollectError>;

pub trait Collect<P>
where
    P: symtab::Path,
{
    fn collect_inner(&self, ctx: CollectCtx<P>) -> CollectResult;

    /// call collect_symbols(), but return a CollectCtx with the same path as the one passed in
    fn collect_symbols(&self, ctx: CollectCtx<P>) -> Result<CollectCtx<P>, CollectError> {
        let old_path: P = ctx.path().clone();
        let unpathed = self.collect_inner(ctx)?;
        Ok(unpathed.with_path(old_path))
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

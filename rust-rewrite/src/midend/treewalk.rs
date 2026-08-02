use crate::{
    frontend,
    midend::{
        symtab::{self, Path, Symbol, Symtab, SymtabBase, TypeOwner, ValueOwner},
        types::ParamSubstMap,
        *,
    },
    trace,
};

use std::collections::BTreeSet;

pub(crate) mod collect_ctx;
pub(crate) mod linearize_context;

pub(crate) use collect_ctx::UnpathedCollectCtx;
// pub(crate) use function_linearize_context::FunctionLinearizeCtx;
pub(crate) use linearize_context::{
    FunctionLinearizeCtx, ImplLinearizeCtx, Linearize, LinearizeCtx, LinearizeError,
    LinearizeResult, PathedLinearizeCtxTrait, UnpathedFunctionLinearizeCtx, UnpathedLinearizeCtx,
    UnpathedLinearizeCtxTrait, ValueLinearizeCtx,
};

pub(crate) trait UnpathedCtxTrait: symtab::Symtab + Sized {
    fn with_path<P: symtab::Path>(self, path: P) -> PathedCtx<Self, P>;
}

pub(crate) struct PathedCtx<U: UnpathedCtxTrait, P: symtab::Path> {
    unpathed: U,
    path: P,
}

pub(crate) trait PathedCtxTrait: std::fmt::Debug {
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
        let type_path = self
            .path()
            .clone()
            .with_child_type(String::from(type_.name()));
        self.unpathed_mut().define_type(type_path, type_)
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
        value: symtab::Value,
    ) -> Result<symtab::ValuePath, symtab::SymbolError>
    where
        Self::Path: symtab::ValueOwner,
    {
        let value_path = self
            .path()
            .clone()
            .with_child_value(String::from(value.name()));
        self.unpathed_mut().define_value(value_path, value)
    }

    fn semantic_type_for_syntactic(
        &self,
        ty_: &types::Syntactic,
    ) -> Result<types::Semantic, symtab::SymbolError> {
        self.unpathed()
            .semantic_type_for_syntactic(self.path(), ParamSubstMap::empty(), ty_)
    }

    fn create_impl(
        &mut self,
        _for_type: types::Syntactic,
    ) -> Result<symtab::ImplId, symtab::SymbolError> {
        unimplemented!()
        // let for_type = self.unpathed_mut().semantic_type_for_syntactic(search_def_path, ParamSubstMap::empty(), for_type)?;
        // self.unpathed_mut().create_impl(self.path().clone().into(), for_type)
    }

    fn insert_use_declaration(&mut self, use_declaration: symtab::UseDeclaration) {
        let path: symtab::RawPath = self.path().clone().into();
        self.unpathed_mut()
            .insert_use_declaration(path, use_declaration);
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
    pub(crate) fn semantic_type_for_syntactic(
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

pub(crate) enum CollectError {
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

pub(crate) type CollectCtx<P> = PathedCtx<UnpathedCollectCtx, P>;
pub(crate) type TypeCollectCtx = CollectCtx<symtab::TypePath>;
pub(crate) type ValueCollectCtx = CollectCtx<symtab::ValuePath>;
pub(crate) type CollectResult = Result<UnpathedCollectCtx, CollectError>;

pub(crate) trait Collect<P>
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

pub(crate) fn module_path(module: &frontend::ast::ModuleTree) -> symtab::TypePath {
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

pub(crate) fn walk(
    program: BTreeSet<frontend::ast::ModuleTree>,
    crate_name: &str,
) -> symtab::SymbolTable {
    let mut symtab = symtab::SymbolTable::new();

    trace::debug!("collect symbols");

    for module in &program {
        let (maybe_prefix_segments, _) = module_path(module).split_last();
        let collect_ctx = UnpathedCollectCtx::new(symtab);
        symtab = if module.name.value == crate_name {
            module.collect_from_crate_root(collect_ctx).unwrap().take()
        } else {
            let prefix_segments: symtab::TypePath = maybe_prefix_segments
                .expect("must have at least crate in module path")
                .into();

            module
                .collect_from_parent_path(collect_ctx.with_path(prefix_segments))
                .unwrap()
                .take()
        }
    }

    for decl in symtab.decls() {
        println!("{}", decl);
    }

    for (path, uses) in symtab.uses() {
        println!(
            "{}: {}",
            path,
            uses.iter()
                .map(|use_| format!("{}", use_))
                .collect::<Vec<String>>()
                .join(",\n\t")
        );
    }

    trace::debug!("linearize");

    for module in program {
        let linearize_ctx = UnpathedLinearizeCtx::new(symtab);
        if module.name.value == crate_name {
            let (_, unpathed) = module
                .linearize_from_crate_root(linearize_ctx, crate_name)
                .unwrap();
            symtab = unpathed.take();
        } else {
            let (maybe_prefix_segments, _) = module_path(&module).split_last();
            let prefix_segments: symtab::TypePath = maybe_prefix_segments
                .expect("must have at least crate in module path")
                .into();

            trace::debug!(
                "walk module \"{}\": {:?} (prefix segments {:?})",
                module.name,
                module.module_path,
                prefix_segments
            );
            let (_, ctx) = module
                .linearize_from_prefix_segments(linearize_ctx.with_path(prefix_segments.clone()))
                .unwrap();
            symtab = ctx.take();
        }
    }

    symtab
}

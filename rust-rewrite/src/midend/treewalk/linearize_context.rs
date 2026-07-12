use crate::{
    frontend::{ast::Expression::PathIn, sourceloc},
    midend::{symtab::Path, treewalk::*},
};

use std::collections::{HashMap, HashSet};

pub mod function_linearize_context;

pub use function_linearize_context::{UnpathedFunctionLinearizeCtx, WipFunction};

pub struct UnpathedLinearizeCtx {
    symtab: symtab::SymbolTable,
}

impl UnpathedCtxTrait for UnpathedLinearizeCtx {
    fn with_path<P: symtab::Path>(self, path: P) -> PathedCtx<Self, P> {
        PathedCtx {
            unpathed: self,
            path,
        }
    }
}

impl UnpathedLinearizeCtx {
    pub fn new(symtab: symtab::SymbolTable) -> Self {
        Self { symtab }
    }

    pub fn from_existing(
        symtab: symtab::SymbolTable,
        definition_path: symtab::RawPath,
        manager: ir::BlockManager,
        block: usize,
    ) -> Self {
        unimplemented!();

        Self { symtab }
    }

    pub fn take(self) -> symtab::SymbolTable {
        self.symtab
    }
}

impl<P: symtab::Path> PathedCtx<UnpathedFunctionLinearizeCtx, P> {
    // pub fn create_function(
    //     &mut self,
    //     _prototype: symtab::values::function::FunctionPrototype,
    // ) -> Result<(), symtab::SymbolError> {
    //     unimplemented!();
    /*
    let unit_type_id = self
        .symtab()
        .semantic_type_for_syntactic(
            &self.definition_path,
            types::ParamSubstMap::empty(),
            &types::Syntactic::Unit,
        )
        .unwrap();

    self.define_at::<symtab::Function>(
        self.def_path().clone(),
        symtab::Function::new(prototype.clone(), None),
    )?;

    let function_path_component = symtab::DefPathComponent::Function(prototype.name.clone());
    self.push_def_path(function_path_component, &prototype.generic_params);

    let arg_def_paths = prototype
        .arguments
        .iter()
        .map(|arg| {
            println!("Handle argument {:?}", arg);
            self.define::<symtab::Variable>(arg.clone()).unwrap()
        })
        .collect();

    match self.functions.insert(
        self.def_path().clone(),
        FunctionLinearizeCtx::new(
            prototype,
            self.def_path().clone(),
            unit_type_id,
            arg_def_paths,
        ),
    ) {
        Some(_) => panic!(
            "Function {} has already been inserted to symtab",
            self.def_path()
        ),
        None => Ok(()),
    }
    */
    // }

    // pub fn finish_function(
    //     &mut self,
    //     _expected_name: String,
    //     _return_value: ir::ValueId,
    // ) -> Result<symtab::values::Function, LinearizeError> {
    //     unimplemented!();
    /*
        let def_path = self.def_path().clone();
        self.function_mut().resolve_final_convergence();
        self.function_mut().ensure_finished().unwrap();
        let function_context = self.functions.remove(&def_path).unwrap();
        let function = self.lookup_at_mut::<symtab::Function>(&def_path).unwrap();
        match function.control_flow.replace(function_context.take()) {
            Some(_) => return Err(()),
            None => (),
        }
        self.pop_def_path(DefPathComponent::Function(expected_name))
            .unwrap();
        Ok(())
    }

    // finish a function which has already had the finish() called once, assert that no branches
    // are open
    pub fn refinish_function(&mut self, expected_name: FunctionName) -> Result<(), ()> {
        let def_path = self.def_path().clone();
        self.function_mut().ensure_finished().unwrap();
        let function_context = self.functions.remove(&def_path).unwrap();
        let function = self.lookup_at_mut::<symtab::Function>(&def_path).unwrap();
        match function.control_flow.replace(function_context.take()) {
            Some(_) => return Err(()),
            None => (),
        }
        self.pop_def_path(DefPathComponent::Function(expected_name))
            .unwrap();
        Ok(())
    */
    // }

    // reserves a subscope, returning its defpath
    pub fn reserve_subscope(&mut self) -> symtab::ValuePath {
        unimplemented!();
        /*
        let next_subscope_index = self
            .symtab()
            .children(&self.def_path())
            .into_iter()
            .filter(|path| match path.last() {
                DefPathComponent::Scope(_) => true,
                _ => false,
            })
            .count();

        self.symtab
            .define::<symtab::Scope>(
                self.def_path().clone(),
                symtab::Scope::new(next_subscope_index),
            )
            .unwrap()
        */
    }

    pub fn self_variable(&self) -> Result<symtab::ValuePath, symtab::SymbolError> {
        unimplemented!();
        /*
        Ok(self
            .lookup_with_path::<symtab::Variable>(&String::from("self"))?
            .1)
        */
    }

    // resolves a string type name to either a defined type or a generic param
    pub fn disambiguate_named_type(
        &self,
        _name: &str,
    ) -> Result<types::Syntactic, symtab::SymbolError> {
        unimplemented!();
        /*
        // first, lookup the type in the Symbol table
        let (mut type_, found_def_path) =
            match self.lookup_with_path::<TypeDefinition>(&types::Syntactic::Named(name.into())) {
                // if we find it, grab its type and defpath, otherwise create a dummy type and path
                Ok((type_definition, def_path)) => {
                    (Some(type_definition.syntactic().clone()), def_path)
                }
                Err(_) => (None, DefPath::empty()),
            };

        // next, search the generics
        // we may search any generic path *longer than* the def path we found a type definition at
        // this covers cases where more deeply scoped generic parameter names shadow more shallowly
        // scoped named type definitions
        let mut search_def_path = self.def_path().clone();
        while search_def_path.len() > found_def_path.len() {
            match self.generics().get(&search_def_path) {
                Some(params) => match params.get(&types::GenericParam::TypeParam(name.into())) {
                    Some(param) => match param {
                        types::GenericParam::TypeParam(type_param_name) => {
                            type_ = Some(types::Syntactic::GenericParam(type_param_name.clone()));
                            break;
                        }
                    },
                    None => (),
                },
                None => (),
            }
            search_def_path.pop().unwrap();
        }

        type_.ok_or(SymbolError::Undefined(
            self.def_path().clone(),
            DefPathComponent::Type(types::Syntactic::Named(name.into())),
        ))
        */
    }

    #[allow(dead_code)]
    fn self_type(&self) -> Option<types::Syntactic> {
        unimplemented!();
        /*
        let mut search_def_path = self.def_path().clone();
        loop {
            match search_def_path.last() {
                // lookup required
                DefPathComponent::Type(_) => {
                    let definition = self.lookup_at::<TypeDefinition>(&search_def_path).unwrap();
                    return Some(definition.syntactic().clone());
                }
                // trivial case - just grab whatever we're implementing for
                DefPathComponent::Implementation(implementation) => {
                    return Some(implementation.implemented_for.clone())
                }
                DefPathComponent::Empty => break,
                _ => {
                    search_def_path.pop().unwrap();
                }
            }
        }

        None
        */
    }
}

impl symtab::SymtabBase for UnpathedLinearizeCtx {
    fn insert(
        &mut self,
        path: symtab::RawPath,
        maybe_symbol: Option<symtab::SymbolDef>,
    ) -> Result<symtab::RawPath, symtab::SymbolError> {
        self.symtab.insert(path, maybe_symbol)
    }

    fn lookup_at(
        &self,
        path: &symtab::RawPath,
    ) -> Result<Option<&symtab::SymbolDef>, symtab::SymbolError> {
        self.symtab.lookup_at(path)
    }

    fn lookup_at_mut(
        &mut self,
        path: &symtab::RawPath,
    ) -> Result<Option<&mut symtab::SymbolDef>, symtab::SymbolError> {
        self.symtab.lookup_at_mut(path)
    }
}

impl symtab::Symtab for UnpathedLinearizeCtx {
    fn get_impls_for(
        &self,
        path: &symtab::TypePath,
    ) -> Result<&HashSet<symtab::ImplPath>, symtab::SymbolError> {
        self.symtab.get_impls_for(path)
    }

    fn create_impl(
        &mut self,
        impl_parent_path: symtab::RawPath,
        impl_for_path: symtab::TypePath,
    ) -> Result<symtab::ImplPath, symtab::SymbolError> {
        self.symtab.create_impl(impl_parent_path, impl_for_path)
    }

    fn semantic_type_for_syntactic(
        &self,
        search_def_path: &impl symtab::Path,
        generic_params: crate::midend::types::ParamSubstMap,
        ty_: &crate::midend::types::Syntactic,
    ) -> Result<crate::midend::types::Semantic, symtab::SymbolError> {
        self.symtab
            .semantic_type_for_syntactic(search_def_path, generic_params, ty_)
    }
}

#[derive(Debug)]
pub enum LinearizeError {
    Symbol(symtab::SymbolError),
    DisallowedInferredType(sourceloc::SourceSpan),
}

impl From<symtab::SymbolError> for LinearizeError {
    fn from(value: symtab::SymbolError) -> Self {
        Self::Symbol(value)
    }
}

pub trait UnpathedLinearizeCtxTrait: UnpathedCtxTrait {}
impl UnpathedLinearizeCtxTrait for UnpathedLinearizeCtx {}

trait LinearizeCtxTrait {
    type Unpathed: UnpathedCtxTrait;
    type Path: symtab::Path;
}

pub type LinearizeCtx<P: symtab::Path> = PathedCtx<UnpathedLinearizeCtx, P>;
// TODO: remove P from this
pub type FunctionLinearizeCtx<P: symtab::Path> = PathedCtx<UnpathedFunctionLinearizeCtx, P>;

pub type RawLinearizeCtx = LinearizeCtx<symtab::RawPath>;
pub type TypeLinearizeCtx = LinearizeCtx<symtab::TypePath>;
pub type ValueLinearizeCtx = LinearizeCtx<symtab::ValuePath>;
pub type ImplLinearizeCtx = LinearizeCtx<symtab::ImplPath>;

pub type LinearizeResult<D, U: UnpathedLinearizeCtxTrait> = Result<(D, U), LinearizeError>;

pub type ValueFunctionLinearizeCtx = FunctionLinearizeCtx<symtab::ValuePath>;

impl<P> PathedCtx<UnpathedLinearizeCtx, P>
where
    P: symtab::Path,
{
    pub fn into_result<T>(self, result_data: T) -> LinearizeResult<T, UnpathedLinearizeCtx> {
        Ok((result_data, self.unpathed))
    }
}
impl<P> PathedCtx<UnpathedFunctionLinearizeCtx, P>
where
    P: symtab::Path,
{
    pub fn into_result<T>(
        self,
        result_data: T,
    ) -> LinearizeResult<T, UnpathedFunctionLinearizeCtx> {
        Ok((result_data, self.unpathed))
    }

    pub fn function_mut(&mut self) -> &mut treewalk::linearize_context::WipFunction {
        unimplemented!();
    }
}

pub trait PathedLinearizeCtxTrait: PathedCtxTrait
where
    Self::Unpathed: UnpathedLinearizeCtxTrait,
{
    fn new(unpathed: Self::Unpathed, path: Self::Path) -> Self;

    fn into_unpathed(self) -> Self::Unpathed;

    fn into_result<R>(self, data: R) -> LinearizeResult<R, Self::Unpathed>;
}

impl<U, P> PathedLinearizeCtxTrait for PathedCtx<U, P>
where
    U: UnpathedCtxTrait + UnpathedLinearizeCtxTrait,
    P: symtab::Path,
{
    fn new(unpathed: Self::Unpathed, path: Self::Path) -> Self {
        Self { unpathed, path }
    }

    fn into_unpathed(self) -> Self::Unpathed {
        self.unpathed
    }

    fn into_result<R>(self, data: R) -> LinearizeResult<R, U> {
        Ok((data, self.unpathed))
    }
}

pub trait Linearize<U, P, C>: Sized
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data;
    fn linearize_inner(self, ctx: C) -> Result<(Self::Data, U), LinearizeError>;

    fn linearize(self, ctx: C) -> Result<(Self::Data, C), LinearizeError> {
        let old_path = ctx.path().clone();
        let (data, unpathed) = self.linearize_inner(ctx)?;
        Ok((data, C::new(unpathed, old_path)))
    }
}

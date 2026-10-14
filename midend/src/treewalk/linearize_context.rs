use frontend::{ast, loc};

use crate::{
    ir::{self, block_manager::branch},
    symtab::{self},
    treewalk::{PathedCtx, PathedCtxTrait, UnpathedCtxTrait},
    types,
};

pub(crate) mod function_linearize_context;

pub(crate) use function_linearize_context::UnpathedFunctionLinearizeCtx;

pub(crate) struct UnpathedLinearizeCtx {
    symtab: symtab::SymbolTable,
    types: types::Interner,
}

impl UnpathedCtxTrait for UnpathedLinearizeCtx {
    fn with_path<P: symtab::Path>(self, path: P) -> PathedCtx<Self, P> {
        PathedCtx {
            unpathed: self,
            path,
        }
    }

    fn symtab(&self) -> &impl symtab::Symtab {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut impl symtab::Symtab {
        &mut self.symtab
    }

    fn symtab_and_types(&mut self) -> (&mut impl symtab::Symtab, &mut types::Interner) {
        (&mut self.symtab, &mut self.types)
    }
}

impl UnpathedLinearizeCtx {
    pub(crate) fn new(symtab: symtab::SymbolTable, types: types::Interner) -> Self {
        Self { symtab, types }
    }

    pub(crate) fn take(self) -> (symtab::SymbolTable, types::Interner) {
        (self.symtab, self.types)
    }
}

impl<B: branch::Branch> PathedCtx<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath> {
    // reserves a subscope, returning its defpath
    pub(crate) fn reserve_subscope(&mut self) -> symtab::ScopePath {
        let path = self.path().clone();
        self.unpathed.reserve_subscope(path)
    }

    #[allow(unused)]
    pub(crate) fn self_variable(&self) -> Result<symtab::ValuePath, symtab::SymbolError> {
        unimplemented!();
        /*
        Ok(self
            .lookup_with_path::<symtab::Variable>(&String::from("self"))?
            .1)
        */
    }

    // resolves a string type name to either a defined type or a generic param
    #[allow(unused)]
    pub(crate) fn disambiguate_named_type(
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

#[derive(Debug)]
#[allow(unused)]
pub(crate) enum LinearizeError {
    Symbol(symtab::SymbolError),
    DisallowedInferredType(loc::Span),
}

impl From<symtab::SymbolError> for LinearizeError {
    fn from(value: symtab::SymbolError) -> Self {
        Self::Symbol(value)
    }
}

pub(crate) trait UnpathedLinearizeCtxTrait: UnpathedCtxTrait {}
impl UnpathedLinearizeCtxTrait for UnpathedLinearizeCtx {}

pub(crate) type LinearizeCtx<P> = PathedCtx<UnpathedLinearizeCtx, P>;
// TODO: remove P from this
pub(crate) type FunctionLinearizeCtx<B> =
    PathedCtx<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath>;
impl FunctionLinearizeCtx<branch::Epilogue> {
    pub(crate) fn take(self) -> (symtab::values::Function, UnpathedLinearizeCtx) {
        self.unpathed.take(&self.path)
    }
}

#[allow(unused)]
pub(crate) type ValueLinearizeCtx = LinearizeCtx<symtab::ValuePath>;
#[allow(unused)]
pub(crate) type ImplLinearizeCtx = LinearizeCtx<symtab::ImplPath>;

pub(crate) type LinearizeResult<D, U> = Result<(D, U), LinearizeError>;

impl<P> PathedCtx<UnpathedLinearizeCtx, P>
where
    P: symtab::Path,
{
    // allow for ergonomics: just stick this at the bottom of linearizing functions
    #[allow(clippy::unnecessary_wraps)]
    pub(crate) fn into_result<T>(self, result_data: T) -> LinearizeResult<T, UnpathedLinearizeCtx> {
        Ok((result_data, self.unpathed))
    }
}
impl<B: branch::Branch> PathedCtx<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath> {
    // allow for ergonomics: just stick this at the bottom of functions that return results
    #[allow(clippy::unnecessary_wraps)]
    pub(crate) fn into_result<T>(
        self,
        result_data: T,
    ) -> LinearizeResult<T, UnpathedFunctionLinearizeCtx<B>> {
        Ok((result_data, self.unpathed))
    }

    pub(crate) fn append_statement_to_current_block(&mut self, statement: ir::IrLine) {
        self.unpathed.append_statement_to_current_block(statement);
    }
}

impl<B: branch::Branch> PathedCtx<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath> {
    pub(crate) fn create_double_conditional_branch(
        self,
        branch_loc: loc::Loc,
        after_loc: loc::Loc,
        true_data: branch::BranchTarget,
        false_data: branch::BranchTarget,
        condition: ir::lowered::JumpCondition,
        parent_scope_def_path: symtab::ScopePath,
    ) -> PathedCtx<
        UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::DoubleBranchTrue>>,
        symtab::ScopePath,
    > {
        PathedCtx {
            unpathed: self.unpathed.create_double_conditional_branch(
                branch_loc,
                after_loc,
                condition,
                true_data,
                false_data,
                parent_scope_def_path,
            ),
            path: self.path,
        }
    }

    pub(crate) fn create_single_conditional_branch(
        self,
        branch_loc: loc::Loc,
        after_loc: loc::Loc,
        true_data: branch::BranchTarget,
        condition: ir::lowered::JumpCondition,
        parent_scope_def_path: symtab::ScopePath,
    ) -> FunctionLinearizeCtx<branch::NestedBranch<B, branch::SingleBranchTrue>> {
        FunctionLinearizeCtx {
            unpathed: self.unpathed.create_single_conditional_branch(
                branch_loc,
                after_loc,
                condition,
                true_data,
                parent_scope_def_path,
            ),
            path: self.path,
        }
    }

    pub(crate) fn create_unconditional_branch(
        self,
        branch_loc: loc::Loc,
        after_loc: loc::Loc,
        parent_def_path: symtab::ScopePath,
        true_scope_def_path: symtab::ScopePath,
    ) -> PathedCtx<
        UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::Unconditional>>,
        symtab::ScopePath,
    > {
        PathedCtx {
            unpathed: self.unpathed.create_unconditional_branch(
                ir::block_manager::branch::BranchDesc::new(branch_loc, after_loc),
                parent_def_path,
                true_scope_def_path,
            ),
            path: self.path,
        }
    }

    #[allow(unused)]
    pub(crate) fn values(&self) -> &ir::ValueInterner<Option<types::Syntactic>> {
        self.unpathed.values()
    }

    pub(crate) fn values_mut(&mut self) -> &mut ir::ValueInterner<Option<types::Syntactic>> {
        self.unpathed.values_mut()
    }

    pub(crate) fn create_loop(
        self,
        desc: branch::BranchDesc,
        top_loc: loc::Loc,
        bottom_loc: loc::Loc,
        after_loc: loc::Loc,
        parent_scope_def_path: symtab::ScopePath,
        loop_scope_def_path: symtab::ScopePath,
    ) -> PathedCtx<
        UnpathedFunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopHeader>>,
        symtab::ScopePath,
    > {
        PathedCtx {
            unpathed: self.unpathed.create_loop(
                desc,
                top_loc,
                bottom_loc,
                after_loc,
                parent_scope_def_path,
                loop_scope_def_path,
            ),
            path: self.path,
        }
    }
}

impl<B: ir::block_manager::branch::Branch>
    FunctionLinearizeCtx<branch::NestedBranch<B, branch::Unconditional>>
{
    pub(crate) fn finish_unconditional_branch(self, loc: loc::Loc) -> FunctionLinearizeCtx<B> {
        FunctionLinearizeCtx {
            unpathed: self.unpathed.finish_unconditional_branch(loc),
            path: self.path,
        }
    }
}

impl<B: branch::Branch> FunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopHeader>> {
    pub(crate) fn finish_loop_header(
        self,
        loc: loc::Loc,
    ) -> FunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopBody>> {
        PathedCtx {
            unpathed: self.unpathed.finish_loop_header(loc),
            path: self.path,
        }
    }

    pub(crate) fn after_loop_label(&self) -> usize {
        self.unpathed.after_loop_label()
    }
}

impl<B: branch::Branch> FunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopBody>> {
    pub(crate) fn finish_loop_body(
        self,
        loc: loc::Loc,
    ) -> FunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopFooter>> {
        PathedCtx {
            unpathed: self.unpathed.finish_loop_body(loc),
            path: self.path,
        }
    }
}

impl<B: branch::Branch> FunctionLinearizeCtx<branch::NestedBranch<B, branch::LoopFooter>> {
    pub(crate) fn finish_loop_footer(self, loc: loc::Loc) -> FunctionLinearizeCtx<B> {
        PathedCtx {
            unpathed: self.unpathed.finish_loop_footer(loc),
            path: self.path,
        }
    }
}

impl<B: ir::block_manager::branch::Branch>
    FunctionLinearizeCtx<branch::NestedBranch<B, branch::DoubleBranchTrue>>
{
    pub(crate) fn finish_true_branch_switch_to_false(
        self,
        loc: loc::Loc,
    ) -> FunctionLinearizeCtx<branch::NestedBranch<B, branch::DoubleBranchFalse>> {
        FunctionLinearizeCtx {
            unpathed: self.unpathed.finish_true_branch_switch_to_false(loc),
            path: self.path,
        }
    }
}

impl<B: ir::block_manager::branch::Branch>
    FunctionLinearizeCtx<branch::NestedBranch<B, branch::DoubleBranchFalse>>
{
    pub(crate) fn finish_false_branch(self, loc: loc::Loc) -> FunctionLinearizeCtx<B> {
        FunctionLinearizeCtx {
            unpathed: self.unpathed.finish_false_branch(loc),
            path: self.path,
        }
    }
}

impl<B: ir::block_manager::branch::Branch>
    FunctionLinearizeCtx<branch::NestedBranch<B, branch::SingleBranchTrue>>
{
    pub(crate) fn finish_true_branch(self, loc: loc::Loc) -> FunctionLinearizeCtx<B> {
        FunctionLinearizeCtx {
            unpathed: self.unpathed.finish_true_branch(loc),
            path: self.path,
        }
    }
}

impl FunctionLinearizeCtx<branch::UnfinishedFunction> {
    pub(crate) fn finish_to_epilogue(
        self,
        loc: loc::Loc,
    ) -> FunctionLinearizeCtx<branch::Epilogue> {
        FunctionLinearizeCtx {
            unpathed: self.unpathed.finish_to_epilogue(&self.path, loc),
            path: self.path,
        }
    }
}

pub(crate) trait PathedLinearizeCtxTrait: PathedCtxTrait
where
    Self::Unpathed: UnpathedLinearizeCtxTrait,
{
    fn new(unpathed: Self::Unpathed, path: Self::Path) -> Self;

    #[allow(unused)]
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

pub(crate) trait Linearize<U, P, C>: Sized + ast::Ast
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
{
    type Data;
    fn linearize_inner(self, ctx: C) -> Result<(Self::Data, U), LinearizeError>;

    fn linearize(self, ctx: C) -> Result<(Self::Data, C), LinearizeError> {
        let _ = trace::span_auto_debug!("Linearize {Self::name()}");
        let old_path = ctx.path().clone();
        let (data, unpathed) = self.linearize_inner(ctx)?;
        Ok((data, C::new(unpathed, old_path)))
    }
}

pub(crate) trait FunctionLinearizeErased: Sized + ast::Ast {
    type Data;
    fn linearize_erased(
        self,
        ctx: FunctionLinearizeCtx<Box<dyn branch::Branch>>,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx<Box<dyn branch::Branch>>>;
}

impl<B: branch::Branch> FunctionLinearizeCtx<B> {
    fn into_erased(self) -> FunctionLinearizeCtx<Box<dyn branch::Branch>> {
        FunctionLinearizeCtx {
            unpathed: self.unpathed.into_erased(),
            path: self.path,
        }
    }
}

impl<B, T> Linearize<UnpathedFunctionLinearizeCtx<B>, symtab::ScopePath, FunctionLinearizeCtx<B>>
    for T
where
    B: branch::Branch,
    T: FunctionLinearizeErased,
{
    type Data = <T as FunctionLinearizeErased>::Data;

    fn linearize_inner(
        self,
        ctx: FunctionLinearizeCtx<B>,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx<B>> {
        let erased_ctx = ctx.into_erased();

        let (data, erased_ctx) = self.linearize_erased(erased_ctx)?;

        let UnpathedFunctionLinearizeCtx {
            base,
            function_path,
            prototype,
            block_manager,
        } = erased_ctx;

        let original_ctx = UnpathedFunctionLinearizeCtx {
            base,
            function_path,
            prototype,
            block_manager: block_manager.downcast::<B>(),
        };

        Ok((data, original_ctx))
    }
}

use crate::{
    frontend::ast::{types::TypeNoBoundsTree, *},
    midend::{
        symtab::{self, TypePath, ValueOwner},
        treewalk::{PathedCtxTrait, PathedLinearizeCtxTrait},
    },
};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct TupleDataTree {
    pub open_paren_loc: sourceloc::SourceSpan,
    pub element_types: Vec<TypeTree>,
    pub close_paren_loc: sourceloc::SourceSpan,
}

impl Ast for TupleDataTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        self.open_paren_loc
            .clone()
            .merge(&self.close_paren_loc)
            .unwrap()
    }
}

impl<P, C> treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for TupleDataTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
    TypeNoBoundsTree:
        treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C, Data = midend::types::Syntactic>,
{
    type Data = midend::symtab::types::EnumVariantRepr;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> midend::treewalk::LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let mut element_types = Vec::new();
        for element in self.element_types {
            let element_type: midend::types::Syntactic;
            (element_type, ctx) = element.linearize(ctx)?;

            element_types.push(element_type);
        }

        ctx.into_result(midend::symtab::types::EnumVariantRepr::Tuple(element_types))
    }
}

impl<P, C> treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C> for TupleDataTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = P,
    >,
    TypeNoBoundsTree: treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        P,
        C,
        Data = Option<midend::types::Syntactic>,
    >,
{
    type Data = midend::symtab::types::EnumVariantRepr;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> midend::treewalk::LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let mut element_types = Vec::new();
        for element in self.element_types {
            let maybe_element_type: Option<midend::types::Syntactic>;
            (maybe_element_type, ctx) = element.linearize(ctx)?;

            element_types.push(maybe_element_type.expect("tuple members must have types"));
        }

        ctx.into_result(midend::symtab::types::EnumVariantRepr::Tuple(element_types))
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum EnumVariantDataTree {
    TupleData(TupleDataTree),
}

impl Ast for EnumVariantDataTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match self {
            Self::TupleData(tuple) => tuple.loc(),
        }
    }
}

impl<P, C> treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C> for EnumVariantDataTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = P>,
    TypeNoBoundsTree:
        treewalk::Linearize<treewalk::UnpathedLinearizeCtx, P, C, Data = midend::types::Syntactic>,
{
    type Data = midend::symtab::types::EnumVariantRepr;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> midend::treewalk::LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let (variant, ctx) = match self {
            EnumVariantDataTree::TupleData(elements) => elements.linearize(ctx)?,
        };

        ctx.into_result(variant)
    }
}

impl<P, C> treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, P, C> for EnumVariantDataTree
where
    P: symtab::Path,
    C: treewalk::PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = P,
    >,
    TypeNoBoundsTree: treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        P,
        C,
        Data = Option<midend::types::Syntactic>,
    >,
{
    type Data = midend::symtab::types::EnumVariantRepr;
    fn linearize_inner(
        self,
        ctx: C,
    ) -> midend::treewalk::LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let (variant, ctx) = match self {
            EnumVariantDataTree::TupleData(elements) => elements.linearize(ctx)?,
        };

        ctx.into_result(variant)
    }
}

fn _create_enum_variant_constructor(
    mut ctx: midend::treewalk::ImplLinearizeCtx,
    _enum_name: &str,
    variant_name: &str,
    arg_types: Vec<midend::types::Syntactic>,
    loc: sourceloc::SourceLoc,
) {
    // create variables for each argument, named by index
    let args: Vec<midend::symtab::values::Variable> = arg_types
        .into_iter()
        .enumerate()
        .map(|(arg_idx, arg_type)| {
            midend::symtab::values::Variable::new(format!("{}", arg_idx), Some(arg_type))
        })
        .collect();

    // create the function prototype, declare the function, and set up to create IR
    let prototype = midend::symtab::values::function::FunctionPrototype::new(
        String::from(variant_name),
        Vec::new(),
        args,
        midend::types::Syntactic::_Self,
    );

    let ctor_function_path = ctx
        .path()
        .clone()
        .with_child_value(String::from(variant_name));
    ctx.declare_value(String::from(variant_name))
        .expect("Duplicate enum variant constructor");

    let (mut block_mgr, current_block) = midend::ir::BlockManager::new(
        ctx.semantic_type_for_syntactic(&midend::types::Syntactic::Unit)
            .unwrap(),
        ctor_function_path.clone(),
    );

    // define a variable for the object we are building
    let constructed_object = midend::symtab::values::Variable::new(
        "constructed".into(),
        Some(midend::types::Syntactic::_Self),
    );
    let constructed_object_path = ctx
        .define_value(midend::symtab::Value::LocalBinding(
            midend::symtab::values::LocalBinding::Let(constructed_object),
        ))
        .unwrap();

    let constructed_object_value = block_mgr.values_mut().id_for_path(constructed_object_path);

    /*
     * for each argument:
     * define in the symtab
     * get its value
     * get a temp to hold the address of the corresponding field
     * store the argument into the field
     */
    for arg in &prototype.arguments {
        let arg_binding =
            symtab::Value::LocalBinding(symtab::values::LocalBinding::FunctionParam(arg.clone()));
        let arg_def_path = ctx.define_value(arg_binding).unwrap();
        let arg_value = block_mgr.values_mut().id_for_path(arg_def_path);
        let field_temp = block_mgr.values_mut().next_temp();
        let field_get_line = midend::ir::IrLine::new_get_field_pointer(
            loc.clone(),
            constructed_object_value,
            arg.name.clone(),
            field_temp,
        );

        let field_store_line = midend::ir::IrLine::new_store(loc.clone(), arg_value, field_temp);

        block_mgr
            .get_mut(&current_block)
            .unwrap()
            .append(&mut vec![field_get_line, field_store_line]);
    }

    block_mgr.resolve_final_convergence(current_block).unwrap();

    let ctor_function = midend::symtab::values::Function::new(
        prototype,
        Some(midend::ir::ControlFlow::from(block_mgr)),
    );

    ctx.define_value(symtab::Value::Function(Box::new(ctor_function)))
        .unwrap();
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct EnumVariantTree {
    pub name: IdentifierTree,
    pub data: Option<EnumVariantDataTree>,
}

impl Ast for EnumVariantTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        match &self.data {
            Some(data) => self.name.loc().merge(&data.loc()).unwrap(),
            None => self.name.loc(),
        }
    }
}

impl midend::treewalk::Collect<TypePath> for EnumVariantTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.declare_value(self.name.value.clone())?;
        ctx.into_result()
    }
}

impl<C> midend::treewalk::Linearize<treewalk::UnpathedLinearizeCtx, symtab::TypePath, C>
    for EnumVariantTree
where
    C: PathedLinearizeCtxTrait<Unpathed = treewalk::UnpathedLinearizeCtx, Path = symtab::TypePath>,
    TypeNoBoundsTree: treewalk::Linearize<
        treewalk::UnpathedLinearizeCtx,
        symtab::TypePath,
        C,
        Data = midend::types::Syntactic,
    >,
{
    type Data = (String, midend::symtab::types::EnumVariantRepr);
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> midend::treewalk::LinearizeResult<Self::Data, treewalk::UnpathedLinearizeCtx> {
        let variant_data_type;
        (variant_data_type, ctx) = match self.data {
            Some(variant_item) => variant_item.linearize(ctx)?,
            None => (midend::symtab::types::EnumVariantRepr::Unit, ctx),
        };

        let (variant_name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result((variant_name, variant_data_type))
    }
}

impl<C> midend::treewalk::Linearize<treewalk::UnpathedFunctionLinearizeCtx, symtab::TypePath, C>
    for EnumVariantTree
where
    C: PathedLinearizeCtxTrait<
        Unpathed = treewalk::UnpathedFunctionLinearizeCtx,
        Path = symtab::TypePath,
    >,
    TypeNoBoundsTree: treewalk::Linearize<
        treewalk::UnpathedFunctionLinearizeCtx,
        symtab::TypePath,
        C,
        Data = Option<midend::types::Syntactic>,
    >,
{
    type Data = (String, midend::symtab::types::EnumVariantRepr);
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> midend::treewalk::LinearizeResult<Self::Data, treewalk::UnpathedFunctionLinearizeCtx> {
        let variant_data_type;
        (variant_data_type, ctx) = match self.data {
            Some(variant_item) => variant_item.linearize(ctx)?,
            None => (midend::symtab::types::EnumVariantRepr::Unit, ctx),
        };

        let (variant_name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result((variant_name, variant_data_type))
    }
}

impl Display for EnumVariantTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match &self.data {
            Some(variant_data) => write!(f, "{}: {:?}", self.name, variant_data),
            None => write!(f, "{}", self.name),
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct EnumDefinitionTree {
    pub enum_keyword_loc: sourceloc::SourceSpan,
    pub name: IdentifierTree,
    pub generic_params: generics::OptionalGenericParamsListTree,
    pub variants: Vec<EnumVariantTree>,
}

impl Ast for EnumDefinitionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = self
            .enum_keyword_loc
            .clone()
            .merge(&self.name.loc())
            .unwrap();

        loc = loc.merge(&self.generic_params.loc()).unwrap();

        for variant in &self.variants {
            loc = loc.merge(&variant.loc()).unwrap();
        }

        loc
    }
}

impl midend::treewalk::Collect<TypePath> for EnumDefinitionTree {
    fn collect_inner(
        &self,
        mut ctx: midend::treewalk::TypeCollectCtx,
    ) -> midend::treewalk::CollectResult {
        let _enum_path = ctx.declare_type(self.name.value.clone())?;
        ctx = ctx.with_child_type(self.name.value.clone());

        ctx = self.generic_params.collect_symbols(ctx)?;

        for variant in &self.variants {
            ctx = variant.collect_symbols(ctx)?;
        }
        ctx.into_result()
    }
}

impl<U, P, C> treewalk::Linearize<U, P, C> for EnumDefinitionTree
where
    U: treewalk::UnpathedLinearizeCtxTrait,
    P: symtab::Path + symtab::TypeOwner,
    C: treewalk::PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    generics::OptionalGenericParamsListTree: midend::treewalk::Linearize<U, P, C>,
    TypeNoBoundsTree: treewalk::Linearize<U, P, C, Data = midend::types::Syntactic>,
    EnumVariantTree: treewalk::Linearize<
        U,
        symtab::TypePath,
        treewalk::PathedCtx<U, symtab::TypePath>,
        Data = (String, symtab::types::EnumVariantRepr),
    >,
{
    type Data = (
        midend::symtab::types::EnumRepr,
        <generics::OptionalGenericParamsListTree as midend::treewalk::Linearize<U, P, C>>::Data,
    );
    #[tracing::instrument(skip(self, ctx), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize_inner(self, ctx: C) -> midend::treewalk::LinearizeResult<Self::Data, U> {
        let (enum_name, ctx) = self.name.linearize(ctx)?;

        let (generic_params, ctx) = self.generic_params.linearize(ctx)?;
        let mut ctx = ctx.with_child_type(enum_name.clone());
        let _enum_path = ctx.path().clone();

        let mut variants: Vec<(String, midend::symtab::types::EnumVariantRepr)> = Vec::new();
        let _constructor_impl_path =
            ctx.create_impl(midend::types::Syntactic::Named(enum_name.clone()))?;

        // unimplemented!("enum variant constructor");
        for variant in self.variants {
            let _variant_loc = variant.loc();
            let (variant_name, variant_repr): (String, symtab::types::EnumVariantRepr);
            ((variant_name, variant_repr), ctx) = variant.linearize(ctx)?;
            let _arg_types = match &variant_repr {
                midend::symtab::types::EnumVariantRepr::Tuple(types) => types.clone(),
                midend::symtab::types::EnumVariantRepr::Unit => Vec::new(),
            };

            // let (variant_ctor, variant_ctx) = create_enum_variant_constructor(
            //     variant_ctx.with_path(constructor_impl_path.clone()),
            //     &enum_name,
            //     &variant_name,
            //     arg_types,
            //     variant_loc.start(),
            // )?;

            // ctx.define_value(variant_ctor)?;

            variants.push((variant_name, variant_repr));
        }

        let enum_repr = midend::symtab::types::EnumRepr::new(enum_name, variants)
            .expect("duplicate enum variant:");
        ctx.into_result((enum_repr, generic_params))
    }
}

impl Display for EnumDefinitionTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut fields = String::new();
        for variant in &self.variants {
            fields += &variant.to_string();
            fields += " ";
        }

        write!(f, "Enum Definition: {}: {}", self.name, fields)
    }
}

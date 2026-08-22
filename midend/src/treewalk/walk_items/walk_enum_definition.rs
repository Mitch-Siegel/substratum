use frontend::{
    ast::{self, Ast},
    sourceloc,
};

use crate::{
    ir,
    symtab::{self, ValueOwner},
    treewalk::{
        Collect, CollectResult, ImplLinearizeCtx, Linearize, LinearizeResult, PathedCtx,
        PathedCtxTrait, PathedLinearizeCtxTrait, TypeCollectCtx, UnpathedFunctionLinearizeCtx,
        UnpathedLinearizeCtx, UnpathedLinearizeCtxTrait,
    },
    types,
};

impl<P, C> Linearize<UnpathedLinearizeCtx, P, C> for ast::items::enum_definition::TupleDataTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
    ast::types::TypeNoBoundsTree: Linearize<UnpathedLinearizeCtx, P, C, Data = types::Syntactic>,
{
    type Data = symtab::types::EnumVariantRepr;
    fn linearize_inner(self, mut ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let mut element_types = Vec::new();
        for element in self.element_types {
            let element_type: types::Syntactic;
            (element_type, ctx) = element.linearize(ctx)?;

            element_types.push(element_type);
        }

        ctx.into_result(symtab::types::EnumVariantRepr::Tuple(element_types))
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C>
    for ast::items::enum_definition::TupleDataTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
    ast::types::TypeNoBoundsTree:
        Linearize<UnpathedFunctionLinearizeCtx, P, C, Data = Option<types::Syntactic>>,
{
    type Data = symtab::types::EnumVariantRepr;
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let mut element_types = Vec::new();
        for element in self.element_types {
            let maybe_element_type: Option<types::Syntactic>;
            (maybe_element_type, ctx) = element.linearize(ctx)?;

            element_types.push(maybe_element_type.expect("tuple members must have types"));
        }

        ctx.into_result(symtab::types::EnumVariantRepr::Tuple(element_types))
    }
}

impl<P, C> Linearize<UnpathedLinearizeCtx, P, C>
    for ast::items::enum_definition::EnumVariantDataTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = P>,
    ast::types::TypeNoBoundsTree: Linearize<UnpathedLinearizeCtx, P, C, Data = types::Syntactic>,
{
    type Data = symtab::types::EnumVariantRepr;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let (variant, ctx) = match self {
            Self::TupleData(elements) => elements.linearize(ctx)?,
        };

        ctx.into_result(variant)
    }
}

impl<P, C> Linearize<UnpathedFunctionLinearizeCtx, P, C>
    for ast::items::enum_definition::EnumVariantDataTree
where
    P: symtab::Path,
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = P>,
    ast::types::TypeNoBoundsTree:
        Linearize<UnpathedFunctionLinearizeCtx, P, C, Data = Option<types::Syntactic>>,
{
    type Data = symtab::types::EnumVariantRepr;
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let (variant, ctx) = match self {
            Self::TupleData(elements) => elements.linearize(ctx)?,
        };

        ctx.into_result(variant)
    }
}

fn _create_enum_variant_constructor(
    mut ctx: ImplLinearizeCtx,
    _enum_name: &str,
    variant_name: &str,
    arg_types: Vec<types::Syntactic>,
    loc: &sourceloc::SourceLoc,
) {
    // create variables for each argument, named by index
    let args: Vec<symtab::values::Variable> = arg_types
        .into_iter()
        .enumerate()
        .map(|(arg_idx, arg_type)| {
            symtab::values::Variable::new(format!("{arg_idx}"), Some(arg_type))
        })
        .collect();

    // create the function prototype, declare the function, and set up to create IR
    let prototype = symtab::values::function::FunctionPrototype::new(
        String::from(variant_name),
        Vec::new(),
        args,
        types::Syntactic::_Self,
    );

    let ctor_function_path = ctx
        .path()
        .clone()
        .with_child_value(String::from(variant_name));
    ctx.declare_value(String::from(variant_name))
        .expect("Duplicate enum variant constructor");

    let (mut block_mgr, current_block) = ir::BlockManager::new(
        ctx.semantic_type_for_syntactic(&types::Syntactic::Unit)
            .unwrap(),
        &ctor_function_path,
    );

    // define a variable for the object we are building
    let constructed_object =
        symtab::values::Variable::new("constructed".into(), Some(types::Syntactic::_Self));
    let constructed_object_path = ctx
        .define_value(symtab::Value::LocalBinding(
            symtab::values::LocalBinding::Let(constructed_object),
        ))
        .unwrap();

    let constructed_object_value = block_mgr.values_mut().id_for_path(&constructed_object_path);

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
        let arg_value = block_mgr.values_mut().id_for_path(&arg_def_path);
        let field_temp = block_mgr.values_mut().next_temp();
        let field_get_line = ir::IrLine::new_get_field_pointer(
            loc.clone(),
            constructed_object_value,
            arg.name.clone(),
            field_temp,
        );

        let field_store_line = ir::IrLine::new_store(loc.clone(), arg_value, field_temp);

        block_mgr
            .get_mut(current_block)
            .unwrap()
            .append(&mut vec![field_get_line, field_store_line]);
    }

    let ctor_function =
        symtab::values::Function::new(prototype, Some(block_mgr.take(current_block)));

    ctx.define_value(symtab::Value::Function(Box::new(ctor_function)))
        .unwrap();
}

impl Collect<symtab::TypePath> for ast::items::enum_definition::EnumVariantTree {
    fn collect_inner(&self, mut ctx: TypeCollectCtx) -> CollectResult {
        ctx.declare_value(self.name.value.clone())?;
        ctx.into_result()
    }
}

impl<C> Linearize<UnpathedLinearizeCtx, symtab::TypePath, C>
    for ast::items::enum_definition::EnumVariantTree
where
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedLinearizeCtx, Path = symtab::TypePath>,
    ast::types::TypeNoBoundsTree:
        Linearize<UnpathedLinearizeCtx, symtab::TypePath, C, Data = types::Syntactic>,
{
    type Data = (String, symtab::types::EnumVariantRepr);
    fn linearize_inner(self, mut ctx: C) -> LinearizeResult<Self::Data, UnpathedLinearizeCtx> {
        let variant_data_type;
        (variant_data_type, ctx) = match self.data {
            Some(variant_item) => variant_item.linearize(ctx)?,
            None => (symtab::types::EnumVariantRepr::Unit, ctx),
        };

        let (variant_name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result((variant_name, variant_data_type))
    }
}

impl<C> Linearize<UnpathedFunctionLinearizeCtx, symtab::TypePath, C>
    for ast::items::enum_definition::EnumVariantTree
where
    C: PathedLinearizeCtxTrait<Unpathed = UnpathedFunctionLinearizeCtx, Path = symtab::TypePath>,
    ast::types::TypeNoBoundsTree: Linearize<
            UnpathedFunctionLinearizeCtx,
            symtab::TypePath,
            C,
            Data = Option<types::Syntactic>,
        >,
{
    type Data = (String, symtab::types::EnumVariantRepr);
    fn linearize_inner(
        self,
        mut ctx: C,
    ) -> LinearizeResult<Self::Data, UnpathedFunctionLinearizeCtx> {
        let variant_data_type;
        (variant_data_type, ctx) = match self.data {
            Some(variant_item) => variant_item.linearize(ctx)?,
            None => (symtab::types::EnumVariantRepr::Unit, ctx),
        };

        let (variant_name, ctx) = self.name.linearize(ctx)?;

        ctx.into_result((variant_name, variant_data_type))
    }
}

impl Collect<symtab::TypePath> for ast::items::EnumDefinitionTree {
    fn collect_inner(&self, mut ctx: TypeCollectCtx) -> CollectResult {
        let _enum_path = ctx.declare_type(self.name.value.clone())?;
        ctx = ctx.with_child_type(self.name.value.clone());

        ctx = self.generic_params.collect_symbols(ctx)?;

        for variant in &self.variants {
            ctx = variant.collect_symbols(ctx)?;
        }
        ctx.into_result()
    }
}

impl<U, P, C> Linearize<U, P, C> for ast::items::EnumDefinitionTree
where
    U: UnpathedLinearizeCtxTrait,
    P: symtab::Path + symtab::TypeOwner,
    C: PathedLinearizeCtxTrait<Unpathed = U, Path = P>,
    ast::generics::OptionalGenericParamsListTree: Linearize<U, P, C>,
    ast::types::TypeNoBoundsTree: Linearize<U, P, C, Data = types::Syntactic>,
    ast::items::enum_definition::EnumVariantTree: Linearize<
            U,
            symtab::TypePath,
            PathedCtx<U, symtab::TypePath>,
            Data = (String, symtab::types::EnumVariantRepr),
        >,
{
    type Data = (
        symtab::types::EnumRepr,
        <ast::generics::OptionalGenericParamsListTree as Linearize<U, P, C>>::Data,
    );
    fn linearize_inner(self, ctx: C) -> LinearizeResult<Self::Data, U> {
        let (enum_name, ctx) = self.name.linearize(ctx)?;

        let (generic_params, ctx) = self.generic_params.linearize(ctx)?;
        let mut ctx = ctx.with_child_type(enum_name.clone());
        let _enum_path = ctx.path().clone();

        let mut variants: Vec<(String, symtab::types::EnumVariantRepr)> = Vec::new();
        let _constructor_impl_path = ctx.create_impl(types::Syntactic::Named(enum_name.clone()))?;

        // unimplemented!("enum variant constructor");
        for variant in self.variants {
            let _variant_loc = variant.loc();
            let (variant_name, variant_repr): (String, symtab::types::EnumVariantRepr);
            ((variant_name, variant_repr), ctx) = variant.linearize(ctx)?;
            let _arg_types = match &variant_repr {
                symtab::types::EnumVariantRepr::Tuple(types) => types.clone(),
                symtab::types::EnumVariantRepr::Unit => Vec::new(),
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

        let enum_repr =
            symtab::types::EnumRepr::new(enum_name, variants).expect("duplicate enum variant:");
        ctx.into_result((enum_repr, generic_params))
    }
}

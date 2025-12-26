use crate::frontend::ast::*;

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

impl midend::treewalk::Linearize<midend::symtab::EnumVariantRepr> for TupleDataTree {
    fn linearize(
        self,
        ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> midend::symtab::EnumVariantRepr {
        midend::symtab::EnumVariantRepr::Tuple(
            self.element_types
                .into_iter()
                .map(|type_tree| {
                    type_tree
                        .linearize(ctx)
                        .expect("tuple members must have types")
                })
                .collect(),
        )
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

impl midend::treewalk::Linearize<midend::symtab::EnumVariantRepr> for EnumVariantDataTree {
    fn linearize(
        self,
        ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> midend::symtab::EnumVariantRepr {
        match self {
            EnumVariantDataTree::TupleData(elements) => elements.linearize(ctx),
        }
    }
}

fn create_enum_variant_constructor(
    symtab: &mut midend::symtab::SymbolTable,
    enum_path: midend::symtab::DefPath,
    enum_name: &String,
    variant_name: &String,
    arg_types: Vec<midend::types::Syntactic>,
    loc: sourceloc::SourceLoc,
) {
    unimplemented!();
    // create variables for each argument, named by index
    /*let args: Vec<midend::symtab::Variable> = arg_types
        .into_iter()
        .enumerate()
        .map(|(arg_idx, arg_type)| {
            midend::symtab::Variable::new(format!("{}", arg_idx), Some(arg_type))
        })
        .collect();

    // construct a defpath for the function (under an impl block at the same depth as the type decl
    // itself)
    let mut function_path = enum_path.clone();
    function_path.pop().unwrap();
    function_path
        .push(midend::symtab::DefPathComponent::Implementation(
            ImplementationName::new(
                Vec::new(),
                midend::types::Syntactic::Named(enum_name.clone()),
                Vec::new(),
            ),
        ))
        .unwrap();

    function_path
        .push(midend::symtab::DefPathComponent::Value(
            variant_name.clone(),
        ))
        .unwrap();

    // create the function prototype, declare the function, and set up to create IR
    let prototype = midend::symtab::values::function::FunctionPrototype::new(
        variant_name.clone(),
        Vec::new(),
        args,
        midend::types::Syntactic::_Self,
    );

    symtab.declare(function_path.clone()).unwrap();

    let (mut block_mgr, current_block) = midend::ir::BlockManager::new(
        symtab
            .semantic_type_for_syntactic(
                &enum_path,
                midend::types::ParamSubstMap::empty(),
                &midend::types::Syntactic::Unit,
            )
            .unwrap(),
        function_path.clone(),
    );

    // define a variable for the object we are building
    let constructed_object =
        midend::symtab::Variable::new("constructed".into(), Some(midend::types::Syntactic::_Self));
    let constructed_object_path = symtab
        .define(function_path.clone(), constructed_object)
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
        let arg_def_path = symtab.define(function_path.clone(), arg.clone()).unwrap();
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
    symtab
        .define(function_path.parent().unwrap(), ctor_function)
        .unwrap();
    */
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

impl midend::treewalk::Collect for EnumVariantTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.declare_value(self.name.value.clone())?;
        Ok(ctx.take())
    }
}

impl midend::treewalk::Linearize<(String, midend::symtab::EnumVariantRepr)> for EnumVariantTree {
    fn linearize(
        self,
        ctx: &mut midend::treewalk::LinearizeCtx,
    ) -> (String, midend::symtab::EnumVariantRepr) {
        let variant_data_type = match self.data {
            Some(variant_item) => variant_item.linearize(ctx),
            None => midend::symtab::EnumVariantRepr::Unit,
        };

        let variant_name = self.name.linearize(ctx);

        (variant_name, variant_data_type)
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
    pub generic_params: Option<generics::GenericParamsListTree>,
    pub variants: Vec<EnumVariantTree>,
}

impl Ast for EnumDefinitionTree {
    fn loc(&self) -> sourceloc::SourceSpan {
        let mut loc = self
            .enum_keyword_loc
            .clone()
            .merge(&self.name.loc())
            .unwrap();

        if let Some(params) = &self.generic_params {
            loc = loc.merge(&params.loc()).unwrap();
        }

        for variant in &self.variants {
            loc = loc.merge(&variant.loc()).unwrap();
        }

        loc
    }
}

impl midend::treewalk::Collect for EnumDefinitionTree {
    fn collect_symbols(
        &self,
        mut ctx: midend::treewalk::CollectCtx,
    ) -> midend::treewalk::CollectResult {
        ctx.declare_type(self.name.value.clone())?;

        for variant in &self.variants {
            ctx = variant.collect_to_ctx(ctx)?;
        }
        Ok(ctx.take())
    }
}

impl midend::treewalk::Linearize<midend::symtab::EnumRepr> for EnumDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut midend::treewalk::LinearizeCtx) -> midend::symtab::EnumRepr {
        unimplemented!();
        /*let name = self.name.linearize(ctx);
        let type_def_path_component =
            midend::symtab::DefPathComponent::Type(midend::types::Syntactic::Named(name.clone()));

        let generic_params = match self.generic_params {
            Some(params) => params.linearize(ctx),
            None => Vec::new(),
        };

        ctx.push_def_path(type_def_path_component.clone(), &generic_params);

        let variants: Vec<(String, midend::symtab::EnumVariantRepr)> = self
            .variants
            .into_iter()
            .map(|variant| {
                let variant_loc = variant.loc();
                let (variant_name, variant_repr) = variant.linearize(ctx);
                let arg_types = match &variant_repr {
                    midend::symtab::EnumVariantRepr::Tuple(types) => types.clone(),
                    midend::symtab::EnumVariantRepr::Unit => Vec::new(),
                };

                let enum_def_path = ctx.def_path().clone();
                create_enum_variant_constructor(
                    ctx.symtab_mut(),
                    enum_def_path,
                    &name,
                    &variant_name,
                    arg_types,
                    variant_loc.start(),
                );

                (variant_name, variant_repr)
            })
            .collect::<Vec<_>>();

        ctx.pop_def_path(type_def_path_component).unwrap();
        midend::symtab::EnumRepr::new(name, generic_params, variants).unwrap()
        */
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

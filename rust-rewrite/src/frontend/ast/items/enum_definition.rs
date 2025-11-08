use crate::{frontend::ast::*, midend::symtab::ImplementationName};

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum EnumVariantData {
    TupleData(Vec<TypeTree>),
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct EnumVariantDataTree {
    pub loc: SourceLoc,
    pub data: EnumVariantData,
}

fn create_enum_variant_constructor(
    symtab: &mut midend::symtab::SymbolTable,
    enum_path: midend::symtab::DefPath,
    enum_name: &String,
    variant_name: &String,
    arg_types: Vec<midend::types::Syntactic>,
) {
    // create variables for each argument, named by index
    let args: Vec<midend::symtab::Variable> = arg_types
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
        .push(midend::symtab::DefPathComponent::Function(
            midend::symtab::FunctionName::new(variant_name.clone()),
        ))
        .unwrap();

    // create the function prototype, declare the function, and set up to create IR
    let prototype = midend::symtab::FunctionPrototype::new(
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

    let constructed_object_value = block_mgr
        .values_mut()
        .id_for_variable(constructed_object_path);

    /*
     * for each argument:
     * define in the symtab
     * get its value
     * get a temp to hold the address of the corresponding field
     * store the argument into the field
     */
    for arg in &prototype.arguments {
        let arg_def_path = symtab.define(function_path.clone(), arg.clone()).unwrap();
        let arg_value = block_mgr.values_mut().id_for_variable(arg_def_path);
        let field_temp = block_mgr.values_mut().next_temp();
        let field_get_line = midend::ir::IrLine::new_get_field_pointer(
            SourceLoc::none(),
            constructed_object_value,
            arg.name.clone(),
            field_temp,
        );

        let field_store_line =
            midend::ir::IrLine::new_store(SourceLoc::none(), arg_value, field_temp);

        block_mgr
            .get_mut(&current_block)
            .unwrap()
            .append(&mut vec![field_get_line, field_store_line]);
    }

    block_mgr.resolve_final_convergence(current_block).unwrap();

    let ctor_function =
        midend::symtab::Function::new(prototype, Some(midend::ir::ControlFlow::from(block_mgr)));
    symtab.define(function_path.parent().unwrap(), ctor_function);
}

impl treewalk::Linearize<midend::symtab::enum_definition::EnumVariantRepr> for EnumVariantDataTree {
    fn linearize(
        self,
        context: &mut treewalk::LinearizeCtx,
    ) -> midend::symtab::enum_definition::EnumVariantRepr {
        match self.data {
            EnumVariantData::TupleData(elements) => {
                midend::symtab::enum_definition::EnumVariantRepr::Tuple(
                    elements
                        .into_iter()
                        .map(|type_tree| type_tree.linearize(context))
                        .collect(),
                )
            }
        }
    }
}

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct EnumVariantTree {
    pub loc: SourceLoc,
    pub name: String,
    pub data: Option<EnumVariantDataTree>,
}
impl EnumVariantTree {
    pub fn new(loc: SourceLoc, name: String, data: Option<EnumVariantDataTree>) -> Self {
        Self { loc, name, data }
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
    pub loc: SourceLoc,
    pub name: String,
    pub generic_params: Option<generics::GenericParamsListTree>,
    pub variants: Vec<EnumVariantTree>,
}

impl EnumDefinitionTree {
    pub fn new(
        loc: SourceLoc,
        name: String,
        generic_params: Option<generics::GenericParamsListTree>,
        variants: Vec<EnumVariantTree>,
    ) -> Self {
        Self {
            loc,
            name,
            generic_params,
            variants,
        }
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

impl treewalk::CollectSymbols for EnumDefinitionTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        ctx.declare(midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(self.name.clone()),
        ))
        .unwrap();
    }
}

impl treewalk::Linearize<midend::symtab::EnumRepr> for EnumDefinitionTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, ctx: &mut treewalk::LinearizeCtx) -> midend::symtab::EnumRepr {
        let type_def_path_component = midend::symtab::DefPathComponent::Type(
            midend::types::Syntactic::Named(self.name.clone()),
        );

        let generic_params = match self.generic_params {
            Some(params) => params.linearize(ctx),
            None => Vec::new(),
        };

        ctx.push_def_path(type_def_path_component.clone(), &generic_params);

        let variants: Vec<(String, midend::symtab::EnumVariantRepr)> = self
            .variants
            .into_iter()
            .map(|variant| {
                let variant_data_type = match variant.data {
                    Some(variant_item) => variant_item.linearize(ctx),
                    None => midend::symtab::EnumVariantRepr::Unit,
                };

                let arg_types = match &variant_data_type {
                    midend::symtab::EnumVariantRepr::Tuple(types) => types.clone(),
                    midend::symtab::EnumVariantRepr::Unit => Vec::new(),
                };

                let enum_def_path = ctx.def_path().clone();
                create_enum_variant_constructor(
                    ctx.symtab_mut(),
                    enum_def_path,
                    &self.name,
                    &variant.name,
                    arg_types,
                );
                (variant.name, variant_data_type)
            })
            .collect::<Vec<_>>();

        ctx.pop_def_path(type_def_path_component).unwrap();
        midend::symtab::EnumRepr::new(self.name, generic_params, variants).unwrap()
    }
}

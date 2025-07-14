use crate::midend::{symtab::*, types};

fn create_core_types(symtab: &mut SymbolTable) {
    let core_def_path = DefPath::empty()
        .with_component(DefPathComponent::Module(ModuleName {
            name: "core".into(),
        }))
        .unwrap();

    {
        let unit_definition = TypeDefinition::new(types::Type::Unit, TypeRepr::Unit);
        symtab
            .insert(core_def_path.clone(), unit_definition)
            .unwrap();
    }

    for (type_, size) in [
        (types::Type::U8, 1),
        (types::Type::U16, 2),
        (types::Type::U32, 4),
        (types::Type::U64, 8),
    ] {
        let unsigned_definition = TypeDefinition::new(
            type_.clone(),
            TypeRepr::UnsignedInteger(PrimitiveIntegerRepr::new(size)),
        );

        symtab
            .insert(core_def_path.clone(), unsigned_definition)
            .unwrap();
    }

    for (type_, size) in [
        (types::Type::I8, 1),
        (types::Type::I16, 2),
        (types::Type::I32, 4),
        (types::Type::I64, 8),
    ] {
        let signed_definition = TypeDefinition::new(
            type_.clone(),
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(size)),
        );

        symtab
            .insert(core_def_path.clone(), signed_definition)
            .unwrap();
    }
}

pub fn create_core(symtab: &mut SymbolTable) {
    let core_module_path = symtab
        .insert(DefPath::empty(), Module::new("core".into()))
        .unwrap();
    symtab
        .insert(
            core_module_path,
            Import::new(
                "core".into(),
                DefPath::empty()
                    .with_component(
                        ModuleName {
                            name: "core".into(),
                        }
                        .into(),
                    )
                    .unwrap(),
            ),
        )
        .unwrap();

    create_core_types(symtab);
}

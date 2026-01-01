use crate::midend::symtab::*;

fn create_core_types(symtab: &mut SymbolTable) {
    let core_def_path = DefPath::new_type(Vec::new(), "core".into());

    for type_ in [
        BuiltinType::Unit,
        BuiltinType::U8,
        BuiltinType::U16,
        BuiltinType::U32,
        BuiltinType::U64,
        BuiltinType::I8,
        BuiltinType::I16,
        BuiltinType::I32,
        BuiltinType::I64,
    ] {
        symtab
            .define_type(
                core_def_path
                    .clone()
                    .with_segment(type_.path_segment())
                    .unwrap(),
                type_,
            )
            .unwrap();
    }
}

pub fn create_core(symtab: &mut SymbolTable) {
    /*
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
    */

    create_core_types(symtab);
}

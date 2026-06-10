use crate::midend::symtab::{types, DefPath, Symbol, SymbolTable};

fn create_core_types(symtab: &mut SymbolTable) {
    let core_def_path = DefPath::new_type(Vec::new(), "core".into());

    for type_ in [
        types::BuiltinType::Unit,
        types::BuiltinType::U8,
        types::BuiltinType::U16,
        types::BuiltinType::U32,
        types::BuiltinType::U64,
        types::BuiltinType::I8,
        types::BuiltinType::I16,
        types::BuiltinType::I32,
        types::BuiltinType::I64,
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

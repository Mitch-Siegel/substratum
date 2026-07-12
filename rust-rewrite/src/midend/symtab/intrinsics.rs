use crate::midend::symtab::{self, types, Symbol, SymbolTable, Symtab, TypeOwner, TypePath};

fn create_core_types(symtab: &mut SymbolTable) {
    let core_def_path = TypePath::new(None::<TypePath>, "core".into());

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
                    .with_child_type(String::from(type_.name())),
                symtab::Type::Builtin(type_),
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

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

pub(crate) fn create_core(symtab: &mut SymbolTable) {
    create_core_types(symtab);
}

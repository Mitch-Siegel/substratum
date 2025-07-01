use crate::midend::{symtab::*, types};

pub fn create_module() -> Module {
    let mut intrinsics = Module::new("_intrinsics_".into());

    // unsigned integers
    intrinsics
        .insert_type(TypeDefinition::new(
            types::Type::U8,
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(1)),
        ))
        .unwrap();
    intrinsics
        .insert_type(TypeDefinition::new(
            types::Type::U16,
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(2)),
        ))
        .unwrap();
    intrinsics
        .insert_type(TypeDefinition::new(
            types::Type::U32,
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(4)),
        ))
        .unwrap();
    intrinsics
        .insert_type(TypeDefinition::new(
            types::Type::U64,
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(8)),
        ))
        .unwrap();

    // signed integers
    intrinsics
        .insert_type(TypeDefinition::new(
            types::Type::I8,
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(1)),
        ))
        .unwrap();
    intrinsics
        .insert_type(TypeDefinition::new(
            types::Type::I16,
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(2)),
        ))
        .unwrap();
    intrinsics
        .insert_type(TypeDefinition::new(
            types::Type::I32,
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(4)),
        ))
        .unwrap();
    intrinsics
        .insert_type(TypeDefinition::new(
            types::Type::I64,
            TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(8)),
        ))
        .unwrap();

    intrinsics
}

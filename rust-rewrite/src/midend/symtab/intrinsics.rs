use crate::midend::{symtab::*, types};

pub fn create_module() -> Module {
    let mut intrinsics = Module::new("_intrinsics_".into());

    // unsigned integers
    intrinsics
        .insert_type(ResolvedTypeDefinition::new(
            types::ResolvedType::U8,
            UnresolvedTypeRepr::SignedInteger(PrimitiveIntegerRepr::new(1)),
        ))
        .unwrap();
    intrinsics
        .insert_type(ResolvedTypeDefinition::new(
            types::ResolvedType::U16,
            UnresolvedTypeRepr::SignedInteger(PrimitiveIntegerRepr::new(2)),
        ))
        .unwrap();
    intrinsics
        .insert_type(ResolvedTypeDefinition::new(
            types::ResolvedType::U32,
            UnresolvedTypeRepr::SignedInteger(PrimitiveIntegerRepr::new(4)),
        ))
        .unwrap();
    intrinsics
        .insert_type(ResolvedTypeDefinition::new(
            types::ResolvedType::U64,
            UnresolvedTypeRepr::SignedInteger(PrimitiveIntegerRepr::new(8)),
        ))
        .unwrap();

    // signed integers
    intrinsics
        .insert_type(ResolvedTypeDefinition::new(
            types::ResolvedType::I8,
            UnresolvedTypeRepr::SignedInteger(PrimitiveIntegerRepr::new(1)),
        ))
        .unwrap();
    intrinsics
        .insert_type(ResolvedTypeDefinition::new(
            types::ResolvedType::I16,
            UnresolvedTypeRepr::SignedInteger(PrimitiveIntegerRepr::new(2)),
        ))
        .unwrap();
    intrinsics
        .insert_type(ResolvedTypeDefinition::new(
            types::ResolvedType::I32,
            UnresolvedTypeRepr::SignedInteger(PrimitiveIntegerRepr::new(4)),
        ))
        .unwrap();
    intrinsics
        .insert_type(ResolvedTypeDefinition::new(
            types::ResolvedType::I64,
            UnresolvedTypeRepr::SignedInteger(PrimitiveIntegerRepr::new(8)),
        ))
        .unwrap();

    intrinsics
}

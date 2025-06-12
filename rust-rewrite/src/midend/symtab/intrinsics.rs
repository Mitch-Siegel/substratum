use crate::midend::{symtab::*, types};

pub fn create_module() -> Module {
    let mut intrinsics = Module::new("_intrinsics_".into());

    intrinsics.insert_type(TypeDefinition::new(
        types::Type::U8,
        TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(1)),
    ));
    intrinsics.insert_type(TypeDefinition::new(
        types::Type::U16,
        TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(2)),
    ));
    intrinsics.insert_type(TypeDefinition::new(
        types::Type::U32,
        TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(4)),
    ));
    intrinsics.insert_type(TypeDefinition::new(
        types::Type::U64,
        TypeRepr::SignedInteger(PrimitiveIntegerRepr::new(8)),
    ));

    intrinsics
}

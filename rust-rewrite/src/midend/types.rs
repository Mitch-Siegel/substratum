use serde::Serialize;

#[derive(Debug, Serialize)]
pub struct BasicTypeInfo {
    pointer_level: usize,
}

#[derive(Debug, Serialize)]
pub enum Type {
    U8(BasicTypeInfo),
    U16(BasicTypeInfo),
    U32(BasicTypeInfo),
    U64(BasicTypeInfo),
}

impl Type {
    pub fn new_u8(pointer_level: usize) -> Self {
        Type::U8(BasicTypeInfo { pointer_level })
    }

    pub fn new_u16(pointer_level: usize) -> Self {
        Type::U16(BasicTypeInfo { pointer_level })
    }

    pub fn new_u32(pointer_level: usize) -> Self {
        Type::U32(BasicTypeInfo { pointer_level })
    }

    pub fn new_u64(pointer_level: usize) -> Self {
        Type::U64(BasicTypeInfo { pointer_level })
    }
}

use serde::Serialize;
use std::fmt::Display;

#[derive(Clone, Copy, Debug, Serialize)]
pub struct BasicTypeInfo {
    pointer_level: usize,
}

impl Display for BasicTypeInfo {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let mut pointer_string = String::new();
        for _ in 0..self.pointer_level {
            pointer_string.push('*');
        }
        write!(f, "{}", pointer_string)
    }
}

#[derive(Clone, Copy, Debug, Serialize)]
pub enum Type {
    U8(BasicTypeInfo),
    U16(BasicTypeInfo),
    U32(BasicTypeInfo),
    U64(BasicTypeInfo),
}

impl Display for Type {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::U8(info) => write!(f, "u8 {}", info),
            Self::U16(info) => write!(f, "u8 {}", info),
            Self::U32(info) => write!(f, "u8 {}", info),
            Self::U64(info) => write!(f, "u8 {}", info),
        }
    }
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

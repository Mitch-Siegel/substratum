use crate::midend::{symtab::*, types::Type};

enum ImplementedFunction {
    Associated(Function),
    Method(Function),
}

pub struct Implementation {
    pub type_: Type,
    pub functions: HashMap<String, ImplementedFunction>,
}

impl Implementation {
    pub fn new(type_: Type) -> Self {
        Self {
            type_: type_,
            functions: HashMap::new(),
        }
    }
}

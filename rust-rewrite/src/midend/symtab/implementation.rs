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

    pub fn add_associated(&mut self, associated: Function) {
        self.functions.insert(
            associated.name().into(),
            ImplementedFunction::Associated(associated),
        );
    }

    pub fn add_method(&mut self, method: Function) {
        self.functions
            .insert(method.name().into(), ImplementedFunction::Method(method));
    }

    pub fn lookup_associated(&mut self, name: String) -> Result<&Function, UndefinedSymbolError> {
        let result_with_name =
            self.functions
                .get(&name)
                .ok_or(UndefinedSymbolError::associated(
                    self.type_.clone(),
                    name.clone(),
                ))?;

        match result_with_name {
            ImplementedFunction::Associated(associated) => Ok(&associated),
            _ => Err(UndefinedSymbolError::associated(
                self.type_.clone(),
                name.clone(),
            )),
        }
    }

    pub fn lookup_method(&mut self, name: String) -> Result<&Function, UndefinedSymbolError> {
        let result_with_name = self
            .functions
            .get(&name)
            .ok_or(UndefinedSymbolError::method(
                self.type_.clone(),
                name.clone(),
            ))?;

        match result_with_name {
            ImplementedFunction::Method(method) => Ok(&method),
            _ => Err(UndefinedSymbolError::method(
                self.type_.clone(),
                name.clone(),
            )),
        }
    }
}

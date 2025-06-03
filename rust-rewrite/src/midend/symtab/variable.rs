use std::fmt::Display;

use serde::Serialize;

use crate::midend::types::Type;

#[derive(Clone, Debug, Serialize)]
pub struct Variable {
    name: String,
    mangled_name: Option<String>,
    type_: Option<Type>,
}

impl Display for Variable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "{}: {}",
            self.name,
            match &self.type_ {
                Some(type_) => format!("{}", type_),
                None => "?Unknown Type?".into(),
            },
        )
    }
}

impl Variable {
    pub fn new(name: String, type_: Option<Type>) -> Self {
        Variable {
            name,
            mangled_name: None,
            type_,
        }
    }

    pub fn add_mangled_name(&mut self, scope_indices: &Vec<usize>) {
        let mut mangled_name = String::new();
        for scope in scope_indices {
            mangled_name.push_str(&(scope.to_string() + &String::from("_")));
        }
        mangled_name.push_str(&self.name.clone());

        match &self.mangled_name {
            Some(current_name) => {
                panic!(
                "Variable {} already has mangled name {}, can't add_mangled_name with new name {}",
                self.name,
                current_name,
                mangled_name)
            }
            None => self.mangled_name.replace(mangled_name),
        };
    }

    pub fn name(&self) -> String {
        match &self.mangled_name {
            Some(mangled_name) => mangled_name.clone(),
            None => self.name.clone(),
        }
    }

    pub fn type_(&self) -> &Type {
        &self.type_.as_ref().unwrap()
    }
}

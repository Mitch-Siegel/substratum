use std::collections::HashMap;

use serde::Serialize;

use crate::{
    backend,
    midend::{symtab::*, types},
};

use super::Function;

#[derive(Debug, Clone, Serialize)]
pub struct TypeDefinition {
    type_: types::Type,
    pub repr: TypeRepr,
    methods: HashMap<String, Function>,
    associated_functions: HashMap<String, Function>,
}

impl PartialEq for TypeDefinition {
    fn eq(&self, other: &Self) -> bool {
        self.type_.eq(&other.type_) && self.repr.eq(&other.repr)
    }
}

impl Eq for TypeDefinition {}

impl types::SizedType for TypeDefinition {
    fn size<C>(&self, context: &C) -> Result<usize, UndefinedSymbol>
    where
        C: types::TypeSizingContext,
    {
        self.repr.size(context)
    }

    fn alignment<C>(&self, context: &C) -> Result<usize, UndefinedSymbol>
    where
        C: types::TypeSizingContext,
    {
        self.repr.alignment(context)
    }
}
impl TypeDefinition {
    pub fn new(type_: types::Type, repr: TypeRepr) -> Self {
        TypeDefinition {
            type_,
            repr,
            methods: HashMap::new(),
            associated_functions: HashMap::new(),
        }
    }

    pub fn type_(&self) -> &types::Type {
        &self.type_
    }
}

impl AssociatedOwner for TypeDefinition {
    fn associated_functions(&self) -> impl Iterator<Item = &Function> {
        self.associated_functions.values()
    }

    fn lookup_associated(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.associated_functions
            .get(name)
            .ok_or(UndefinedSymbol::associated(self.type_.clone(), name.into()))
    }
}
impl MutAssociatedOwner for TypeDefinition {
    fn associated_functions_mut(&mut self) -> impl Iterator<Item = &mut Function> {
        self.associated_functions.values_mut()
    }

    fn insert_associated(&mut self, associated: Function) -> Result<(), DefinedSymbol> {
        match self.methods.get(associated.name()) {
            Some(existing_method) => Err(DefinedSymbol::Method(
                self.type_.clone(),
                existing_method.prototype.clone(),
            )),
            None => {
                match self
                    .associated_functions
                    .insert(associated.name().into(), associated)
                {
                    Some(existing_associated) => Err(DefinedSymbol::associated(
                        self.type_.clone(),
                        existing_associated.prototype,
                    )),
                    None => Ok(()),
                }
            }
        }
    }
}

impl MethodOwner for TypeDefinition {
    fn methods(&self) -> impl Iterator<Item = &Function> {
        self.methods.values()
    }

    fn lookup_method(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.methods
            .get(name)
            .ok_or(UndefinedSymbol::Method(self.type_.clone(), name.into()))
    }
}
impl MutMethodOwner for TypeDefinition {
    fn methods_mut(&mut self) -> impl Iterator<Item = &mut Function> {
        self.methods.values_mut()
    }

    fn insert_method(&mut self, method: Function) -> Result<(), DefinedSymbol> {
        match self.associated_functions.get(method.name()) {
            Some(existing_associated) => Err(DefinedSymbol::Associated(
                self.type_.clone(),
                existing_associated.prototype.clone(),
            )),
            None => match self.methods.insert(method.name().into(), method) {
                Some(existing_method) => Err(DefinedSymbol::method(
                    self.type_.clone(),
                    existing_method.prototype,
                )),
                None => Ok(()),
            },
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub enum TypeRepr {
    UnsignedInteger(PrimitiveIntegerRepr),
    SignedInteger(PrimitiveIntegerRepr),
    Struct(StructRepr),
}

impl TypeRepr {
    pub fn name(&self) -> String {
        match self {
            Self::UnsignedInteger(repr) => format!("u{}", repr.size),
            Self::SignedInteger(repr) => format!("i{}", repr.size),
            Self::Struct(struct_repr) => struct_repr.name.clone(),
        }
    }
}

impl types::SizedType for TypeRepr {
    fn size<C>(&self, context: &C) -> Result<usize, UndefinedSymbol>
    where
        C: types::TypeSizingContext,
    {
        match self {
            Self::UnsignedInteger(repr) => repr.size(context),
            Self::SignedInteger(repr) => repr.size(context),
            Self::Struct(repr) => repr.size(context),
        }
    }

    fn alignment<C>(&self, context: &C) -> Result<usize, UndefinedSymbol>
    where
        C: types::TypeSizingContext,
    {
        match self {
            Self::UnsignedInteger(repr) => repr.alignment(context),
            Self::SignedInteger(repr) => repr.alignment(context),
            Self::Struct(repr) => repr.alignment(context),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct PrimitiveIntegerRepr {
    size: usize,
}

impl PrimitiveIntegerRepr {
    pub fn new(size: usize) -> Self {
        Self { size }
    }
}

impl types::SizedType for PrimitiveIntegerRepr {
    fn size<C>(&self, _context: &C) -> Result<usize, UndefinedSymbol>
    where
        C: TypeOwner,
    {
        Ok(self.size)
    }

    fn alignment<C>(&self, _context: &C) -> Result<usize, UndefinedSymbol>
    where
        C: types::TypeSizingContext,
    {
        Ok(types::Type::align_size_power_of_two(self.size))
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct StructField {
    pub name: String,
    pub type_: types::Type,
    pub offset: usize,
}

impl StructField {
    pub fn new(name: String, type_: types::Type, offset: usize) -> Self {
        Self {
            name,
            type_,
            offset,
        }
    }
}

impl std::fmt::Display for StructField {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {} (@{})", self.name, self.type_, self.offset)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct StructRepr {
    pub name: String,
    fields: HashMap<String, StructField>,
    size: usize,
    alignment: usize,
}

impl StructRepr {
    pub fn new<C>(
        name: String,
        field_definitions: Vec<(String, types::Type)>,
        context: &C,
    ) -> Result<Self, DefinedSymbol>
    where
        C: types::TypeSizingContext,
    {
        let mut last_offset = 0;
        let mut fields = HashMap::<String, StructField>::new();
        let mut max_alignment: usize = 0;
        for (name, type_) in field_definitions {
            let field_size = type_.size::<backend::arch::Target, C>(context).unwrap();
            let field_alignment = type_
                .alignment::<backend::arch::Target, C>(context)
                .unwrap();

            let padding_bytes = field_alignment - (last_offset % field_alignment);
            last_offset += padding_bytes;

            trace::trace!(
                "Insert struct field {} (type: {}, size: {}, alignment: {}, padding before: {})",
                name.clone(),
                type_.clone(),
                field_size,
                field_alignment,
                padding_bytes
            );
            let field = StructField::new(name.clone(), type_, last_offset);
            match fields.insert(name, field) {
                Some(existing_field) => return Err(DefinedSymbol::field(existing_field)),
                None => (),
            }

            last_offset += field_size;
            max_alignment = max_alignment.max(field_alignment);
        }

        Ok(Self {
            name,
            fields,
            size: last_offset,
            alignment: max_alignment,
        })
    }

    pub fn lookup_field(&self, name: &str) -> Result<&StructField, UndefinedSymbol> {
        match self.fields.get(name) {
            Some(field) => Ok(field),
            None => Err(UndefinedSymbol::field(name.into())),
        }
    }
}

impl<'a> IntoIterator for &'a StructRepr {
    type Item = (&'a String, &'a StructField);
    type IntoIter = std::collections::hash_map::Iter<'a, String, StructField>;

    fn into_iter(self) -> Self::IntoIter {
        self.fields.iter()
    }
}

impl types::SizedType for StructRepr {
    fn size<C>(&self, _context: &C) -> Result<usize, UndefinedSymbol>
    where
        C: types::TypeSizingContext,
    {
        Ok(self.size)
    }

    fn alignment<C>(&self, _context: &C) -> Result<usize, UndefinedSymbol>
    where
        C: types::TypeSizingContext,
    {
        Ok(self.alignment)
    }
}

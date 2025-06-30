use std::collections::{BTreeMap, HashMap};

use serde::{Deserialize, Serialize};

use crate::{
    backend,
    midend::{symtab::*, types},
};

use super::Function;

#[derive(Debug, Clone, Serialize)]
pub struct ResolvedTypeDefinition {
    type_: types::ResolvedType,
    pub repr: ResolvedTypeRepr,
    methods: HashMap<String, ResolvedFunction>,
    associated_functions: HashMap<String, ResolvedFunction>,
}

impl PartialEq for ResolvedTypeDefinition {
    fn eq(&self, other: &Self) -> bool {
        self.type_.eq(&other.type_) && self.repr.eq(&other.repr)
    }
}

impl Eq for ResolvedTypeDefinition {}

impl ResolvedTypeDefinition {
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
impl ResolvedTypeDefinition {
    pub fn new(type_: types::ResolvedType, repr: UnresolvedTypeRepr) -> Self {
        ResolvedTypeDefinition {
            type_,
            repr,
            methods: HashMap::new(),
            associated_functions: HashMap::new(),
        }
    }

    pub fn type_(&self) -> &types::ResolvedType {
        &self.type_
    }
}

impl AssociatedOwner for ResolvedTypeDefinition {
    fn associated_functions(&self) -> impl Iterator<Item = &Function> {
        self.associated_functions.values()
    }

    fn lookup_associated(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.associated_functions
            .get(name)
            .ok_or(UndefinedSymbol::associated(self.type_.clone(), name.into()))
    }
}
impl MutAssociatedOwner for ResolvedTypeDefinition {
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

impl MethodOwner for ResolvedTypeDefinition {
    fn methods(&self) -> impl Iterator<Item = &Function> {
        self.methods.values()
    }

    fn lookup_method(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.methods
            .get(name)
            .ok_or(UndefinedSymbol::Method(self.type_.clone(), name.into()))
    }
}
impl MutMethodOwner for ResolvedTypeDefinition {
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

#[derive(Debug, Clone, Serialize)]
pub struct UnresolvedTypeDefinition {
    type_: types::UnresolvedType,
    pub repr: UnresolvedTypeRepr,
    methods: HashMap<String, Function>,
    associated_functions: HashMap<String, Function>,
}

impl PartialEq for UnresolvedTypeDefinition {
    fn eq(&self, other: &Self) -> bool {
        self.type_.eq(&other.type_) && self.repr.eq(&other.repr)
    }
}

impl Eq for UnresolvedTypeDefinition {}

impl ResolvedTypeDefinition {
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
impl UnresolvedTypeDefinition {
    pub fn new(type_: types::ResolvedType, repr: UnresolvedTypeRepr) -> Self {
        Self {
            type_,
            repr,
            methods: HashMap::new(),
            associated_functions: HashMap::new(),
        }
    }

    pub fn type_(&self) -> &types::ResolvedType {
        &self.type_
    }
}

impl AssociatedOwner for ResolvedTypeDefinition {
    fn associated_functions(&self) -> impl Iterator<Item = &Function> {
        self.associated_functions.values()
    }

    fn lookup_associated(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.associated_functions
            .get(name)
            .ok_or(UndefinedSymbol::associated(self.type_.clone(), name.into()))
    }
}
impl MutAssociatedOwner for ResolvedTypeDefinition {
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

impl MethodOwner for ResolvedTypeDefinition {
    fn methods(&self) -> impl Iterator<Item = &Function> {
        self.methods.values()
    }

    fn lookup_method(&self, name: &str) -> Result<&Function, UndefinedSymbol> {
        self.methods
            .get(name)
            .ok_or(UndefinedSymbol::Method(self.type_.clone(), name.into()))
    }
}
impl MutMethodOwner for ResolvedTypeDefinition {
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

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Hash)]
pub enum UnresolvedTypeRepr {
    UnsignedInteger(PrimitiveIntegerRepr),
    SignedInteger(PrimitiveIntegerRepr),
    Struct(UnresolvedStructRepr),
}

impl UnresolvedTypeRepr {
    pub fn name(&self) -> String {
        match self {
            Self::UnsignedInteger(repr) => format!("u{}", repr.size),
            Self::SignedInteger(repr) => format!("i{}", repr.size),
            Self::Struct(struct_repr) => struct_repr.name.clone(),
        }
    }
}

impl UnresolvedTypeRepr {
    pub fn size<C>(&self, context: &C) -> usize
    where
        C: types::TypeSizingContext,
    {
        match self {
            Self::UnsignedInteger(repr) => repr.size(context),
            Self::SignedInteger(repr) => repr.size(context),
            Self::Struct(repr) => repr.size(context),
        }
    }

    pub fn alignment<C>(&self, context: &C) -> Result<usize, UndefinedSymbol>
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

pub enum ResolvedTypeRepr {
    UnsignedInteger(PrimitiveIntegerRepr),
    SignedInteger(PrimitiveIntegerRepr),
    Struct(ResolvedStructRepr),
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Hash)]
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
        Ok(types::ResolvedType::align_size_power_of_two(self.size))
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Hash)]
pub struct UnresolvedStructField {
    pub name: String,
    pub type_: types::ResolvedType,
}

impl UnresolvedStructField {
    pub fn new(name: String, type_: types::ResolvedType) -> Self {
        Self { name, type_ }
    }
}

impl std::fmt::Display for UnresolvedStructField {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {}", self.name, self.type_,)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Hash)]
pub struct ResolvedStructField {
    pub name: String,
    pub type_: types::ResolvedType,
    pub offset: usize,
}

impl ResolvedStructField {
    pub fn new(name: String, type_: types::ResolvedType, offset: usize) -> Self {
        Self {
            name,
            type_,
            offset,
        }
    }
}

impl std::fmt::Display for ResolvedStructField {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}: {} (@{})", self.name, self.type_, self.offset)
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Hash)]
pub struct UnresolvedStructRepr {
    pub name: String,
    fields: BTreeMap<String, UnresolvedStructField>,
}

impl UnresolvedStructRepr {
    pub fn new<C>(
        name: String,
        field_definitions: Vec<(String, types::UnresolvedType)>,
    ) -> Result<Self, DefinedSymbol> {
        Ok(Self {
            name,
            fields: field_definitions.into_iter().collect(),
        })
    }

    pub fn lookup_field(&self, name: &str) -> Result<&UnresolvedStructField, UndefinedSymbol> {
        match self.fields.get(name) {
            Some(field) => Ok(field),
            None => Err(UndefinedSymbol::field(name.into())),
        }
    }
}

impl<'a> IntoIterator for &'a UnresolvedStructRepr {
    type Item = (&'a String, &'a UnresolvedStructField);
    type IntoIter = std::collections::hash_map::Iter<'a, String, UnresolvedStructField>;

    fn into_iter(self) -> Self::IntoIter {
        self.fields.iter()
    }
}

pub struct ResolvedStructRepr {
    pub name: String,
    fields: BTreeMap<String, ResolvedStructField>,
}

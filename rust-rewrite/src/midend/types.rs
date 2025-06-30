use serde::Serialize;
use std::fmt::Display;

use crate::backend;
use crate::midend::symtab;

mod primitive_type;
mod resolved_type;
mod unresolved_type;

pub use primitive_type::Mutability;
pub use primitive_type::PrimitiveType;
pub use resolved_type::ResolvedType;
pub use unresolved_type::UnresolvedType;

pub trait TypeSizingContext: symtab::TypeOwner + symtab::SelfTypeOwner {}

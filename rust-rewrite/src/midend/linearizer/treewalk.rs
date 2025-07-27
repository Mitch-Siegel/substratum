use crate::{
    frontend::{ast::*, sourceloc::SourceLocWithMod},
    midend::{linearizer::*, symtab::DefPathComponent},
};
use std::collections::BTreeSet;

use name_derive::NameReflectable;

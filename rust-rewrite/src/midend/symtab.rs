use std::collections::{BTreeMap, HashSet};

use crate::{
    midend::{self, *},
    trace,
};
pub use errors::*;

mod def_path;

mod errors;
pub mod intrinsics;
pub mod symbols;
pub mod visitor;

pub mod implementation;
pub mod types;
pub mod values;

pub use def_path::{ImplOwner, MacroOwner, Path, TypeOwner, ValueOwner, *};
pub use implementation::Implementation;
pub use symbols::*;
pub use types::Type;
pub use values::Value;
pub use visitor::*;

pub trait SymtabBase {
    fn insert(
        &mut self,
        path: RawPath,
        maybe_symbol: Option<SymbolDef>,
    ) -> Result<RawPath, SymbolError>;

    fn lookup_at(&self, path: &RawPath) -> Result<Option<&SymbolDef>, SymbolError>;

    fn lookup_at_mut(&mut self, path: &RawPath) -> Result<Option<&mut SymbolDef>, SymbolError>;
}

mod private {
    use super::*;

    impl<T: SymtabBase> SymtabBaseInternal for T {}

    pub trait SymtabBaseInternal: SymtabBase {
        /// declare 'path' to exist
        fn declare(&mut self, path: RawPath) -> Result<RawPath, SymbolError> {
            trace::trace!("declare {}", path);
            self.insert(path, None)
        }

        // define 'symbol' as a child of 'path', returning path::symbol or error
        fn define(&mut self, path: RawPath, symbol: SymbolDef) -> Result<RawPath, SymbolError> {
            match symbol {
                SymbolDef::Type(_) => assert!(path.is_type()),
                SymbolDef::Value(_) => assert!(path.is_value()),
                SymbolDef::Impl(_) => assert!(path.is_impl()),
            }
            assert!(*path.last() == symbol.path_segment());

            match path.last() {
                PathSegment::Impl(_) => {
                    // TODO: check that parent_path is a type or module
                    Ok::<(), SymbolError>(())
                }
                _ => Ok::<(), SymbolError>(()),
            }?;

            trace::trace!("define {} at {}", symbol.name(), path);
            self.insert(path, Some(symbol))
        }

        // ===== Generic Lookups =====
        fn lookup_decl_at(&self, path: &RawPath) -> Result<(), SymbolError> {
            match self.lookup_at(path) {
                Ok(_) => Ok(()),
                Err(e) => Err(e),
            }
        }

        fn lookup_def_at(&self, path: &RawPath) -> Result<&SymbolDef, SymbolError> {
            match self.lookup_at(path) {
                Ok(Some(symbol)) => Ok(symbol),
                Ok(None) => Err(SymbolError::Undefined(path.clone())),
                Err(e) => Err(e),
            }
        }

        fn lookup_def_at_mut(&mut self, path: &RawPath) -> Result<&mut SymbolDef, SymbolError> {
            match self.lookup_at_mut(path) {
                Ok(Some(symbol)) => Ok(symbol),
                Ok(None) => Err(SymbolError::Undefined(path.clone())),
                Err(e) => Err(e),
            }
        }

        fn lookup_decl(
            &self,
            search_path: RawPath,
            lookup_path: RawPath,
        ) -> Result<RawPath, SymbolError> {
            let mut search_segments = search_path.clone().into_iter().collect::<Vec<_>>();
            let lookup_segments = lookup_path.clone().into_iter().collect::<Vec<_>>();
            let (symbol_segment, lookup_segments) = lookup_segments.split_last().unwrap();
            while !search_segments.is_empty() {
                let all_prefix_segments = search_path
                    .clone()
                    .into_iter()
                    .chain(lookup_segments.to_owned())
                    .collect::<Vec<_>>();
                let search_path = RawPath::new(all_prefix_segments, symbol_segment.to_owned());
                match self.lookup_at(&search_path) {
                    Ok(Some(_)) | Ok(None) => {
                        return Ok(search_path);
                    }
                    Err(_) => {
                        search_segments.pop().unwrap();
                    }
                }
            }

            Err(SymbolError::Undeclared(lookup_path))
        }

        fn lookup_def(
            &self,
            search_path: RawPath,
            lookup_path: RawPath,
        ) -> Result<(&SymbolDef, RawPath), SymbolError> {
            let found_path = self.lookup_decl(search_path, lookup_path)?;
            Ok((self.lookup_def_at(&found_path).unwrap(), found_path))
        }

        fn lookup_def_mut(
            &mut self,
            search_path: RawPath,
            lookup_path: RawPath,
        ) -> Result<(&mut SymbolDef, RawPath), SymbolError> {
            let found_path = self.lookup_decl(search_path, lookup_path)?;
            Ok((self.lookup_def_at_mut(&found_path).unwrap(), found_path))
        }
    }
}
pub trait Symtab: SymtabBase + private::SymtabBaseInternal {
    // ===== Declaration =====
    fn declare_type(&mut self, path: TypePath) -> Result<TypePath, SymbolError> {
        self.declare(path.0).map(TypePath::from)
    }

    fn declare_value(&mut self, path: ValuePath) -> Result<ValuePath, SymbolError> {
        self.declare(path.0).map(ValuePath::from)
    }

    // ===== Definition =====
    // define 'symbol' at 'path', returning path or error
    fn define_type(&mut self, path: impl TypeOwner, symbol: Type) -> Result<TypePath, SymbolError> {
        self.define(path.into(), SymbolDef::Type(symbol))
            .map(TypePath::from)
    }

    // define 'symbol' at 'path', returning path or error
    fn define_value(&mut self, path: impl ValueOwner, symbol: Value) -> Result<ValuePath, SymbolError> {
        self.define(path.into(), SymbolDef::Value(symbol))
            .map(ValuePath::from)
    }

    // ===== Typed Lookups =====
    /// Perform a full lookup, searching for the type segment ending `path` at any of its parents
    /// returns the path at which the declaration is found
    fn lookup_type_decl(&self, path: &TypePath) -> Result<TypePath, SymbolError> {
        let (parent_path, type_segment) = path.clone().split_last();
        let lookup_path = RawPath::new(Vec::new(), type_segment);
        let raw_path = self.lookup_decl(
            parent_path.expect("lookup_type_decl called on root path"),
            lookup_path,
        )?;

        Ok(raw_path.into())
    }

    /// Perform a full lookup, searching for the type segment ending `path` at any of its parents
    /// returns the type and path at which it was found
    fn lookup_type_def(&self, path: &TypePath) -> Result<(&Type, TypePath), SymbolError> {
        let (parent_path, type_segment) = path.clone().split_last();
        let lookup_path = RawPath::new(Vec::new(), type_segment);

        match self.lookup_def(
            parent_path.expect("lookup_type_def called on root path"),
            lookup_path,
        )? {
            (SymbolDef::Type(t), found_path) => Ok((t, found_path.into())),
            (_, _) => {
                panic!("lookup_type_def found non-type symbol");
            }
        }
    }

    /// Perform a full lookup, searching for the type segment ending `path` at any of its parents
    /// returns the mutable type and path at which it was found
    fn lookup_type_def_mut(
        &mut self,
        path: &TypePath,
    ) -> Result<(&mut Type, TypePath), SymbolError> {
        let (parent_path, type_segment) = path.clone().split_last();
        let lookup_path = RawPath::new(Vec::new(), type_segment);

        match self.lookup_def_mut(
            parent_path.expect("lookup_type_def_mut called on root path"),
            lookup_path,
        )? {
            (SymbolDef::Type(t), found_path) => Ok((t, found_path.into())),
            (_, _) => {
                panic!("lookup_type_def found non-type symbol");
            }
        }
    }

    /// Perform a full lookup, searching for the value segment ending `path` at any of its parents
    /// returns the path at which the declaration is found
    fn lookup_value_decl(&self, path: &ValuePath) -> Result<RawPath, SymbolError> {
        let (parent_path, value_segment) = path.clone().split_last();
        let lookup_path = RawPath::new(Vec::new(), value_segment);
        self.lookup_decl(
            parent_path.expect("lookup_value_decl called on root path"),
            lookup_path,
        )
    }

    /// Perform a full lookup, searching for the value segment ending `path` at any of its parents
    /// returns the value and path at which it was found
    fn lookup_value_def(&self, path: &ValuePath) -> Result<(&Value, ValuePath), SymbolError> {
        let (parent_path, value_segment) = path.clone().split_last();
        let lookup_path = RawPath::new(Vec::new(), value_segment);

        match self.lookup_def(
            parent_path.expect("lookup_value_def called on root path"),
            lookup_path,
        )? {
            (SymbolDef::Value(v), found_path) => Ok((v, found_path.into())),
            (_, _) => {
                panic!("lookup_value_def found non-value symbol");
            }
        }
    }

    fn lookup_value_def_mut(
        &mut self,
        path: &ValuePath,
    ) -> Result<(&mut Value, ValuePath), SymbolError> {
        let (parent_path, value_segment) = path.clone().split_last();
        let lookup_path = RawPath::new(Vec::new(), value_segment);

        match self.lookup_def_mut(
            parent_path.expect("lookup_value_def_mut called on root path"),
            lookup_path,
        )? {
            (SymbolDef::Value(v), found_path) => Ok((v, found_path.into())),
            (_, _) => {
                panic!("lookup_value_def_mut found non-value symbol");
            }
        }
    }

    fn get_impls_for(&self, path: &TypePath) -> Result<&HashSet<ImplPath>, SymbolError>;

    fn create_impl(
        &mut self,
        impl_parent_path: RawPath,
        impl_for_path: TypePath,
    ) -> Result<ImplPath, SymbolError>;

    fn semantic_type_for_syntactic(
        &self,
        search_def_path: &impl Path,
        generic_params: midend::types::ParamSubstMap,
        ty_: &midend::types::Syntactic,
    ) -> Result<midend::types::Semantic, SymbolError>;
}

pub struct SymbolTable {
    pub types: midend::types::Interner,
    // mapping of symbols to declarations (None) or definitions (Some)
    symbols: BTreeMap<RawPath, Option<SymbolDef>>,
    children: BTreeMap<RawPath, HashSet<RawPath>>,
    // mapping from type definitions to implementations that match them
    impls: BTreeMap<TypePath, HashSet<ImplPath>>,
}

impl Default for SymbolTable {
    fn default() -> Self {
        Self {
            types: midend::types::Interner::new(),
            symbols: BTreeMap::new(),
            children: BTreeMap::new(),
            impls: BTreeMap::new(),
        }
    }
}

impl std::fmt::Debug for SymbolTable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        //writeln!(f, "types: {:?}", self.types)?;
        writeln!(f, "definitions:")?;

        for (path, def) in &self.symbols {
            match def {
                Some(def) => writeln!(f, "defpath {} - {:?}", path, def,)?,
                _ => writeln!(f, "defpath {} - {:?}", path, def)?,
            }
        }
        Ok(())
    }
}

impl SymbolTable {
    pub fn new() -> Self {
        let mut symtab = Self::default();
        intrinsics::create_core(&mut symtab);

        symtab
    }

    pub fn children(&self, def_path: &RawPath) -> HashSet<&RawPath> {
        match self.children.get(def_path) {
            Some(paths) => paths.iter().collect(),
            None => HashSet::new(),
        }
    }

    pub fn decls(&self) -> impl Iterator<Item = &RawPath> {
        self.symbols.keys()
    }

    pub fn defs(&self) -> impl Iterator<Item = (&RawPath, &SymbolDef)> {
        self.symbols
            .iter()
            .filter_map(|(path, maybe_def)| maybe_def.as_ref().map(|def| (path, def)))
    }

    pub fn defs_mut(&mut self) -> impl Iterator<Item = (&RawPath, &mut SymbolDef)> {
        self.symbols
            .iter_mut()
            .filter_map(|(path, maybe_def)| maybe_def.as_mut().map(|def| (path, def)))
    }
}

impl SymtabBase for SymbolTable {
    fn insert(
        &mut self,
        path: RawPath,
        maybe_symbol: Option<SymbolDef>,
    ) -> Result<RawPath, SymbolError> {
        let allow_definition = maybe_symbol.is_some();
        if path.len() > 1 {
            let (maybe_parent_path, _) = path.clone().split_last();
            if let Some(parent_path) = maybe_parent_path {
                if !self
                    .children
                    .entry(parent_path)
                    .or_default()
                    .insert(path.clone())
                    && !allow_definition
                {
                    panic!("untracked child path {}", path)
                }
            }
        }

        match self.symbols.insert(path.clone(), maybe_symbol) {
            Some(Some(_)) => Err(SymbolError::AlreadyDefined(path)),
            Some(None) => {
                if allow_definition {
                    Ok(path)
                } else {
                    Err(SymbolError::AlreadyDeclared(path))
                }
            }
            None => Ok(path),
        }
    }

    fn lookup_at(&self, path: &RawPath) -> Result<Option<&SymbolDef>, SymbolError> {
        match self.symbols.get(path) {
            Some(maybe_symbol) => Ok(maybe_symbol.as_ref()),
            None => Err(SymbolError::Undeclared(path.clone())),
        }
    }

    fn lookup_at_mut(&mut self, path: &RawPath) -> Result<Option<&mut SymbolDef>, SymbolError> {
        match self.symbols.get_mut(path) {
            Some(maybe_symbol) => Ok(maybe_symbol.as_mut()),
            None => Err(SymbolError::Undeclared(path.clone())),
        }
    }
}

impl Symtab for SymbolTable {
    fn get_impls_for(&self, path: &TypePath) -> Result<&HashSet<ImplPath>, SymbolError> {
        self.impls
            .get(path)
            .ok_or(SymbolError::Undeclared(path.clone().into()))
    }

    fn create_impl(
        &mut self,
        impl_parent_path: RawPath,
        impl_for_path: TypePath,
    ) -> Result<ImplPath, SymbolError> {
        let id = ImplId(self.impls.entry(impl_for_path).or_default().len());

        unimplemented!();
    }

    fn semantic_type_for_syntactic(
        &self,
        search_def_path: &impl Path,
        generic_params: midend::types::ParamSubstMap,
        ty_: &midend::types::Syntactic,
    ) -> Result<midend::types::Semantic, SymbolError> {
        unimplemented!();
        /*
        let (_, path) = self.lookup_type(search_def_path, ty_)?;
        trace::trace!(
            "found definition of syntactic type {} at defpath {}",
            ty_,
            path
        );
        Ok(self
            .types
            .semantic_for_defpath(path, generic_params)
            .unwrap())
        */
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn insert_and_lookup() {
        let mut symtab = SymbolTable::new();

        assert_eq!(
            symtab.define(RawPath::empty(), Module::new("test_mod".into())),
            Ok(RawPath::empty()
                .with_component(PathSegment::Module(ModuleName {
                    name: "test_mod".into()
                }))
                .unwrap())
        );

        /*
        assert_eq!(
            symtab.insert(DefPath::new(), Module::new("test_mod".into())),
            Err(SymbolError::Defined(DefPath::new().with_component(
                DefPathComponent::Module(ModuleName("test_mod".into()))
            )))
        );
        */
    }
}

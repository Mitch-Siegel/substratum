use std::collections::{BTreeMap, HashSet};

use crate::{
    midend::{self, symtab, types as midend_types, BTreeSet},
    trace,
};
pub(crate) use errors::*;

mod def_path;

mod errors;
pub(crate) mod symbols;
pub(crate) mod visitor;

pub(crate) mod implementation;
pub(crate) mod types;
pub(crate) mod values;

pub(crate) use def_path::{MaybeEmptyPath, Path, TypeOwner, ValueOwner, *};
pub(crate) use implementation::Implementation;
pub(crate) use symbols::*;
pub(crate) use types::Type;
pub(crate) use values::Value;
pub(crate) use visitor::*;

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
enum UseBinding {
    OriginalName,
    AsName(String),
    Multiple(Vec<Self>),
    Glob,
}

impl std::fmt::Display for UseBinding {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::OriginalName => Ok(()),
            Self::AsName(as_) => write!(f, " as {as_}"),
            Self::Multiple(multiples) => write!(
                f,
                "::{{{}}}",
                multiples
                    .iter()
                    .map(|other| format!("{other}"))
                    .collect::<Vec<String>>()
                    .join(", ")
            ),
            Self::Glob => write!(f, "::*"),
        }
    }
}

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
pub(crate) struct UseDeclaration {
    target_path: RawPath,
    local_binding: UseBinding,
}

impl UseDeclaration {
    pub(crate) fn new_original_name(target_path: impl Path) -> Self {
        Self {
            target_path: target_path.into(),
            local_binding: UseBinding::OriginalName,
        }
    }
}

impl std::fmt::Display for UseDeclaration {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "use {}{}", self.target_path, self.local_binding)
    }
}

pub(crate) trait SymtabBase {
    fn insert(
        &mut self,
        path: RawPath,
        maybe_symbol: Option<SymbolDef>,
    ) -> Result<RawPath, SymbolError>;

    fn lookup_at(&self, path: &RawPath) -> Result<Option<&SymbolDef>, SymbolError>;

    fn lookup_at_mut(&mut self, path: &RawPath) -> Result<Option<&mut SymbolDef>, SymbolError>;

    fn insert_use_declaration(&mut self, path: RawPath, use_declaration: UseDeclaration);

    fn get_use_declarations_at(&self, path: &RawPath) -> Option<&BTreeSet<UseDeclaration>>;

    fn children_of_path(&self, path: &impl Path) -> BTreeSet<RawPath>;
}

mod private {
    use super::{
        trace, Path, PathSegment, RawPath, Symbol, SymbolDef, SymbolError, SymtabBase, UseBinding,
        UseDeclaration,
    };

    impl<T: SymtabBase> SymtabBaseInternal for T {}

    pub(crate) trait SymtabBaseInternal: SymtabBase {
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
            println!("{path:?}");
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

        fn try_resolve_use(&self, use_: &UseDeclaration, lookup_path: &RawPath) -> Option<RawPath> {
            match &use_.local_binding {
                UseBinding::OriginalName => {
                    if use_.target_path.last() == lookup_path.last() {
                        Some(lookup_path.clone())
                    } else {
                        None
                    }
                }
                UseBinding::AsName(use_as_name) => {
                    if lookup_path.len() == 1 && lookup_path.last().to_string() == *use_as_name {
                        Some(use_.target_path.clone())
                    } else {
                        None
                    }
                }
                UseBinding::Multiple(_used) => {
                    unimplemented!();
                }
                UseBinding::Glob => {
                    unimplemented!();
                }
            }
        }

        fn lookup_decl(
            &self,
            search_path: Option<RawPath>,
            lookup_path: RawPath,
        ) -> Result<RawPath, SymbolError> {
            let mut search_segments = match search_path {
                Some(search_path) => search_path.into_iter().collect::<Vec<_>>(),
                None => Vec::new(),
            };

            let lookup_segments = lookup_path.clone().into_iter().collect::<Vec<_>>();
            let (symbol_segment, lookup_segments) = lookup_segments.split_last().unwrap();
            while !search_segments.is_empty() {
                let all_prefix_segments = search_segments
                    .clone()
                    .into_iter()
                    .chain(lookup_segments.to_owned())
                    .collect::<Vec<_>>();
                let full_search_path = RawPath::new(all_prefix_segments, symbol_segment.to_owned());

                // lookup directly at
                if let Ok(Some(_) | None) = self.lookup_at(&full_search_path) {
                    return Ok(full_search_path);
                }

                let search_parent_path = full_search_path.clone().split_last().0.unwrap();
                if let Some(use_directives) = self.get_use_declarations_at(&search_parent_path) {
                    for use_ in use_directives {
                        if let Some(path) = self.try_resolve_use(use_, &lookup_path) {
                            return Ok(path);
                        }
                    }
                }

                search_segments.pop().unwrap();
            }

            Err(SymbolError::Undeclared(lookup_path))
        }

        fn lookup_def(
            &self,
            search_path: Option<RawPath>,
            lookup_path: RawPath,
        ) -> Result<(&SymbolDef, RawPath), SymbolError> {
            let found_path = self.lookup_decl(search_path, lookup_path)?;
            Ok((self.lookup_def_at(&found_path).unwrap(), found_path))
        }

        fn lookup_def_mut(
            &mut self,
            search_path: Option<RawPath>,
            lookup_path: RawPath,
        ) -> Result<(&mut SymbolDef, RawPath), SymbolError> {
            let found_path = self.lookup_decl(search_path, lookup_path)?;
            Ok((self.lookup_def_at_mut(&found_path).unwrap(), found_path))
        }
    }
}

// TODO: pub(in crate::midend)
pub(crate) trait Symtab: SymtabBase + private::SymtabBaseInternal {
    // ===== Declaration =====
    fn declare_type(&mut self, path: TypePath) -> Result<TypePath, SymbolError> {
        self.declare(path.0).map(TypePath::from)
    }

    fn declare_value(&mut self, path: ValuePath) -> Result<ValuePath, SymbolError> {
        self.declare(path.0).map(ValuePath::from)
    }

    // ===== Definition =====
    // define 'symbol' at 'path', returning path or error
    fn define_type(
        &mut self,
        parent_path: impl TypeOwner,
        symbol: Type,
    ) -> Result<TypePath, SymbolError> {
        self.define(parent_path.into(), SymbolDef::Type(symbol))
            .map(TypePath::from)
    }

    // define 'symbol' at 'path', returning path or error
    fn define_value(
        &mut self,
        parent_path: impl ValueOwner,
        symbol: Value,
    ) -> Result<ValuePath, SymbolError> {
        self.define(parent_path.into(), SymbolDef::Value(symbol))
            .map(ValuePath::from)
    }

    fn declare_scope(&mut self, path: ScopePath) -> Result<ScopePath, SymbolError> {
        self.declare(path.into()).map(ScopePath::from)
    }

    // ===== Typed Lookups =====
    /// Perform a full lookup, searching for the type segment ending `path` at any of its parents
    /// returns the path at which the declaration is found
    fn lookup_type_decl(
        &self,
        search_path: &impl Path,
        type_: TypeSegment,
    ) -> Result<TypePath, SymbolError> {
        let raw_path = self.lookup_decl(
            Some(search_path.clone().into()),
            RawPath::new(vec![], type_.into()),
        )?;

        Ok(raw_path.into())
    }

    /// Perform a full lookup, searching for the type segment ending `path` at any of its parents
    /// returns the type and path at which it was found
    fn lookup_type_def(
        &self,
        search_path: &impl Path,
        type_: TypeSegment,
    ) -> Result<(&Type, TypePath), SymbolError> {
        match self.lookup_def(
            Some(search_path.clone().into()),
            RawPath::new(vec![], type_.into()),
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
        search_path: &impl Path,
        type_: TypeSegment,
    ) -> Result<(&mut Type, TypePath), SymbolError> {
        match self.lookup_def_mut(
            Some(search_path.clone().into()),
            RawPath::new(vec![], type_.into()),
        )? {
            (SymbolDef::Type(t), found_path) => Ok((t, found_path.into())),
            (_, _) => {
                panic!("lookup_type_def found non-type symbol");
            }
        }
    }

    /// Perform a full lookup, searching for the value segment ending `path` at any of its parents
    /// returns the path at which the declaration is found
    fn lookup_value_decl(
        &self,
        search_path: &impl Path,
        value: ValueSegment,
    ) -> Result<RawPath, SymbolError> {
        self.lookup_decl(
            Some(search_path.clone().into()),
            RawPath::new(vec![], value.into()),
        )
    }

    /// Perform a full lookup, searching for the value segment ending `path` at any of its parents
    /// returns the value and path at which it was found
    fn lookup_value_def(
        &self,
        search_path: &impl Path,
        value: ValueSegment,
    ) -> Result<(&Value, ValuePath), SymbolError> {
        match self.lookup_def(
            Some(search_path.clone().into()),
            RawPath::new(vec![], value.into()),
        )? {
            (SymbolDef::Value(v), found_path) => Ok((v, found_path.into())),
            (_, _) => {
                panic!("lookup_value_def found non-value symbol");
            }
        }
    }

    fn lookup_value_def_mut(
        &mut self,
        search_path: &impl Path,
        value: ValueSegment,
    ) -> Result<(&mut Value, ValuePath), SymbolError> {
        match self.lookup_def_mut(
            Some(search_path.clone().into()),
            RawPath::new(vec![], value.into()),
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

pub(crate) struct SymbolTable {
    pub(crate) types: midend::types::Interner,
    // mapping of symbols to declarations (None) or definitions (Some)
    symbols: BTreeMap<RawPath, Option<SymbolDef>>,
    children: BTreeMap<RawPath, HashSet<RawPath>>,
    // mapping from type definitions to implementations that match them
    impls: BTreeMap<TypePath, HashSet<ImplPath>>,
    use_declarations: BTreeMap<RawPath, BTreeSet<UseDeclaration>>,
}

impl Default for SymbolTable {
    fn default() -> Self {
        Self {
            types: midend::types::Interner::new(),
            symbols: BTreeMap::new(),
            children: BTreeMap::new(),
            impls: BTreeMap::new(),
            use_declarations: BTreeMap::new(),
        }
    }
}

impl std::fmt::Debug for SymbolTable {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        //writeln!(f, "types: {:?}", self.types)?;
        writeln!(f, "definitions:")?;

        for (path, def) in &self.symbols {
            match def {
                Some(def) => writeln!(f, "defpath {path} - {def:?}",)?,
                _ => writeln!(f, "defpath {path} - {def:?}")?,
            }
        }
        Ok(())
    }
}

impl SymbolTable {
    pub(crate) fn new() -> Self {
        Self::default()
    }

    pub(crate) fn children(&self, def_path: &RawPath) -> HashSet<&RawPath> {
        match self.children.get(def_path) {
            Some(paths) => paths.iter().collect(),
            None => HashSet::new(),
        }
    }

    pub(crate) fn decls(&self) -> impl Iterator<Item = &RawPath> {
        self.symbols.keys()
    }

    pub(crate) fn defs(&self) -> impl Iterator<Item = (&RawPath, &SymbolDef)> {
        self.symbols
            .iter()
            .filter_map(|(path, maybe_def)| maybe_def.as_ref().map(|def| (path, def)))
    }

    pub(crate) fn defs_mut(&mut self) -> impl Iterator<Item = (&RawPath, &mut SymbolDef)> {
        self.symbols
            .iter_mut()
            .filter_map(|(path, maybe_def)| maybe_def.as_mut().map(|def| (path, def)))
    }

    pub(crate) fn uses(&self) -> impl Iterator<Item = (&RawPath, &BTreeSet<UseDeclaration>)> {
        self.use_declarations.iter()
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
                    panic!("untracked child path {path}")
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

    fn insert_use_declaration(&mut self, path: RawPath, use_declaration: UseDeclaration) {
        self.use_declarations
            .entry(path)
            .or_default()
            .insert(use_declaration);
    }

    fn get_use_declarations_at(&self, path: &RawPath) -> Option<&BTreeSet<UseDeclaration>> {
        self.use_declarations.get(path)
    }

    fn children_of_path(&self, path: &impl Path) -> BTreeSet<RawPath> {
        self.children
            .get(&path.clone().into())
            .cloned()
            .unwrap_or_default()
            .into_iter()
            .collect()
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
        _impl_parent_path: RawPath,
        impl_for_path: TypePath,
    ) -> Result<ImplPath, SymbolError> {
        let _id = ImplId(self.impls.entry(impl_for_path).or_default().len());

        unimplemented!();
    }

    fn semantic_type_for_syntactic(
        &self,
        search_path: &impl Path,
        generic_params: midend_types::ParamSubstMap,
        ty_: &midend_types::Syntactic,
    ) -> Result<midend_types::Semantic, SymbolError> {
        match ty_ {
            midend_types::Syntactic::Unit => Ok(midend_types::Semantic::Unit),
            midend_types::Syntactic::U8 => Ok(midend_types::Semantic::U8),
            midend_types::Syntactic::U16 => Ok(midend_types::Semantic::U16),
            midend_types::Syntactic::U32 => Ok(midend_types::Semantic::U32),
            midend_types::Syntactic::U64 => Ok(midend_types::Semantic::U64),
            midend_types::Syntactic::I8 => Ok(midend_types::Semantic::I8),
            midend_types::Syntactic::I16 => Ok(midend_types::Semantic::I16),
            midend_types::Syntactic::I32 => Ok(midend_types::Semantic::I32),
            midend_types::Syntactic::I64 => Ok(midend_types::Semantic::I64),
            midend_types::Syntactic::GenericParam(name) | midend_types::Syntactic::Named(name) => {
                let path = self.lookup_type_decl(search_path, TypeSegment(name.clone()))?;
                trace::trace!(
                    "found definition of syntactic type {} at defpath {}",
                    ty_,
                    path
                );
                Ok(self
                    .types
                    .semantic_for_defpath(path, generic_params)
                    .unwrap())
            }
            midend_types::Syntactic::_Self => unimplemented!("semantic type for Self"),
            midend_types::Syntactic::Reference(_, _) => {
                unimplemented!("semantic type for reference")
            }
            midend_types::Syntactic::Pointer(_, _) => unimplemented!("semantic type for pointer"),
            midend_types::Syntactic::Tuple(_) => unimplemented!("semantic type for tuple"),
            midend_types::Syntactic::Function {
                args: _args,
                ret_ty: _ret_ty,
            } => unimplemented!("semantic type for function"),
        }
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

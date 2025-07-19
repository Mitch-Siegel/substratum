use crate::midend::{symtab::*, types};

use std::collections::{BTreeSet, HashMap};

pub struct GenericParamsContext {
    params_by_path: HashMap<DefPath, BTreeSet<String>>,
    all_params: BTreeSet<String>,
}

impl GenericParamsContext {
    pub fn new() -> Self {
        Self {
            params_by_path: HashMap::new(),
            all_params: BTreeSet::new(),
        }
    }

    pub fn add_params_at_path(
        &mut self,
        def_path: DefPath,
        params: BTreeSet<String>,
    ) -> Result<(), BTreeSet<String>> {
        let duplicated: BTreeSet<String> = self
            .all_params
            .intersection(&params)
            .map(|param| param.clone())
            .collect();
        if duplicated.len() > 0 {
            return Err(duplicated);
        }

        self.all_params.append(&mut params.clone());
        match self.params_by_path.insert(def_path.clone(), params) {
            Some(existing_params) => panic!(
                "existing params at defpath {}: {:?}!",
                def_path, existing_params
            ),
            None => (),
        }

        Ok(())
    }

    fn remove_params_at_path(&mut self, def_path: DefPath) -> Result<BTreeSet<String>, ()> {
        let params_at_path = match self.params_by_path.remove(&def_path) {
            Some(params) => params,
            None => return Err(()),
        };

        for param in &params_at_path {
            if !self.all_params.remove(param) {
                return Err(());
            }
        }
        Ok(params_at_path)
    }

    fn get(&self, def_path: &DefPath) -> Option<&BTreeSet<String>> {
        self.params_by_path.get(def_path)
    }
}

pub struct BasicDefContext {
    symtab: Box<SymbolTable>,
    definition_path: DefPath,
    generics: GenericParamsContext,
}

impl std::fmt::Debug for BasicDefContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "DefContext @ {}", self.definition_path)
    }
}

impl BasicDefContext {
    pub fn new(symtab: Box<SymbolTable>) -> Self {
        Self {
            symtab,
            definition_path: DefPath::empty(),
            generics: GenericParamsContext::new(),
        }
    }

    pub fn with_path(
        symtab: Box<SymbolTable>,
        definition_path: DefPath,
        generics: GenericParamsContext,
    ) -> Self {
        Self {
            symtab,
            definition_path,
            generics: generics,
        }
    }
}

pub trait DefContext: std::fmt::Debug {
    fn symtab(&self) -> &SymbolTable;
    fn symtab_mut(&mut self) -> &mut SymbolTable;
    fn def_path(&self) -> DefPath;
    fn def_path_mut(&mut self) -> &mut DefPath;
    fn generics(&self) -> &GenericParamsContext;
    fn generics_mut(&mut self) -> &mut GenericParamsContext;

    fn push_def_path(&mut self, component: DefPathComponent, generic_params: &Vec<String>) {
        let params_set = generic_params
            .iter()
            .map(|param| param.clone())
            .collect::<BTreeSet<String>>();
        assert_eq!(
            params_set.len(),
            generic_params.len(),
            "Unchecked duplicate generic param"
        );

        self.def_path_mut().push(component).unwrap();
        let new_def_path = self.def_path();
        self.generics_mut()
            .add_params_at_path(new_def_path, params_set)
            .unwrap();
    }

    fn pop_def_path(&mut self, expect: DefPathComponent) -> Result<(), ()> {
        let def_path = self.def_path();
        self.generics_mut().remove_params_at_path(def_path).unwrap();
        let popped = self.def_path_mut().pop().unwrap();

        if popped == expect {
            Ok(())
        } else {
            Err(())
        }
    }

    fn id_for_type(&self, type_: &types::Type) -> Result<TypeId, SymbolError> {
        self.symtab().id_for_type(&self.def_path(), type_)
    }

    fn type_for_id(&self, id: &TypeId) -> Option<&TypeDefinition> {
        self.symtab().type_for_id(id)
    }

    // resolves a string type name to either a defined type or a generic param
    fn resolve_type_name(&self, name: &str) -> Result<types::Type, SymbolError> {
        // first, lookup the type in the symbol table
        let (mut type_, found_def_path) = match self
            .lookup_with_path::<TypeDefinition>(&types::Type::Named(name.into()))
        {
            // if we find it, grab its type and defpath, otherwise create a dummy type and path
            Ok((type_definition, def_path)) => (Some(type_definition.type_().clone()), def_path),
            Err(_) => (None, DefPath::empty()),
        };

        // next, search the generics
        // we may search any generic path *longer than* the def path we found a type definition at
        // this covers cases where more deeply scoped generic parameter names shadow more shallowly
        // scoped named type definitions
        let mut search_def_path = self.def_path();
        while search_def_path.len() > found_def_path.len() {
            match self.generics().get(&search_def_path) {
                Some(params) => match params.get(name) {
                    Some(param) => {
                        type_ = Some(types::Type::GenericParam(param.clone()));
                        break;
                    }
                    None => (),
                },
                None => (),
            }
            search_def_path.pop().unwrap();
        }

        type_.ok_or(SymbolError::Undefined(
            self.def_path(),
            DefPathComponent::Type(types::Type::Named(name.into())),
        ))
    }

    fn lookup<S>(&self, key: &<S as Symbol>::SymbolKey) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup::<S>(&self.def_path(), key)
    }

    fn lookup_with_path<S>(
        &self,
        key: &<S as Symbol>::SymbolKey,
    ) -> Result<(&S, DefPath), SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup_with_path::<S>(&self.def_path(), key)
    }

    fn lookup_mut<S>(&mut self, key: &<S as Symbol>::SymbolKey) -> Result<&mut S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let def_path = self.def_path();
        self.symtab_mut().lookup_mut::<S>(&def_path, key)
    }

    fn lookup_at<S>(&self, def_path: &DefPath) -> Result<&S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab().lookup_at::<S>(def_path)
    }

    fn lookup_at_mut<S>(&mut self, def_path: &DefPath) -> Result<&mut S, SymbolError>
    where
        S: Symbol,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab_mut().lookup_at_mut::<S>(def_path)
    }

    // add a DefPathComponent for 'symbol' at the end of the current def path
    fn insert<S>(&mut self, symbol: S) -> Result<DefPath, SymbolError>
    where
        S: Symbol + std::fmt::Debug,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let def_path = self.def_path();
        let symtab_mut = self.symtab_mut();
        symtab_mut.insert::<S>(def_path, symbol)
    }

    fn insert_at<S>(&mut self, def_path: DefPath, symbol: S) -> Result<DefPath, SymbolError>
    where
        S: Symbol + std::fmt::Debug,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        self.symtab_mut().insert::<S>(def_path, symbol)
    }

    fn lookup_implemented_function(
        &self,
        receiver_type: &types::Type,
        name: &str,
    ) -> Result<&Function, SymbolError> {
        let (_, receiver_type_definition_path) =
            self.lookup_with_path::<TypeDefinition>(receiver_type)?;

        self.lookup_at::<Function>(
            &receiver_type_definition_path
                .clone()
                .with_component(FunctionName { name: name.into() }.into())
                .unwrap(),
        )
    }

    fn take(self) -> Result<(Box<SymbolTable>, DefPath, GenericParamsContext), ()>;
}

impl DefContext for BasicDefContext {
    fn symtab(&self) -> &SymbolTable {
        &self.symtab
    }

    fn symtab_mut(&mut self) -> &mut SymbolTable {
        &mut self.symtab
    }

    fn def_path(&self) -> DefPath {
        self.definition_path.clone()
    }

    fn def_path_mut(&mut self) -> &mut DefPath {
        &mut self.definition_path
    }

    fn generics(&self) -> &GenericParamsContext {
        &self.generics
    }

    fn generics_mut(&mut self) -> &mut GenericParamsContext {
        &mut self.generics
    }

    fn take(self) -> Result<(Box<SymbolTable>, DefPath, GenericParamsContext), ()> {
        Ok((self.symtab, self.definition_path, self.generics))
    }
}

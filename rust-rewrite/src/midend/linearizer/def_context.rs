use crate::{
    midend::{linearizer::*, symtab::*, types, *},
    trace,
};

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

pub struct WalkContext {
    symtab: Box<SymbolTable>,
    definition_path: DefPath,
    functions: HashMap<DefPath, FunctionWalkContext>,
    generics: GenericParamsContext,
}

impl std::fmt::Debug for WalkContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "DefContext @ {}", self.definition_path)
    }
}

impl WalkContext {
    pub fn new(
        symtab: Box<SymbolTable>,
        definition_path: DefPath,
        generics: GenericParamsContext,
    ) -> Self {
        Self {
            symtab,
            definition_path,
            functions: HashMap::new(),
            generics: generics,
        }
    }

    pub fn from_existing(
        symtab: Box<SymbolTable>,
        generics: GenericParamsContext,
        definition_path: DefPath,
        manager: ir::BlockManager,
        block: usize,
    ) -> Self {
        let functions: HashMap<DefPath, FunctionWalkContext> = std::iter::once((
            definition_path.clone(),
            FunctionWalkContext::from_existing(manager, block),
        ))
        .collect();

        Self {
            symtab,
            definition_path,
            functions,
            generics,
        }
    }

    pub fn take(self) -> Result<(Box<SymbolTable>, DefPath, GenericParamsContext), ()> {
        Ok((self.symtab, self.definition_path, self.generics))
    }

    pub fn symtab(&self) -> &SymbolTable {
        &self.symtab
    }

    pub fn symtab_mut(&mut self) -> &mut SymbolTable {
        &mut self.symtab
    }

    pub fn def_path(&self) -> &DefPath {
        &self.definition_path
    }

    //fn def_path_mut(&mut self) -> &mut DefPath
    //
    pub fn generics(&self) -> &GenericParamsContext {
        &self.generics
    }

    fn generics_mut(&mut self) -> &mut GenericParamsContext {
        &mut self.generics
    }

    pub fn create_function(&mut self, prototype: FunctionPrototype) -> Result<(), SymbolError> {
        let (_, unit_type_path) = self
            .lookup_with_path::<symtab::TypeDefinition>(&types::Syntactic::Unit)
            .unwrap();
        let unit_type_id = self
            .symtab_mut()
            .types
            .get_semantic(&unit_type_path)
            .unwrap();

        self.insert_at::<symtab::Function>(
            self.def_path().clone(),
            symtab::Function::new(prototype.clone(), None),
        )?;

        let function_path_component = symtab::DefPathComponent::Function(prototype.name.clone());
        self.push_def_path(function_path_component, &prototype.generic_params);

        for argument in prototype.arguments.clone() {
            self.insert::<symtab::Variable>(argument.clone()).unwrap();
        }

        match self.functions.insert(
            self.def_path().clone(),
            FunctionWalkContext::new(prototype, self.def_path().clone(), unit_type_id),
        ) {
            Some(p) => panic!("Existing prototype!"),
            None => Ok(()),
        }
    }

    pub fn finish_function(&mut self, expected_name: FunctionName) -> Result<(), ()> {
        let def_path = self.def_path().clone();
        let function_context = self.functions.remove(&def_path).unwrap();
        let function = self.lookup_at_mut::<symtab::Function>(&def_path).unwrap();
        match function.control_flow.replace(function_context.take()) {
            Some(existing_cf) => return Err(()),
            None => (),
        }
        self.pop_def_path(DefPathComponent::Function(expected_name));
        Ok(())
    }

    pub fn function(&mut self) -> &mut FunctionWalkContext {
        let mut scan_def_path = self.def_path().clone();
        while scan_def_path.len() > 0 {
            if self.functions.contains_key(&scan_def_path) {
                break;
            } else {
                scan_def_path.pop().unwrap();
            }
        }

        if scan_def_path.len() > 0 {
            self.functions.get_mut(&scan_def_path).unwrap()
        } else {
            panic!("DefContext::function() called with no active function!");
        }
    }

    pub fn push_def_path(&mut self, component: DefPathComponent, generic_params: &Vec<String>) {
        let params_set = generic_params
            .iter()
            .map(|param| param.clone())
            .collect::<BTreeSet<String>>();
        assert_eq!(
            params_set.len(),
            generic_params.len(),
            "Unchecked duplicate generic param"
        );

        trace::warning!("push {:?} to defcontext defpath", component);
        self.definition_path.push(component).unwrap();
        let new_def_path = self.def_path().clone();
        self.generics_mut()
            .add_params_at_path(new_def_path, params_set)
            .unwrap();
    }

    pub fn pop_def_path(&mut self, expect: DefPathComponent) -> Result<(), ()> {
        let def_path = self.def_path().clone();
        self.generics_mut().remove_params_at_path(def_path).unwrap();
        let popped = self.definition_path.pop().unwrap();

        trace::warning!("pop {:?} from defcontext defpath", popped);

        if popped == expect {
            Ok(())
        } else {
            Err(())
        }
    }

    // reserves a subscope, returning its defpath
    pub fn reserve_subscope(&mut self) -> symtab::DefPath {
        let next_subscope_index = self
            .symtab()
            .children(&self.def_path())
            .into_iter()
            .filter(|path| match path.last() {
                DefPathComponent::Scope(_) => true,
                _ => false,
            })
            .count();

        self.symtab
            .insert::<symtab::Scope>(
                self.def_path().clone(),
                symtab::Scope::new(next_subscope_index),
            )
            .unwrap()
    }

    pub fn self_variable(&self) -> Result<DefPath, SymbolError> {
        Ok(self
            .lookup_with_path::<symtab::Variable>(&String::from("self"))?
            .1)
    }

    fn definition_for_semantic_type(&self, type_: &types::Semantic) -> Option<&TypeDefinition> {
        self.symtab().types.get_definition(type_)
    }

    // resolves a string type name to either a defined type or a generic param
    pub fn resolve_type_name(&self, name: &str) -> Result<types::Syntactic, SymbolError> {
        // first, lookup the type in the Symbol table
        let (mut type_, found_def_path) =
            match self.lookup_with_path::<TypeDefinition>(&types::Syntactic::Named(name.into())) {
                // if we find it, grab its type and defpath, otherwise create a dummy type and path
                Ok((type_definition, def_path)) => {
                    (Some(type_definition.syntactic().clone()), def_path)
                }
                Err(_) => (None, DefPath::empty()),
            };

        // next, search the generics
        // we may search any generic path *longer than* the def path we found a type definition at
        // this covers cases where more deeply scoped generic parameter names shadow more shallowly
        // scoped named type definitions
        let mut search_def_path = self.def_path().clone();
        while search_def_path.len() > found_def_path.len() {
            match self.generics().get(&search_def_path) {
                Some(params) => match params.get(name) {
                    Some(param) => {
                        type_ = Some(types::Syntactic::GenericParam(param.clone()));
                        break;
                    }
                    None => (),
                },
                None => (),
            }
            search_def_path.pop().unwrap();
        }

        type_.ok_or(SymbolError::Undefined(
            self.def_path().clone(),
            DefPathComponent::Type(types::Syntactic::Named(name.into())),
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

    pub fn lookup_with_path<S>(
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
        let def_path = self.def_path().clone();
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
    pub fn insert<S>(&mut self, symbol: S) -> Result<DefPath, SymbolError>
    where
        S: Symbol + std::fmt::Debug,
        for<'a> &'a S: From<DefResolver<'a>>,
        for<'a> &'a mut S: From<MutDefResolver<'a>>,
        for<'a> DefGenerator<'a, S>: Into<SymbolDef>,
    {
        let def_path = self.def_path().clone();
        let symtab_mut = self.symtab_mut();
        trace::trace!("insert {:?} at {:?}", symbol, def_path);
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
        receiver_type: &types::Syntactic,
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

    pub fn semantic_type_for_syntactic(&self, ty_: &types::Syntactic) -> Option<types::Semantic> {
        let (_, path) = self.lookup_with_path::<symtab::TypeDefinition>(ty_).ok()?;
        self.symtab().types.get_semantic(&path)
    }

    fn self_type(&self) -> Option<types::Syntactic> {
        let mut search_def_path = self.def_path().clone();
        loop {
            match search_def_path.last() {
                // lookup required
                DefPathComponent::Type(_) => {
                    let definition = self.lookup_at::<TypeDefinition>(&search_def_path).unwrap();
                    return Some(definition.syntactic().clone());
                }
                // trivial case - just grab whatever we're implementing for
                DefPathComponent::Implementation(implementation) => {
                    return Some(implementation.implemented_for.clone())
                }
                DefPathComponent::Empty => break,
                _ => {
                    search_def_path.pop().unwrap();
                }
            }
        }

        None
    }
}

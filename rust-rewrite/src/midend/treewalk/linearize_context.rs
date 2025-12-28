use crate::midend::treewalk::*;

use std::collections::HashMap;

pub struct UnpathedLinearizeCtx {
    symtab: Box<symtab::SymbolTable>,
    _functions: HashMap<symtab::DefPath, FunctionLinearizeCtx>,
}

impl std::fmt::Debug for LinearizeCtx {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "LinearizeCtx")
    }
}

impl PathableContext for UnpathedLinearizeCtx {}

impl UnpathedLinearizeCtx {
    pub fn new(symtab: Box<symtab::SymbolTable>) -> Self {
        Self {
            symtab,
            _functions: HashMap::new(),
        }
    }

    pub fn from_existing(
        symtab: Box<symtab::SymbolTable>,
        definition_path: symtab::DefPath,
        manager: ir::BlockManager,
        block: usize,
    ) -> Self {
        let functions: HashMap<symtab::DefPath, FunctionLinearizeCtx> = std::iter::once((
            definition_path.clone(),
            FunctionLinearizeCtx::from_existing(manager, block),
        ))
        .collect();

        Self {
            symtab,
            _functions: functions,
        }
    }

    pub fn into_result<T>(self, data: T) -> LinearizeResult<T> {
        LinearizeResult::<T>::Ok((data, self))
    }

    pub fn take(self) -> Box<symtab::SymbolTable> {
        self.symtab
    }

    //fn def_path_mut(&mut self) -> &mut DefPath
    //

    pub fn create_function(
        &mut self,
        _prototype: symtab::values::function::FunctionPrototype,
    ) -> Result<(), symtab::SymbolError> {
        unimplemented!();
        /*
        let unit_type_id = self
            .symtab()
            .semantic_type_for_syntactic(
                &self.definition_path,
                types::ParamSubstMap::empty(),
                &types::Syntactic::Unit,
            )
            .unwrap();

        self.define_at::<symtab::Function>(
            self.def_path().clone(),
            symtab::Function::new(prototype.clone(), None),
        )?;

        let function_path_component = symtab::DefPathComponent::Function(prototype.name.clone());
        self.push_def_path(function_path_component, &prototype.generic_params);

        let arg_def_paths = prototype
            .arguments
            .iter()
            .map(|arg| {
                println!("Handle argument {:?}", arg);
                self.define::<symtab::Variable>(arg.clone()).unwrap()
            })
            .collect();

        match self.functions.insert(
            self.def_path().clone(),
            FunctionLinearizeCtx::new(
                prototype,
                self.def_path().clone(),
                unit_type_id,
                arg_def_paths,
            ),
        ) {
            Some(_) => panic!(
                "Function {} has already been inserted to symtab",
                self.def_path()
            ),
            None => Ok(()),
        }
        */
    }

    pub fn finish_function(
        &mut self,
        _expected_name: String,
        _return_value: ir::ValueId,
    ) -> Result<symtab::Function, LinearizeError> {
        unimplemented!();
        /*
            let def_path = self.def_path().clone();
            self.function_mut().resolve_final_convergence();
            self.function_mut().ensure_finished().unwrap();
            let function_context = self.functions.remove(&def_path).unwrap();
            let function = self.lookup_at_mut::<symtab::Function>(&def_path).unwrap();
            match function.control_flow.replace(function_context.take()) {
                Some(_) => return Err(()),
                None => (),
            }
            self.pop_def_path(DefPathComponent::Function(expected_name))
                .unwrap();
            Ok(())
        }

        // finish a function which has already had the finish() called once, assert that no branches
        // are open
        pub fn refinish_function(&mut self, expected_name: FunctionName) -> Result<(), ()> {
            let def_path = self.def_path().clone();
            self.function_mut().ensure_finished().unwrap();
            let function_context = self.functions.remove(&def_path).unwrap();
            let function = self.lookup_at_mut::<symtab::Function>(&def_path).unwrap();
            match function.control_flow.replace(function_context.take()) {
                Some(_) => return Err(()),
                None => (),
            }
            self.pop_def_path(DefPathComponent::Function(expected_name))
                .unwrap();
            Ok(())
        */
    }

    pub fn function(&self) -> &FunctionLinearizeCtx {
        unimplemented!();
        /*
        let mut scan_def_path = self.def_path().clone();
        while scan_def_path.len() > 0 {
            if self.functions.contains_key(&scan_def_path) {
                break;
            } else {
                scan_def_path.pop().unwrap();
            }
        }

        if scan_def_path.len() > 0 {
            self.functions.get(&scan_def_path).unwrap()
        } else {
            panic!("DefContext::function() called with no active function!");
        }
        */
    }

    pub fn function_mut(&mut self) -> &mut FunctionLinearizeCtx {
        unimplemented!();
        /*
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
            panic!("DefContext::function_mut() called with no active function!");
        }
        */
    }

    pub fn push_def_path(
        &mut self,
        _component: symtab::PathSegment,
        _generic_params: &types::GenericParamsList,
    ) {
        unimplemented!();
        /*
        let params_set = generic_params
            .iter()
            .map(|param| param.clone())
            .collect::<BTreeSet<types::GenericParam>>();
        assert_eq!(
            params_set.len(),
            generic_params.len(),
            "Unchecked duplicate generic param"
        );

        trace::debug!("push {:?} to defcontext defpath", component);
        self.definition_path.push(component).unwrap();
        let new_def_path = self.def_path().clone();
        self.generics_mut()
            .add_params_at_path(new_def_path, params_set)
            .unwrap();
        */
    }

    // FUTURE: error type for pop def path here and in symbol collection context?
    pub fn pop_def_path(
        &mut self,
        _expect: symtab::PathSegment,
    ) -> Result<(), (symtab::PathSegment, symtab::PathSegment)> {
        unimplemented!();
        /*
        let def_path = self.def_path().clone();
        self.generics_mut().remove_params_at_path(def_path).unwrap();
        let popped = self.definition_path.pop().unwrap();

        trace::debug!("pop {:?} from defcontext defpath", popped);

        if popped == expect {
            Ok(())
        } else {
            Err((popped, expect))
        }
        */
    }

    // reserves a subscope, returning its defpath
    pub fn reserve_subscope(&mut self) -> symtab::DefPath {
        unimplemented!();
        /*
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
            .define::<symtab::Scope>(
                self.def_path().clone(),
                symtab::Scope::new(next_subscope_index),
            )
            .unwrap()
        */
    }

    pub fn self_variable(&self) -> Result<symtab::DefPath, symtab::SymbolError> {
        unimplemented!();
        /*
        Ok(self
            .lookup_with_path::<symtab::Variable>(&String::from("self"))?
            .1)
        */
    }

    // resolves a string type name to either a defined type or a generic param
    pub fn disambiguate_named_type(
        &self,
        _name: &str,
    ) -> Result<types::Syntactic, symtab::SymbolError> {
        unimplemented!();
        /*
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
                Some(params) => match params.get(&types::GenericParam::TypeParam(name.into())) {
                    Some(param) => match param {
                        types::GenericParam::TypeParam(type_param_name) => {
                            type_ = Some(types::Syntactic::GenericParam(type_param_name.clone()));
                            break;
                        }
                    },
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
        */
    }

    pub fn semantic_type_for_syntactic(
        &self,
        _ty: types::Syntactic,
        _params: types::ParamSubstMap,
    ) -> Result<types::Semantic, symtab::SymbolError> {
        unimplemented!();
        /*
        let (_, type_path) = self.lookup_with_path::<symtab::TypeDefinition>(&ty)?;
        Ok(self
            .symtab()
            .types
            .semantic_for_defpath(type_path, params)
            .unwrap())
        */
    }

    #[allow(dead_code)]
    fn self_type(&self) -> Option<types::Syntactic> {
        unimplemented!();
        /*
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
        */
    }
}

impl symtab::Symtab for UnpathedLinearizeCtx {
    fn insert(
        &mut self,
        path: symtab::DefPath,
        maybe_symbol: Option<symtab::SymbolDef>,
    ) -> Result<symtab::DefPath, symtab::SymbolError> {
        self.symtab.insert(path, maybe_symbol)
    }

    fn lookup_at(
        &self,
        path: &symtab::DefPath,
    ) -> Result<Option<&symtab::SymbolDef>, symtab::SymbolError> {
        self.symtab.lookup_at(path)
    }
}

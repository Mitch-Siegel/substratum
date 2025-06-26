use crate::midend::{symtab::*, types};

pub struct MutSymtabVisitor<C> {
    on_module: Option<fn(&mut Module, &mut C)>,
    on_function: Option<fn(&mut Function, &mut C)>,
    on_type_definition: Option<fn(&mut TypeDefinition, &mut C)>,
    on_associated: Option<fn(&mut Function, &types::Type, &mut C)>,
    on_method: Option<fn(&mut Function, &types::Type, &mut C)>,
}

impl<C> MutSymtabVisitor<C> {
    pub fn new(
        on_module: Option<fn(&mut Module, &mut C)>,
        on_function: Option<fn(&mut Function, &mut C)>,
        on_type_definition: Option<fn(&mut TypeDefinition, &mut C)>,
        on_associated: Option<fn(&mut Function, &types::Type, &mut C)>,
        on_method: Option<fn(&mut Function, &types::Type, &mut C)>,
    ) -> Self {
        Self {
            on_module,
            on_function,
            on_type_definition,
            on_associated: on_associated,
            on_method,
        }
    }

    fn visit_function(&self, function: &mut Function, context: &mut C) {
        if let Some(on_function) = self.on_function {
            on_function(function, context);
        }
        // TODO: type definitions here
    }

    fn visit_type_definition(&self, type_definition: &mut TypeDefinition, context: &mut C) {
        if let Some(on_type_definition) = self.on_type_definition {
            on_type_definition(type_definition, context);
        }

        let defined_type = type_definition.type_().clone();

        if let Some(on_associated) = self.on_associated {
            for associated in type_definition.associated_functions_mut() {
                on_associated(associated, &defined_type, context);
            }
        }

        if let Some(on_method) = self.on_method {
            for method in type_definition.methods_mut() {
                on_method(method, &defined_type, context);
            }
        }
    }

    fn visit_module(&self, module: &mut Module, context: &mut C) {
        // first, call on_module if we have it
        if let Some(on_module) = self.on_module {
            on_module(module, context)
        }

        // then, visit all submodules
        for submodule in module.submodules_mut() {
            self.visit_module(submodule, context);
        }

        for function in module.functions_mut() {
            self.visit_function(function, context);
        }

        // type defintions within the module
        if let Some(on_type_definition) = self.on_type_definition {
            for type_definition in module.types_mut() {
                on_type_definition(type_definition, context);
            }
        }
    }

    pub fn visit(self, module: &mut Module, context: &mut C) {
        self.visit_module(module, context);
    }
}

pub struct SymtabVisitor<'a, C> {
    parent_modules: Vec<&'a Module>,
    on_module: Option<fn(&Module, &Self, &mut C)>,
    on_function: Option<fn(&Function, &Self, &mut C)>,
    on_type_definition: Option<fn(&TypeDefinition, &Self, &mut C)>,
    on_associated: Option<fn(&Function, &types::Type, &Self, &mut C)>,
    on_method: Option<fn(&Function, &types::Type, &Self, &mut C)>,
}

impl<'a, C> SymtabVisitor<'a, C> {
    pub fn new(
        on_module: Option<fn(&Module, &Self, &mut C)>,
        on_function: Option<fn(&Function, &Self, &mut C)>,
        on_type_definition: Option<fn(&TypeDefinition, &Self, &mut C)>,
        on_associated: Option<fn(&Function, &types::Type, &Self, &mut C)>,
        on_method: Option<fn(&Function, &types::Type, &Self, &mut C)>,
    ) -> Self {
        Self {
            parent_modules: Vec::new(),
            on_module,
            on_function,
            on_type_definition,
            on_associated: on_associated,
            on_method,
        }
    }

    fn all_modules(&self) -> impl Iterator<Item = &'a Module> {
        self.parent_modules
            .iter()
            .rev()
            .map(|module: &&Module| *module)
            .collect::<Vec<_>>()
            .into_iter()
    }

    fn visit_function(&self, function: &Function, context: &mut C) {
        if let Some(on_function) = self.on_function {
            on_function(function, self, context);
        }
        // TODO: type definitions here
    }

    fn visit_type_definition(&self, type_definition: &TypeDefinition, context: &mut C) {
        if let Some(on_type_definition) = self.on_type_definition {
            on_type_definition(type_definition, self, context);
        }

        let defined_type = type_definition.type_().clone();

        if let Some(on_associated) = self.on_associated {
            for associated in type_definition.associated_functions() {
                on_associated(associated, &defined_type, self, context);
            }
        }

        if let Some(on_method) = self.on_method {
            for method in type_definition.methods() {
                on_method(method, &defined_type, self, context);
            }
        }
    }

    fn visit_module(&mut self, module: &'a Module, context: &mut C) {
        // first, call on_module if we have it
        if let Some(on_module) = self.on_module {
            on_module(module, self, context)
        }

        self.parent_modules.push(module);

        // then, visit all submodules
        for submodule in module.submodules() {
            self.visit_module(submodule, context);
        }

        for function in module.functions() {
            self.visit_function(function, context);
        }

        // type defintions within the module
        if let Some(on_type_definition) = self.on_type_definition {
            for type_definition in module.types() {
                on_type_definition(type_definition, self, context);
            }
        }

        self.parent_modules.pop().unwrap();
    }

    pub fn visit(mut self, module: &'a Module, context: &mut C) {
        self.visit_module(module, context);
    }
}

impl<'a, C> TypeOwner for SymtabVisitor<'a, C> {
    fn types(&self) -> impl Iterator<Item = &TypeDefinition> {
        self.parent_modules
            .iter()
            .rev()
            .flat_map(|module| module.types())
    }

    fn lookup_type(&self, type_: &Type) -> Result<&TypeDefinition, UndefinedSymbol> {
        for module in self.all_modules() {
            match module.lookup_type(type_) {
                Ok(type_) => return Ok(type_),
                Err(_) => (),
            }
        }

        Err(UndefinedSymbol::type_(type_.clone()))
    }
}

impl<'a, C> ModuleOwnerships for SymtabVisitor<'a, C> {}

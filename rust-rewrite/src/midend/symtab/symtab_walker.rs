use crate::midend::{symtab::*, types};

enum ModuleItem<'a> {
    Function(&'a Function),
    TypeDefinition(&'a TypeDefinition),
}

pub struct SymtabWalker<'a, C> {
    module_stack: Vec<&'a Module>,
    start_module: &'a Module,
    scope_stack: Vec<&'a Scope>,

    on_module: Option<fn(&'a Module, &Self, &mut C)>,
    on_function: Option<fn(&'a Function, &Self, &mut C)>,
    on_type_definition: Option<fn(&'a TypeDefinition, &Self, &mut C)>,
    on_associated: Option<fn(&'a Function, types::Type, &Self, &mut C)>,
    on_method: Option<fn(&'a Function, types::Type, &Self, &mut C)>,
    on_scope: Option<fn(&'a Scope, &'a Function, &Self, &mut C)>,
}

impl<'a, C> SymtabWalker<'a, C> {
    pub fn new(
        start_module: &'a Module,
        on_module: Option<fn(&'a Module, &Self, &mut C)>,
        on_function: Option<fn(&'a Function, &Self, &mut C)>,
        on_type_definition: Option<fn(&'a TypeDefinition, &Self, &mut C)>,
        on_associated: Option<fn(&'a Function, types::Type, &Self, &mut C)>,
        on_method: Option<fn(&'a Function, types::Type, &Self, &mut C)>,
        on_scope: Option<fn(&'a Scope, &'a Function, &Self, &mut C)>,
    ) -> Self {
        Self {
            module_stack: Vec::new(),
            start_module,
            scope_stack: Vec::new(),
            on_module,
            on_function,
            on_type_definition,
            on_associated: on_associated,
            on_method,
            on_scope,
        }
    }

    fn walk_scope(&mut self, scope: &'a Scope, function: &'a Function, context: &mut C) {
        self.scope_stack.push(scope);
        if let Some(on_scope) = self.on_scope {
            on_scope(scope, function, self, context);
        }

        for subscope in scope.subscopes() {
            self.walk_scope(subscope, function, context);
        }

        for type_definition in scope.types() {
            self.walk_type_definition(type_definition, context);
        }

        self.scope_stack.pop().unwrap();
    }

    fn walk_function(&mut self, function: &'a Function, context: &mut C) {
        if let Some(on_function) = self.on_function {
            on_function(function, self, context);
        }

        self.walk_scope(&function.scope, function, context);
    }

    fn walk_type_definition(&self, type_definition: &'a TypeDefinition, context: &mut C) {
        if let Some(on_type_definition) = self.on_type_definition {
            on_type_definition(type_definition, self, context);
        }

        if let Some(on_associated) = self.on_associated {
            for associated in type_definition.associated_functions() {
                on_associated(associated, type_definition.type_().clone(), self, context);
            }
        }

        if let Some(on_method) = self.on_method {
            for method in type_definition.methods() {
                on_method(method, type_definition.type_().clone(), self, context);
            }
        }
    }

    fn walk_module(&mut self, module: &'a Module, context: &mut C) {
        // first, walk all submodules
        self.module_stack.push(module);
        for submodule in module.submodules.values() {
            self.walk_module(submodule, context);
        }
        self.module_stack.pop().unwrap();

        // then do the on_module for the top level module
        if let Some(on_module) = self.on_module {
            on_module(module, self, context)
        }

        // functions within the module
        for function in module.functions.values() {
            self.walk_function(function, context);
        }

        // type defintions within the module
        if let Some(on_type_definition) = self.on_type_definition {
            for type_definition in module.type_definitions.values() {
                on_type_definition(type_definition, self, context);
            }
        }
    }

    pub fn walk(mut self, context: &mut C) {
        self.walk_module(self.start_module, context);
    }
}

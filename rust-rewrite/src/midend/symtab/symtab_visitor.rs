use crate::midend::{symtab::*, types};

pub struct SymtabVisitor<C> {
    on_module: Option<fn(&mut Module, &mut C)>,
    on_module_done: Option<fn(&mut Module, &mut C)>,
    on_function: Option<fn(&mut Function, &mut C)>,
    on_type_definition: Option<fn(&mut TypeDefinition, &mut C)>,
    on_associated: Option<fn(&mut Function, &types::Type, &mut C)>,
    on_method: Option<fn(&mut Function, &types::Type, &mut C)>,
}

impl<C> SymtabVisitor<C> {
    pub fn new(
        on_module: Option<fn(&mut Module, &mut C)>,
        on_module_done: Option<fn(&mut Module, &mut C)>,
        on_function: Option<fn(&mut Function, &mut C)>,
        on_type_definition: Option<fn(&mut TypeDefinition, &mut C)>,
        on_associated: Option<fn(&mut Function, &types::Type, &mut C)>,
        on_method: Option<fn(&mut Function, &types::Type, &mut C)>,
    ) -> Self {
        Self {
            on_module,
            on_module_done,
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

        // lastly, if we have an on_module_done, do it
        if let Some(on_module_done) = self.on_module_done {
            on_module_done(module, context);
        }
    }

    pub fn visit(self, module: &mut Module, context: &mut C) {
        self.visit_module(module, context);
    }
}

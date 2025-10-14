use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct ModuleTree {
    pub module_path: Vec<String>,
    pub name: String,
    pub items: Vec<Item>,
}

impl Display for ModuleTree {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "Module {}", self.name)?;
        for item in &self.items {
            writeln!(f, " - {}", item)?;
        }
        Ok(())
    }
}

impl treewalk::CollectSymbols for ModuleTree {
    fn collect_symbols(&self, ctx: &mut treewalk::CollectCtx) {
        println!("collect for module {}", self.name);
        println!("{:?}", ctx.def_path());
        let module_component = midend::symtab::DefPathComponent::Module(
            midend::symtab::ModuleName::new(self.name.clone()),
        );
        ctx.declare(module_component.clone()).unwrap();

        ctx.push_def_path(module_component.clone()).unwrap();

        for item in &self.items {
            item.collect_symbols(ctx);
        }
        ctx.pop_def_path(module_component).unwrap()
    }
}

impl treewalk::Linearize<()> for ModuleTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn linearize(self, context: &mut treewalk::LinearizeCtx) -> () {
        tracing::trace!(
            "Create symtab module \"{}\" at \"{}\"",
            self.name,
            context.def_path()
        );
        context
            .insert(midend::symtab::symbol::Module::new(self.name.clone()))
            .unwrap();
        context.push_def_path(
            midend::symtab::DefPathComponent::Module(midend::symtab::ModuleName {
                name: self.name.clone(),
            }),
            &Vec::new(),
        );

        for item in self.items {
            item.linearize(context)
        }

        context
            .pop_def_path(midend::symtab::DefPathComponent::Module(
                midend::symtab::ModuleName { name: self.name },
            ))
            .unwrap();
    }
}

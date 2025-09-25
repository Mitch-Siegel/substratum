use crate::frontend::ast::*;

#[derive(ReflectName, Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
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

impl midend::linearizer::Walk<()> for ModuleTree {
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(self, context: &mut midend::linearizer::WalkContext) -> () {
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
            item.walk(context)
        }

        context
            .pop_def_path(midend::symtab::DefPathComponent::Module(
                midend::symtab::ModuleName { name: self.name },
            ))
            .unwrap();
    }
}

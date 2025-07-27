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

impl CustomReturnWalk<midend::linearizer::BasicDefContext, midend::linearizer::BasicDefContext>
    for ModuleTree
{
    #[tracing::instrument(skip(self), level = "trace", fields(tree_name = Self::reflect_name()))]
    fn walk(
        self,
        mut context: midend::linearizer::BasicDefContext,
    ) -> midend::linearizer::BasicDefContext {
        tracing::trace!(
            "Create symtab module \"{}\" at \"{}\"",
            self.name,
            context.def_path()
        );
        context
            .insert(midend::symtab::symbol::Module::new(self.name.clone()))
            .unwrap();
        context
            .def_path_mut()
            .push(midend::symtab::DefPathComponent::Module(
                midend::symtab::ModuleName {
                    name: self.name.clone(),
                },
            ))
            .unwrap();

        for item in self.items {
            context = item.walk(context)
        }

        assert_eq!(
            context.def_path_mut().pop(),
            Some(midend::symtab::DefPathComponent::Module(
                midend::symtab::ModuleName { name: self.name }
            )),
        );
        context
    }
}

use crate::{frontend, trace};
use std::collections::BTreeMap;

mod idfa;
pub mod ir;
mod monomorphization;
mod optimization;
pub mod treewalk;
//mod ssa_gen;
pub mod symtab;
pub mod types;

fn functions_to_graphviz(symtab: &symtab::SymbolTable, suffix: String) {
    let _ = symtab::Visitor::visit_with_starting_data(
        symtab,
        |_path, symbol, suffix| match symbol {
            symtab::SymbolDef::Function(f) => {
                if let Some(cf) = &f.control_flow {
                    {
                        use std::io::Write;
                        let path_string = format!("graphviz/{}{}.txt", f.name(), suffix);
                        let filepath = std::path::Path::new(&path_string);
                        std::fs::create_dir_all(filepath.parent().unwrap()).unwrap();
                        let mut file = std::fs::File::create(filepath).unwrap();
                        file.write(cf.graphviz_string().as_bytes()).unwrap();
                    }
                } else {
                    panic!();
                }
            }
            _ => (),
        },
        suffix,
    );
}

fn get_all_function_arguments(
    path: &symtab::DefPath,
    def: &symtab::SymbolDef,
    ctx: &mut BTreeMap<symtab::DefPath, Vec<symtab::DefPath>>,
) {
    match def {
        symtab::SymbolDef::Function(f) => {
            let entry = ctx.entry(path.clone()).or_default();
            for arg in &f.prototype.arguments {
                entry.push(
                    path.clone()
                        .with_component(symtab::DefPathComponent::Variable(arg.name.clone()))
                        .unwrap(),
                );
            }
        }
        _ => (),
    }
}

fn assign_types_to_function_arguments(
    symtab: &mut symtab::SymbolTable,
    all_arguments: BTreeMap<symtab::DefPath, Vec<symtab::DefPath>>,
) {
    for (function_path, arguments) in all_arguments.into_iter() {
        let arg_types: BTreeMap<symtab::DefPath, types::Semantic> = arguments
            .into_iter()
            .map(|arg_path| {
                let argument = symtab.lookup_at::<symtab::Variable>(&arg_path).unwrap();
                let argument_type = symtab
                    .semantic_type_for_syntactic(
                        &function_path,
                        types::ParamSubstMap::empty(),
                        argument
                            .type_()
                            .expect("function arguments must have a type"),
                    )
                    .unwrap();
                (arg_path, argument_type)
            })
            .collect();

        let function = symtab
            .lookup_at_mut::<symtab::Function>(&function_path)
            .unwrap();

        if let Some(cf) = &mut function.control_flow {
            let values = cf.values_mut();
            for (arg, ty_) in arg_types {
                println!("assign type {} to arg {:?}", ty_, arg);
                let arg_value = values.id_for_variable(arg);
                values.assign_type_to_id(&arg_value, ty_).unwrap();
            }
        }
    }
}

pub fn symbol_table_from_modules(modules: Vec<frontend::ast::ModuleTree>) -> symtab::SymbolTable {
    let _ = trace::span_auto!(trace::Level::DEBUG, "Generate symbol table from AST");

    tracing::debug!("Walk AST");
    let mut symtab = treewalk::walk(modules);

    functions_to_graphviz(&symtab, "_unlowered".into());

    for path in symtab.decls() {
        println!("{}", path);
    }
    /*
    let all_arguments = symtab::Visitor::visit(&symtab, get_all_function_arguments);

    monomorphization::monomorphize_generics(&mut symtab);
    assign_types_to_function_arguments(&mut symtab, all_arguments);
    */
    symtab = ir::lowering::lower_symtab(symtab);
    ir::lowering::assert_lowered(&symtab);

    functions_to_graphviz(&symtab, "".into());

    //tracing::debug!("collapse scopes");
    //symtab.collapse_scopes();

    //tracing::debug!("convert IR to SSA");
    //ssa_gen::convert_functions_to_ssa(&mut symtab);

    // optimization::optimize_functions(&mut symtab.functions);
    //

    //tracing::debug!("convert IR back from SSA");
    //ssa_gen::remove_ssa_from_functions(&mut symtab);

    *symtab
}

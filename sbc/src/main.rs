mod args;
pub(crate) use args::Config;

fn main() {
    let cfg = Config::parse();

    dbg!("{}", &cfg);

    match &cfg.trace_file {
        None => {
            trace::subscriber::fmt()
                //.event_format(trace::Print::default())
                .pretty()
                .with_writer(std::io::stdout)
                .with_max_level(cfg.trace_level)
                .init();
        }
        Some(outfile_name) => {
            let outfile = std::fs::File::create(std::path::Path::new(outfile_name))
                .unwrap_or_else(|_| panic!("couldn't create trace file '{outfile_name}'"));
            let json_outfile = std::sync::Mutex::new(outfile);
            let writer = trace::subscriber::fmt::writer::BoxMakeWriter::new(json_outfile);
            trace::subscriber::fmt()
                .with_writer(writer)
                .with_max_level(cfg.trace_level)
                .init();
        }
    }

    assert!(
        !cfg.input_files.is_empty(),
        "input file must be provided! (-i [file])"
    );

    // let mut modules = BTreeSet::<frontend::ast::ModuleTree>::new();

    let modules = frontend::parse_crate(&cfg.crate_name, &cfg.bin_name, &cfg.crate_path);

    let _symtab = midend::symbol_table_from_modules(modules, &cfg.crate_name);
    //backend::do_backend(symtab);
}

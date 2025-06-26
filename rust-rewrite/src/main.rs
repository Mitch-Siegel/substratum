#![allow(dead_code)]

mod trace;

mod backend;
mod frontend;
mod midend;

mod map_ooo_iter;

//use backend::generate_code;
use frontend::{lexer::Lexer, parser::Parser};

const FIB_FUN: &str = "fun fib(u8 n) -> u64
{
    u64 result;
    result = 0;
    if (n > 0) {
        if(n == 0) {
            result = 0;
        } else {
         result = 1;
        }
    } else {
        result = (n - 1) - (n - 2);
    }
}";

const WHILE_LOOP: &str = "fun down_to_zero(input: u16)
{
    input = input;
    while(input > 0) {
        input = input - 1;
    }

    input = input + 1;
}";

const WHILE_LOOP_WITH_NESTED_BRANCH: &str = "
fun while_with_nested_branch(a: u8, b: u16, c: u32) {
    counter: u8;
    counter = 0;
    while (a < b) {
        if (c > 22) {
            counter = counter + 1;
        } else {
            counter = counter - 1;
        }
    }

    a = a + b;
}
";

const WHILE_LOOP_WITH_NESTED_BRANCH_NO_ARGS: &str = "
fun while_with_nested_branch() {
    a: u8; b: u16; c: u32;

    a = 0;
    b = 1;
    c = 2;
    while (a < b) {
        counter: u8;
        if (c > 22) {
            a = a + b;
        } else {
            b = b - 1;
        }
    }
}
";

const NESTED_WHILE_LOOPS: &str = "
fun while_with_nested_branch(a: u8, b: u64, c: u32) {
    while (a < b) {
        counter: u8;
        counter = 0;
        if (c > 22) {
            counter = counter + 1;
        } else {
            while (counter > 0) {
            counter = counter - 1;
            }
        }
    }

    a = a + b;
}
";

const SSA_EXAMPLE: &str = "
fun while_with_nested_branch() {
    a: u8; b: u16; c: u32;

    a = 0;
    b = 1;
    c = 2;
    
    a = b + c;
    b = a + c;
    if (a > b) {
    c = c + 1;
    } else {
     c = 1;}
    c = c + 1;
    c = c + 1;
}";

const STRUCT_EXAMPLE: &str = "struct Money {
dollars: u64,
cents: u8
}

impl Money {
    fun new(dollars: u64, cents: u8) -> Self {
    }

    fun print(&self) {
    }
}

fun money_add_dollars(m: Money, dollars: u64) {
    m.dollars = m.dollars + dollars;
    m.print();
}";

#[derive(Debug)]
enum TraceLocation {
    NoTrace,
    Stdout,
    File,
}

#[derive(Debug)]
struct CompilerArguments {
    trace_location: TraceLocation,
    trace_level: tracing::Level,
}

fn main() {
    println!("Hello, world!");

    let mut arguments = CompilerArguments {
        trace_location: TraceLocation::NoTrace,
        trace_level: tracing::Level::WARN,
    };

    let mut args_without_executable = std::env::args().map(|arg| arg).collect::<Vec<_>>();
    args_without_executable.remove(0);

    // TODO: real argument parsing
    for argument in args_without_executable {
        match argument.as_str() {
            "trace_file" => arguments.trace_location = TraceLocation::File,
            "trace_stdout" => arguments.trace_location = TraceLocation::Stdout,
            "trace_level_trace" => arguments.trace_level = tracing::Level::TRACE,
            "trace_level_debug" => arguments.trace_level = tracing::Level::DEBUG,
            "trace_level_info" => arguments.trace_level = tracing::Level::INFO,
            "trace_level_warn" => arguments.trace_level = tracing::Level::WARN,
            "trace_level_error" => arguments.trace_level = tracing::Level::ERROR,
            _ => panic!("Invalid argument '{}'", argument),
        }
        println!("{}", argument);
    }

    println!("{:?}", arguments);

    match arguments.trace_location {
        TraceLocation::NoTrace => (),
        TraceLocation::Stdout => {
            tracing_subscriber::fmt()
                //.event_format(trace::Print::default())
                .pretty()
                .with_writer(std::io::stdout)
                .with_max_level(arguments.trace_level)
                .init();
        }
        TraceLocation::File => {
            let json_outfile =
                std::fs::File::create("most_recent.json").expect("Couldn't create trace file");
            let json_outfile = std::sync::Mutex::new(json_outfile);
            let writer = tracing_subscriber::fmt::writer::BoxMakeWriter::new(json_outfile);
            tracing_subscriber::fmt()
                .json()
                .with_writer(writer)
                .with_max_level(arguments.trace_level)
                .init();
        }
    }

    let mut parser = Parser::new(Lexer::from_string(WHILE_LOOP));
    let program = parser.parse().expect("Error parsing input");

    for t in &program {
        println!("{}", t);
    }

    let symtab = midend::symbol_table_from_program(program);
    //println!("{}", serde_json::to_string_pretty(&symtab).unwrap());
    backend::do_backend(symtab);
}

mod ast;
mod lexer;
mod midend;
mod backend;
mod parser;

use backend::generate_code;

use crate::lexer::Lexer;
use crate::midend::SymbolTable;
use crate::midend::TableWalk;
use crate::parser::Parser;

fn main() {
    println!("Hello, world!");
    // let parsed = String::from("fun function(u8 abc, u32 def){u8 abc;u32 def;abc = 1;abc = def;def = 123 + abc}");
    let parsed = String::from(
        "fun fib(u8 n) -> u64 {
            u64 result;
            result = 0;
            if (n > 0) {
                result = n;
            } else {
                result = (n - 1) - (n - 2);
            }
        }",
    );
    let mut parser = Parser::new(Lexer::new(parsed.chars()));
    let program = parser.parse();
    for t in &program {
        println!("{}", t);
    }
    let mut symtab = SymbolTable::new();
    for translation_unit in program {
        translation_unit.walk(&mut symtab);
    }

    // println!("{}", serde_json::to_string_pretty(&symtab).unwrap());
    symtab.print_ir();

    generate_code(symtab);

    
}

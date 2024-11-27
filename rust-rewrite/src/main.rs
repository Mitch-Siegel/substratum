mod ast;
mod lexer;
mod midend;
mod parser;

use crate::lexer::Lexer;
use crate::midend::SymbolTable;
use crate::midend::TableWalk;
use crate::parser::Parser;

fn main() {
    println!("Hello, world!");
    // let parsed = String::from("fun function(u8 abc, u32 def){u8 abc;u32 def;abc = 1;abc = def;def = 123 + abc}");
    let parsed =
        String::from("fun add(u8 number_1, u8 number_2){u8 result; result = number_1 + number_2;}");
    let mut parser = Parser::new(Lexer::new(parsed.chars()));
    let program = parser.parse();
    for t in &program {
        println!("{}", t);
    }
    let mut symtab = SymbolTable::new();
    for translation_unit in program {
        translation_unit.walk(&mut symtab);
    }

    println!("{:?}", symtab);
}

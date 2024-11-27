mod parser;
mod lexer;
mod ast;

use crate::parser::Parser;
use crate::lexer::Lexer;

fn main() {
    println!("Hello, world!");
    // let parsed = String::from("fun function(u8 abc, u32 def){u8 abc;u32 def;abc = 1;abc = def;def = 123 + abc}");
    let parsed = String::from("fun function(u8 abc, u32 def){def = 123 + abc}");
    let mut parser = Parser::new(Lexer::new(parsed.chars()));
    let result = parser.parse();
    for t in result
    {
        println!("{}", t);
    }
}

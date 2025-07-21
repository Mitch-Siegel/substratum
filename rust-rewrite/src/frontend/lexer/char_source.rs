use std::{
    fs::File,
    io::{BufRead, BufReader},
};

pub trait ReadChar {
    fn read_char(&mut self) -> Option<char>;
}

pub trait ReadLine {
    fn read_line(&mut self) -> Option<Vec<char>>;
}

#[derive(Debug)]
pub struct CharReader<T>
where
    T: ReadLine,
{
    line: Option<Vec<char>>,
    line_source: T,
}

impl<T> CharReader<T>
where
    T: ReadLine,
{
    fn new(mut line_source: T) -> Self {
        Self {
            line: line_source.read_line(),
            line_source,
        }
    }

    fn next_line(&mut self) {
        self.line = self.line_source.read_line();
    }
}

impl<T> ReadChar for CharReader<T>
where
    T: ReadLine,
{
    fn read_char(&mut self) -> Option<char> {
        loop {
            if let Some(line) = &mut self.line {
                if !line.is_empty() {
                    return line.pop();
                } else {
                    self.next_line();
                }
            } else {
                // First call or exhausted previous line
                self.next_line();

                if self.line.is_none() {
                    return None;
                }
            }
        }
    }
}

#[derive(Debug)]
pub struct FileLineReader {
    lines: std::io::Lines<BufReader<File>>,
}

impl FileLineReader {
    pub fn new(f: File) -> Self {
        let reader = BufReader::new(f);
        FileLineReader {
            lines: reader.lines(),
        }
    }
}

impl ReadLine for FileLineReader {
    fn read_line(&mut self) -> Option<Vec<char>> {
        let line_option = match self.lines.next() {
            Some(result) => match result {
                Ok(line) => Some(line),
                Err(e) => panic!("Couldn't read next line from file: {}", e),
            },
            None => None,
        };

        match line_option {
            Some(string) => Some(string.chars().rev().chain(std::iter::once('\n')).collect()),
            None => None,
        }
    }
}

#[derive(Debug)]
pub struct StrLineReader<'a> {
    lines: std::str::SplitInclusive<'a, char>,
}

impl<'a> StrLineReader<'a> {
    pub fn new(s: &'a str) -> Self {
        Self {
            lines: s.split_inclusive('\n').to_owned(),
        }
    }
}

impl<'a> ReadLine for StrLineReader<'a> {
    fn read_line(&mut self) -> Option<Vec<char>> {
        let line_option = self.lines.next();
        match line_option {
            Some(string) => Some(string.chars().rev().collect()),
            None => None,
        }
    }
}

#[derive(Debug)]
pub enum CharSource<'a> {
    File(CharReader<FileLineReader>),
    String(CharReader<StrLineReader<'a>>),
}

impl<'a> CharSource<'a> {
    pub fn from_file(f: File) -> Self {
        Self::File(CharReader::<FileLineReader>::new(FileLineReader::new(f)))
    }

    pub fn from_str(s: &'a str) -> Self {
        Self::String(CharReader::<StrLineReader>::new(StrLineReader::new(s)))
    }
}

impl<'a> ReadChar for CharSource<'a> {
    fn read_char(&mut self) -> Option<char> {
        match self {
            Self::File(f) => f.read_char(),
            Self::String(s) => s.read_char(),
        }
    }
}

impl<'a> Iterator for CharSource<'a> {
    type Item = char;

    fn next(&mut self) -> Option<Self::Item> {
        let c = self.read_char();
        /*
        match &c {
            Some(ch) => print!("{}", ch),
            None => print!("NONE"),
        }
        */

        c
    }
}

#[cfg(test)]
mod tests {
    use crate::frontend::lexer::CharSource;

    #[test]
    fn string() {
        let char_source = CharSource::from_str(
            "the quick brown
fox jumps
over
the
lazy
dog",
        );

        assert_eq!(
            char_source.into_iter().collect::<String>(),
            "the quick brown
fox jumps
over
the
lazy
dog"
        );
    }

    #[test]
    fn string_multiple_newline() {
        let char_source = CharSource::from_str(
            "the quick brown

fox jumps
over

the
lazy
dog",
        );

        assert_eq!(
            char_source.into_iter().collect::<String>(),
            "the quick brown

fox jumps
over

the
lazy
dog"
        );
    }

    #[test]
    fn string_newline_at_end() {
        let char_source = CharSource::from_str(
            "text

",
        );

        assert_eq!(
            char_source.into_iter().collect::<String>(),
            "text

"
        );
    }

    #[test]
    fn string_newline_in_middle_and_at_end() {
        let char_source = CharSource::from_str(
            "some text


more text


even more text

",
        );

        assert_eq!(
            char_source.into_iter().collect::<String>(),
            "some text


more text


even more text

"
        );
    }

    #[test]
    fn empty_string() {
        let char_source = CharSource::from_str("");

        assert_eq!(char_source.into_iter().collect::<String>(), "");
    }

    #[test]
    fn only_newline() {
        let char_source = CharSource::from_str("\n");

        assert_eq!(char_source.into_iter().collect::<String>(), "\n");
    }
}

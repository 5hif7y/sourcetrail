//! Scopes and kinds from source text, for indexes that lack them.
//!
//! scip-dotnet 0.2 writes no enclosing ranges and no symbol kinds. Without
//! enclosing ranges every reference would hang from its file. For C-like
//! languages the body of a definition can be found in the text: it runs from
//! the definition's name to the matching `}` of the first `{`, or to the `;`
//! that ends a `=>` expression body or a declaration. Comments, strings and
//! character literals are skipped. The keyword before a type's name (`class`,
//! `interface`, ...) gives its kind.

use crate::writer::{Range, SymbolKind};

/// The code of a file without comments and literals: each significant
/// character with its 1-based line and column. Literals become one `"`.
pub struct Code {
    chars: Vec<(char, i32, i32)>,
}

/// Languages whose bodies are delimited by braces.
pub fn is_brace_language(language: &str) -> bool {
    matches!(
        language.to_ascii_lowercase().as_str(),
        "c#" | "csharp"
            | "java"
            | "kotlin"
            | "scala"
            | "typescript"
            | "javascript"
            | "go"
            | "dart"
            | "swift"
    )
}

impl Code {
    pub fn new(text: &str) -> Code {
        let lines: Vec<Vec<char>> = text.lines().map(|l| l.chars().collect()).collect();
        let mut chars = Vec::new();
        let mut in_block_comment = false;
        // A literal can span lines (verbatim or raw strings): its closing
        // quote count, and whether "" escapes a quote.
        let mut open_string: Option<(usize, bool)> = None;
        for (li, line) in lines.iter().enumerate() {
            let ln = li as i32 + 1;
            let mut i = 0;
            while i < line.len() {
                let c = line[i];
                let next = line.get(i + 1).copied();
                if in_block_comment {
                    if c == '*' && next == Some('/') {
                        in_block_comment = false;
                        i += 2;
                    } else {
                        i += 1;
                    }
                    continue;
                }
                if let Some((quotes, verbatim)) = open_string {
                    if c == '\\' && !verbatim && quotes == 1 {
                        i += 2;
                        continue;
                    }
                    if c == '"' {
                        if verbatim && quotes == 1 && next == Some('"') {
                            i += 2;
                            continue;
                        }
                        let run = line[i..].iter().take_while(|&&q| q == '"').count();
                        if run >= quotes {
                            open_string = None;
                            i += quotes;
                            continue;
                        }
                        i += run;
                        continue;
                    }
                    i += 1;
                    continue;
                }
                match c {
                    '/' if next == Some('/') => break,
                    '/' if next == Some('*') => {
                        in_block_comment = true;
                        i += 2;
                    }
                    '"' | '`' => {
                        let run = line[i..].iter().take_while(|&&q| q == c).count();
                        let quotes = if run >= 3 { run } else { 1 };
                        let verbatim =
                            i > 0 && (line[i - 1] == '@' || (i > 1 && line[i - 2] == '@'));
                        chars.push(('"', ln, i as i32 + 1));
                        open_string = Some((quotes, verbatim || c == '`'));
                        i += quotes;
                        if c == '`' {
                            // Template literals end at the next backtick.
                            open_string = None;
                            if let Some(end) = line[i..].iter().position(|&q| q == '`') {
                                i += end + 1;
                            } else {
                                i = line.len();
                            }
                        }
                    }
                    '\'' => {
                        // A character literal: 'x', '\n', 'A'.
                        let mut j = i + 1;
                        if line.get(j) == Some(&'\\') {
                            j += 2;
                        }
                        while j < line.len() && line[j] != '\'' && j - i < 12 {
                            j += 1;
                        }
                        chars.push(('"', ln, i as i32 + 1));
                        i = j + 1;
                    }
                    c if c.is_whitespace() => i += 1,
                    _ => {
                        chars.push((c, ln, i as i32 + 1));
                        i += 1;
                    }
                }
            }
            if let Some((1, false)) = open_string {
                // A regular string does not continue on the next line.
                open_string = None;
            }
        }
        Code { chars }
    }

    /// Index of the first significant character after `range`.
    fn after(&self, range: &Range) -> usize {
        let end = (range.end_line, range.end_column);
        self.chars.partition_point(|&(_, l, c)| (l, c) <= end)
    }

    /// The body of the definition named at `token`: up to the matching `}`
    /// of the first `{`, or to the `;` that ends an expression body or a
    /// declaration. None if neither is found.
    pub fn scope(&self, token: &Range) -> Option<Range> {
        let mut i = self.after(token);
        let mut parens = 0i32;
        let mut expression_body = false;
        while i < self.chars.len() {
            let (c, l, col) = self.chars[i];
            match c {
                '(' | '[' => parens += 1,
                ')' | ']' => parens -= 1,
                '=' if parens == 0 && self.chars.get(i + 1).is_some_and(|n| n.0 == '>') => {
                    expression_body = true;
                    i += 1;
                }
                '{' if parens == 0 && !expression_body => {
                    let end = self.matching_brace(i)?;
                    let (_, el, ec) = self.chars[end];
                    return Some(Range {
                        start_line: token.start_line,
                        start_column: token.start_column,
                        end_line: el,
                        end_column: ec,
                    });
                }
                '{' => {
                    // A block inside an expression body (lambda, initializer).
                    i = self.matching_brace(i)?;
                }
                ';' if parens == 0 => {
                    return Some(Range {
                        start_line: token.start_line,
                        start_column: token.start_column,
                        end_line: l,
                        end_column: col,
                    });
                }
                '}' if parens == 0 => return None,
                _ => {}
            }
            i += 1;
        }
        None
    }

    fn matching_brace(&self, open: usize) -> Option<usize> {
        let mut depth = 0;
        for (j, &(c, _, _)) in self.chars.iter().enumerate().skip(open) {
            match c {
                '{' => depth += 1,
                '}' => {
                    depth -= 1;
                    if depth == 0 {
                        return Some(j);
                    }
                }
                _ => {}
            }
        }
        None
    }

    /// The kind given by the keyword before a type's name on its line:
    /// `public sealed class Square` -> Class.
    pub fn type_keyword(&self, token: &Range) -> Option<SymbolKind> {
        let start = (token.start_line, token.start_column);
        let first = self
            .chars
            .partition_point(|&(_, l, _)| l < token.start_line);
        let end = self.chars.partition_point(|&(_, l, c)| (l, c) < start);
        let before: String = self.chars[first..end].iter().map(|&(c, _, _)| c).collect();
        // Words are glued together without whitespace; look at the last
        // keyword that ends the prefix.
        const KEYWORDS: [(&str, SymbolKind); 9] = [
            ("interface", SymbolKind::Interface),
            ("class", SymbolKind::Class),
            ("struct", SymbolKind::Struct),
            ("record", SymbolKind::Class),
            ("enum", SymbolKind::Enum),
            ("delegate", SymbolKind::Typedef),
            ("trait", SymbolKind::Interface),
            ("object", SymbolKind::Class),
            ("type", SymbolKind::Typedef),
        ];
        KEYWORDS
            .iter()
            .filter_map(|(k, kind)| before.rfind(k).map(|p| (p, k.len(), *kind)))
            .filter(|(p, len, _)| {
                p + len == before.len() || before[p + len..].starts_with("struct")
            })
            .max_by_key(|(p, _, _)| *p)
            .map(|(_, _, kind)| kind)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn r(l: i32, c: i32, e: i32) -> Range {
        Range {
            start_line: l,
            start_column: c,
            end_line: l,
            end_column: e,
        }
    }

    const CS: &str = r#"namespace Demo;
public class Square : IShape // a { in a comment
{
    public double Side { get; }
    public Square(double side) { Side = side; }
    public double Area()
    {
        var s = "}"; /* } */ var c = '}';
        return s.Length * @"a""}";
    }
    public Square Scale(int f) => new Square(f) { };
    public abstract void Draw();
}
"#;

    #[test]
    fn scopes() {
        let code = Code::new(CS);
        // class Square: lines 2-13.
        assert_eq!(
            code.scope(&r(2, 14, 19)),
            Some(Range {
                start_line: 2,
                start_column: 14,
                end_line: 13,
                end_column: 1
            })
        );
        // Property with accessors.
        assert_eq!(
            code.scope(&r(4, 19, 22))
                .map(|s| (s.end_line, s.end_column)),
            Some((4, 31))
        );
        // Method whose body has braces in strings, comments and chars.
        assert_eq!(
            code.scope(&r(6, 19, 22))
                .map(|s| (s.end_line, s.end_column)),
            Some((10, 5))
        );
        // Expression body with an initializer block.
        assert_eq!(
            code.scope(&r(11, 19, 23))
                .map(|s| (s.end_line, s.end_column)),
            Some((11, 52))
        );
        // Declaration without a body.
        assert_eq!(
            code.scope(&r(12, 26, 29))
                .map(|s| (s.end_line, s.end_column)),
            Some((12, 32))
        );
    }

    #[test]
    fn keywords() {
        let code = Code::new(CS);
        assert_eq!(code.type_keyword(&r(2, 14, 19)), Some(SymbolKind::Class));
        let code = Code::new(
            "public interface IShape\n{\n}\ninternal record struct P(int X);\nenum E { A }",
        );
        assert_eq!(
            code.type_keyword(&r(1, 18, 23)),
            Some(SymbolKind::Interface)
        );
        assert_eq!(code.type_keyword(&r(4, 24, 24)), Some(SymbolKind::Struct));
        assert_eq!(code.type_keyword(&r(5, 6, 6)), Some(SymbolKind::Enum));
        assert_eq!(code.type_keyword(&r(3, 1, 1)), None);
    }
}

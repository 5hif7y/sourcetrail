//! Converts a SCIP index (index.scip) into a Sourcetrail database.
//!
//! The conversion works on the whole index but writes only the documents it
//! is asked for, so Sourcetrail can run it in batches of source files like
//! any other custom-command indexer. Names, kinds and definition kinds come
//! from the whole index, so a symbol is the same node in every batch.

pub mod names;
pub mod writer;

use names::Target;
use protobuf::{Enum, Message};
use scip::types::{
    occurrence, Document, Index, Occurrence, Severity, SymbolInformation, SymbolRole,
};
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};
use writer::{DefinitionKind, Range, ReferenceKind, SymbolKind, Writer};

#[derive(Clone, Debug, Default)]
pub struct Options {
    pub scip: PathBuf,
    pub database: PathBuf,
    /// Source files to write. Empty: every document of the index.
    pub sources: Vec<String>,
    /// Overrides the project root of the index metadata.
    pub project_root: Option<PathBuf>,
    /// Overrides the name delimiter (`::` for Rust, `.` otherwise).
    pub delimiter: Option<String>,
    /// Overrides the language recorded for each file.
    pub language: Option<String>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Stats {
    pub documents: usize,
    pub definitions: usize,
    pub references: usize,
    pub locals: usize,
    /// References with no enclosing definition; they hang from the file.
    pub references_without_context: usize,
    pub relationships: usize,
    pub diagnostics: usize,
    /// Requested sources that are not in the index.
    pub missing_sources: usize,
    pub skipped_occurrences: usize,
}

/// Reads and parses an index.scip file.
pub fn read_index(path: &Path) -> Result<Index, String> {
    let bytes = std::fs::read(path).map_err(|e| format!("cannot read {}: {e}", path.display()))?;
    Index::parse_from_bytes(&bytes)
        .map_err(|e| format!("{} is not a SCIP index: {e}", path.display()))
}

pub fn run(options: &Options) -> Result<Stats, String> {
    let index = read_index(&options.scip)?;
    convert(&index, options)
}

/// Turns `file:///D:/a%20b` or `file:///home/a` into a path.
pub fn uri_to_path(uri: &str) -> PathBuf {
    let rest = uri.strip_prefix("file://").unwrap_or(uri);
    let mut decoded = Vec::with_capacity(rest.len());
    let bytes = rest.as_bytes();
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 2 < bytes.len() {
            if let Ok(v) = u8::from_str_radix(&rest[i + 1..i + 3], 16) {
                decoded.push(v);
                i += 3;
                continue;
            }
        }
        decoded.push(bytes[i]);
        i += 1;
    }
    let mut s = String::from_utf8_lossy(&decoded).into_owned();
    // `/D:/x` on Windows.
    let b = s.as_bytes();
    if b.len() >= 3 && b[0] == b'/' && b[2] == b':' && b[1].is_ascii_alphabetic() {
        s.remove(0);
    }
    PathBuf::from(s)
}

/// A comparable form of a path: forward slashes, no `./`, and lower case on
/// Windows, where paths are case-insensitive.
pub fn normalize(path: &str) -> String {
    let mut s = path.replace('\\', "/");
    if let Some(rest) = s.strip_prefix("//?/") {
        s = rest.to_string();
    }
    while s.contains("/./") {
        s = s.replace("/./", "/");
    }
    if let Some(rest) = s.strip_prefix("./") {
        s = rest.to_string();
    }
    if cfg!(windows) {
        s = s.to_lowercase();
    }
    s
}

/// A SCIP range (0-based, end exclusive; three or four numbers) in
/// Sourcetrail coordinates (1-based, end inclusive).
pub fn to_range(r: &[i32]) -> Option<Range> {
    let (sl, sc, el, ec) = match *r {
        [l, s, e] => (l, s, l, e),
        [l, s, el, ec] => (l, s, el, ec),
        _ => return None,
    };
    if sl < 0 || sc < 0 || el < sl || ec < 0 {
        return None;
    }
    let start_line = sl + 1;
    let start_column = sc + 1;
    let (end_line, end_column) = if ec == 0 && el > sl {
        // Ends at the start of a line: keep the first column of that line.
        (el + 1, 1)
    } else {
        (el + 1, ec.max(if el == sl { start_column } else { 1 }))
    };
    Some(Range {
        start_line,
        start_column,
        end_line,
        end_column,
    })
}

fn occurrence_range(o: &Occurrence) -> Option<Range> {
    match &o.typed_range {
        Some(occurrence::Typed_range::SingleLineRange(r)) => {
            to_range(&[r.line, r.start_character, r.end_character])
        }
        Some(occurrence::Typed_range::MultiLineRange(r)) => {
            to_range(&[r.start_line, r.start_character, r.end_line, r.end_character])
        }
        // Newer range variants this crate does not know yet.
        _ => to_range(&o.range),
    }
}

fn enclosing_range(o: &Occurrence) -> Option<Range> {
    match &o.typed_enclosing_range {
        Some(occurrence::Typed_enclosing_range::SingleLineEnclosingRange(r)) => {
            to_range(&[r.line, r.start_character, r.end_character])
        }
        Some(occurrence::Typed_enclosing_range::MultiLineEnclosingRange(r)) => {
            to_range(&[r.start_line, r.start_character, r.end_line, r.end_character])
        }
        _ if o.enclosing_range.is_empty() => None,
        _ => to_range(&o.enclosing_range),
    }
}

fn has_role(o: &Occurrence, role: SymbolRole) -> bool {
    o.symbol_roles & role.value() != 0
}

fn contains(outer: &Range, inner: &Range) -> bool {
    (outer.start_line, outer.start_column) <= (inner.start_line, inner.start_column)
        && (inner.end_line, inner.end_column) <= (outer.end_line, outer.end_column)
}

/// What is known about the whole index, shared by every batch.
struct Global<'a> {
    info: HashMap<&'a str, &'a SymbolInformation>,
    defined: HashSet<&'a str>,
    delimiter: String,
    scheme_language: String,
}

impl<'a> Global<'a> {
    fn new(index: &'a Index, options: &Options) -> Global<'a> {
        let mut info = HashMap::new();
        let mut defined = HashSet::new();
        for s in &index.external_symbols {
            info.insert(s.symbol.as_str(), s);
        }
        for d in &index.documents {
            for s in &d.symbols {
                info.insert(s.symbol.as_str(), s);
            }
            for o in &d.occurrences {
                if has_role(o, SymbolRole::Definition) && !scip::symbol::is_local_symbol(&o.symbol)
                {
                    defined.insert(o.symbol.as_str());
                }
            }
        }
        let tool = index
            .metadata
            .tool_info
            .as_ref()
            .map(|t| t.name.clone())
            .unwrap_or_default();
        let first_language = index
            .documents
            .iter()
            .map(|d| d.language.as_str())
            .find(|l| !l.is_empty())
            .unwrap_or("");
        let delimiter = options
            .delimiter
            .clone()
            .unwrap_or_else(|| names::delimiter_for(first_language, &tool).to_string());
        let scheme_language = match tool.as_str() {
            "rust-analyzer" => "rust".to_string(),
            "scip-dotnet" => "csharp".to_string(),
            _ => first_language.to_ascii_lowercase(),
        };
        Global {
            info,
            defined,
            delimiter,
            scheme_language,
        }
    }
}

/// Node ids already recorded in this database, by SCIP symbol (and document,
/// for locals).
struct Batch<'a, 'g> {
    global: &'g Global<'a>,
    writer: Writer,
    globals: HashMap<String, Option<(i32, Option<SymbolKind>)>>,
    locals: HashMap<String, i32>,
    stats: Stats,
}

enum Resolved {
    Global(i32, Option<SymbolKind>),
    Local(i32),
}

impl<'a, 'g> Batch<'a, 'g> {
    fn resolve(&mut self, symbol: &str, document: &str) -> Result<Option<Resolved>, String> {
        if let Some(cached) = self.globals.get(symbol) {
            return Ok(cached.map(|(id, k)| Resolved::Global(id, k)));
        }
        match names::parse(symbol, document) {
            None => {
                self.globals.insert(symbol.to_string(), None);
                Ok(None)
            }
            Some(Target::Local(name)) => {
                if let Some(id) = self.locals.get(&name) {
                    return Ok(Some(Resolved::Local(*id)));
                }
                let id = self.writer.local_symbol(&name)?;
                self.locals.insert(name, id);
                Ok(Some(Resolved::Local(id)))
            }
            Some(Target::Global { name, kind }) => {
                let kind = match self.global.info.get(symbol) {
                    Some(info) => names::kind_from_info(info, kind),
                    None => kind,
                };
                let definition = if self.global.defined.contains(symbol) {
                    DefinitionKind::Explicit
                } else {
                    DefinitionKind::None
                };
                let id = self
                    .writer
                    .symbol(&self.global.delimiter, &name, kind, definition)?;
                self.globals.insert(symbol.to_string(), Some((id, kind)));
                Ok(Some(Resolved::Global(id, kind)))
            }
        }
    }

    fn document(&mut self, doc: &Document, path: &str, language: &str) -> Result<(), String> {
        let file = self.writer.file(path)?;
        if !language.is_empty() {
            self.writer.file_language(file, language)?;
        }
        self.stats.documents += 1;
        let rel = doc.relative_path.as_str();

        // Definitions with a body, sorted by start, to find the context of
        // each reference: the innermost one that contains it.
        let mut scopes: Vec<(Range, i32)> = Vec::new();
        let mut definition_token: HashMap<&str, Range> = HashMap::new();
        for o in &doc.occurrences {
            if !has_role(o, SymbolRole::Definition) {
                continue;
            }
            let Some(token) = occurrence_range(o) else {
                continue;
            };
            definition_token.entry(o.symbol.as_str()).or_insert(token);
            if let (Some(scope), Some(Resolved::Global(id, _))) =
                (enclosing_range(o), self.resolve(&o.symbol, rel)?)
            {
                scopes.push((scope, id));
            }
        }
        scopes.sort_by(|a, b| {
            (a.0.start_line, a.0.start_column)
                .cmp(&(b.0.start_line, b.0.start_column))
                .then((b.0.end_line, b.0.end_column).cmp(&(a.0.end_line, a.0.end_column)))
        });

        for o in &doc.occurrences {
            let Some(range) = occurrence_range(o) else {
                self.stats.skipped_occurrences += 1;
                continue;
            };
            for d in &o.diagnostics {
                if d.severity.value() == Severity::Error.value() {
                    self.writer.error(&d.message, false, file, range)?;
                    self.stats.diagnostics += 1;
                }
            }
            if o.symbol.is_empty() {
                continue;
            }
            let Some(target) = self.resolve(&o.symbol, rel)? else {
                self.stats.skipped_occurrences += 1;
                continue;
            };
            match target {
                Resolved::Local(id) => {
                    self.writer.local_symbol_location(id, file, range)?;
                    self.stats.locals += 1;
                }
                Resolved::Global(id, _) if has_role(o, SymbolRole::Definition) => {
                    self.writer.symbol_location(id, file, range)?;
                    if let Some(scope) = enclosing_range(o) {
                        self.writer.symbol_scope_location(id, file, scope)?;
                    }
                    self.stats.definitions += 1;
                }
                Resolved::Global(id, kind) => {
                    let context = innermost(&scopes, &range);
                    let source = match context {
                        Some(c) => c,
                        None => {
                            self.stats.references_without_context += 1;
                            file
                        }
                    };
                    let reference_kind = if has_role(o, SymbolRole::Import) {
                        ReferenceKind::Import
                    } else {
                        match kind {
                            Some(SymbolKind::Macro) => ReferenceKind::MacroUsage,
                            Some(k) if k.is_callable() => ReferenceKind::Call,
                            Some(k) if k.is_type() => ReferenceKind::TypeUsage,
                            _ => ReferenceKind::Usage,
                        }
                    };
                    let r = self.writer.reference(source, id, reference_kind)?;
                    self.writer.reference_location(r, file, range)?;
                    self.stats.references += 1;
                }
            }
        }

        // `impl Trait for T`, `class C : I`, and the methods that implement
        // or override another one.
        for info in &doc.symbols {
            for rel_ship in info.relationships.iter().filter(|r| r.is_implementation) {
                let (Some(Resolved::Global(source, kind)), Some(Resolved::Global(target, _))) = (
                    self.resolve(&info.symbol, rel)?,
                    self.resolve(&rel_ship.symbol, rel)?,
                ) else {
                    continue;
                };
                let k = if kind.is_some_and(|k| k.is_callable()) {
                    ReferenceKind::Override
                } else {
                    ReferenceKind::Inheritance
                };
                let r = self.writer.reference(source, target, k)?;
                if let Some(token) = definition_token.get(info.symbol.as_str()) {
                    self.writer.reference_location(r, file, *token)?;
                }
                self.stats.relationships += 1;
            }
        }
        Ok(())
    }
}

/// The innermost scope that contains `range`. Scopes are sorted by start
/// and properly nested, so it is the last one, starting at or before the
/// range, that contains it.
fn innermost(scopes: &[(Range, i32)], range: &Range) -> Option<i32> {
    let key = (range.start_line, range.start_column);
    let end = scopes.partition_point(|(s, _)| (s.start_line, s.start_column) <= key);
    scopes[..end]
        .iter()
        .rev()
        .find(|(s, _)| contains(s, range))
        .map(|(_, id)| *id)
}

pub fn convert(index: &Index, options: &Options) -> Result<Stats, String> {
    let global = Global::new(index, options);
    let root = match &options.project_root {
        Some(r) => r.clone(),
        None => uri_to_path(&index.metadata.project_root),
    };
    // root/relative with one kind of slash: SCIP paths always use `/`.
    let root = root.to_string_lossy().replace('\\', "/");
    let root = root.trim_end_matches('/');
    let join = |relative: &str| format!("{root}/{relative}");

    // Which documents, and under which path each one is recorded: the path
    // Sourcetrail passed, so its file node matches; otherwise root/relative.
    let mut selected: Vec<(&Document, String)> = Vec::new();
    let mut missing: Vec<String> = Vec::new();
    if options.sources.is_empty() {
        for d in &index.documents {
            selected.push((d, join(&d.relative_path)));
        }
    } else {
        let by_path: HashMap<String, &Document> = index
            .documents
            .iter()
            .map(|d| (normalize(&join(&d.relative_path)), d))
            .collect();
        for s in &options.sources {
            match by_path.get(&normalize(s)) {
                Some(d) => selected.push((d, s.clone())),
                None => missing.push(s.clone()),
            }
        }
    }

    let mut writer = Writer::open(&options.database)?;
    writer.begin()?;
    let mut batch = Batch {
        global: &global,
        writer,
        globals: HashMap::new(),
        locals: HashMap::new(),
        stats: Stats::default(),
    };
    for (doc, path) in &selected {
        let language = options.language.clone().unwrap_or_else(|| {
            if doc.language.is_empty() {
                global.scheme_language.clone()
            } else {
                doc.language.to_ascii_lowercase()
            }
        });
        batch.document(doc, path, &language)?;
    }
    for s in &missing {
        let file = batch.writer.file(s)?;
        let whole = Range {
            start_line: 1,
            start_column: 1,
            end_line: 1,
            end_column: 1,
        };
        batch
            .writer
            .error("the file is not in the SCIP index", true, file, whole)?;
        batch.stats.missing_sources += 1;
    }
    batch.writer.commit()?;
    Ok(batch.stats)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ranges() {
        // One line, end exclusive -> inclusive.
        assert_eq!(
            to_range(&[0, 4, 9]),
            Some(Range {
                start_line: 1,
                start_column: 5,
                end_line: 1,
                end_column: 9
            })
        );
        // Several lines.
        assert_eq!(
            to_range(&[2, 0, 5, 1]),
            Some(Range {
                start_line: 3,
                start_column: 1,
                end_line: 6,
                end_column: 1
            })
        );
        // Ends at the start of a line.
        assert_eq!(
            to_range(&[2, 0, 5, 0]),
            Some(Range {
                start_line: 3,
                start_column: 1,
                end_line: 6,
                end_column: 1
            })
        );
        // Zero width.
        assert_eq!(
            to_range(&[0, 4, 4]),
            Some(Range {
                start_line: 1,
                start_column: 5,
                end_line: 1,
                end_column: 5
            })
        );
        assert_eq!(to_range(&[1, 2]), None);
        assert_eq!(to_range(&[3, 0, 2, 0]), None);
    }

    #[test]
    fn uris() {
        assert_eq!(
            uri_to_path("file:///D:/src/my%20crate"),
            PathBuf::from("D:/src/my crate")
        );
        assert_eq!(
            uri_to_path("file:///home/me/x"),
            PathBuf::from("/home/me/x")
        );
    }

    #[test]
    fn innermost_scope() {
        let r = |a, b, c, d| Range {
            start_line: a,
            start_column: b,
            end_line: c,
            end_column: d,
        };
        let scopes = vec![(r(1, 1, 20, 1), 1), (r(2, 1, 5, 1), 2), (r(7, 1, 9, 1), 3)];
        assert_eq!(innermost(&scopes, &r(3, 5, 3, 8)), Some(2));
        assert_eq!(innermost(&scopes, &r(6, 1, 6, 4)), Some(1));
        assert_eq!(innermost(&scopes, &r(8, 2, 8, 3)), Some(3));
        assert_eq!(innermost(&scopes, &r(25, 1, 25, 2)), None);
    }
}

//! Converts a SCIP index (index.scip) into a Sourcetrail database.
//!
//! The conversion works on the whole index but writes only the documents it
//! is asked for, so Sourcetrail can run it in batches of source files like
//! any other custom-command indexer. Names, kinds and definition kinds come
//! from the whole index, so a symbol is the same node in every batch.

pub mod generate;
pub mod names;
pub mod text;
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
    /// Requested sources that are not in the index; each gets a non-fatal
    /// error.
    pub missing_sources: usize,
    pub skipped_occurrences: usize,
    /// Bodies found in the source text because the index had none.
    pub scopes_from_text: usize,
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

/// The form two paths are compared in. An existing file is resolved first,
/// so that a Windows 8.3 short name (`C:/Users/RUNNER~1/...`) or a symlink
/// matches the long path the indexer wrote.
pub fn comparable(path: &str) -> String {
    match std::fs::canonicalize(path) {
        Ok(p) => normalize(&p.to_string_lossy()),
        Err(_) => normalize(path),
    }
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
    /// Trait symbols by simple name, to resolve the trait of a
    /// rust-analyzer `impl#[T][Trait]` from its name alone.
    traits: HashMap<String, Vec<&'a str>>,
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
        let mut traits: HashMap<String, Vec<&str>> = HashMap::new();
        for (symbol, i) in &info {
            if i.kind.value() == scip::types::symbol_information::Kind::Trait.value() {
                if let Ok(parsed) = scip::symbol::parse_symbol(symbol) {
                    if let Some(last) = parsed.descriptors.last() {
                        traits.entry(last.name.clone()).or_default().push(symbol);
                    }
                }
            }
        }
        for v in traits.values_mut() {
            v.sort();
        }
        Global {
            info,
            defined,
            traits,
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
    /// rust-analyzer trait impl members: symbol -> (trait simple name, name).
    trait_members: HashMap<String, (String, Vec<writer::NameElement>)>,
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
            Some(Target::Global {
                name,
                kind,
                alias,
                trait_impl,
            }) => {
                let kind = match self.global.info.get(symbol) {
                    _ if alias => None,
                    Some(info) => names::kind_from_info(info, kind),
                    None => kind,
                };
                if let Some(t) = trait_impl {
                    self.trait_members
                        .insert(symbol.to_string(), (t, name.clone()));
                }
                let definition = if self.global.defined.contains(symbol) {
                    DefinitionKind::Explicit
                } else {
                    DefinitionKind::None
                };
                let id = self
                    .writer
                    .symbol(&self.global.delimiter, &name, kind, definition)?;
                // An alias records no kind, but a reference to it is still a
                // type use.
                let kind = if alias { Some(SymbolKind::Type) } else { kind };
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
        // When the index has no bodies at all (scip-dotnet), find them in the
        // text of brace languages; the text also gives the kinds of types.
        let index_has_scopes = doc
            .occurrences
            .iter()
            .any(|o| has_role(o, SymbolRole::Definition) && enclosing_range(o).is_some());
        let code = if !index_has_scopes && text::is_brace_language(&doc.language) {
            let source = if doc.text.is_empty() {
                std::fs::read_to_string(path).ok()
            } else {
                Some(doc.text.clone())
            };
            source.map(|s| text::Code::new(&s))
        } else {
            None
        };
        let mut scopes: Vec<(Range, i32)> = Vec::new();
        // Scopes that are modules, namespaces or packages.
        let mut module_scopes: HashSet<i32> = HashSet::new();
        let mut definition_token: HashMap<&str, Range> = HashMap::new();
        let mut own_scope: HashMap<*const Occurrence, Range> = HashMap::new();
        for o in &doc.occurrences {
            if !has_role(o, SymbolRole::Definition) {
                continue;
            }
            let Some(token) = occurrence_range(o) else {
                continue;
            };
            definition_token.entry(o.symbol.as_str()).or_insert(token);
            let Some(Resolved::Global(id, kind)) = self.resolve(&o.symbol, rel)? else {
                continue;
            };
            if let (Some(code), Some(SymbolKind::Type)) = (&code, kind) {
                if let Some(k) = code.type_keyword(&token) {
                    self.refine_kind(&o.symbol, rel, k)?;
                }
            }
            let scope = match (enclosing_range(o), &code) {
                (Some(s), _) => Some(s),
                (None, Some(code)) => {
                    let s = code.scope(&token);
                    if s.is_some() {
                        self.stats.scopes_from_text += 1;
                    }
                    s
                }
                (None, None) => None,
            };
            if let Some(scope) = scope {
                own_scope.insert(o as *const Occurrence, scope);
                if matches!(
                    kind,
                    Some(SymbolKind::Module | SymbolKind::Namespace | SymbolKind::Package)
                ) {
                    module_scopes.insert(id);
                }
                scopes.push((scope, id));
            }
        }
        scopes.sort_by(|a, b| {
            (a.0.start_line, a.0.start_column)
                .cmp(&(b.0.start_line, b.0.start_column))
                .then((b.0.end_line, b.0.end_column).cmp(&(a.0.end_line, a.0.end_column)))
        });

        // rust-analyzer reports an operator such as ` + ` as one occurrence
        // per character; adjacent references to the same symbol on a line
        // are one reference.
        let mut merged: Vec<(Range, &Occurrence)> = Vec::new();
        for o in &doc.occurrences {
            let Some(range) = occurrence_range(o) else {
                self.stats.skipped_occurrences += 1;
                continue;
            };
            if let Some((last, prev)) = merged.last_mut() {
                if !has_role(o, SymbolRole::Definition)
                    && o.symbol_roles == prev.symbol_roles
                    && o.symbol == prev.symbol
                    && o.diagnostics.is_empty()
                    && range.start_line == range.end_line
                    && last.end_line == range.start_line
                    && last.end_column + 1 == range.start_column
                {
                    last.end_column = range.end_column;
                    continue;
                }
            }
            merged.push((range, o));
        }

        for (range, o) in merged {
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
                    if let Some(scope) = own_scope.get(&(o as *const Occurrence)) {
                        self.writer.symbol_scope_location(id, file, *scope)?;
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
                    let mut reference_kind = if has_role(o, SymbolRole::Import) {
                        ReferenceKind::Import
                    } else {
                        match kind {
                            Some(SymbolKind::Macro) => ReferenceKind::MacroUsage,
                            Some(k) if k.is_callable() => ReferenceKind::Call,
                            Some(k) if k.is_type() => ReferenceKind::TypeUsage,
                            _ => ReferenceKind::Usage,
                        }
                    };
                    // rust-analyzer gives `use` items no Import role. A Rust
                    // function cannot be called at module level (const and
                    // static initializers have bodies of their own), so a
                    // "call" from a module or the file is a `use`.
                    if reference_kind == ReferenceKind::Call
                        && self.global.scheme_language == "rust"
                        && context.map_or(true, |c| module_scopes.contains(&c))
                    {
                        reference_kind = ReferenceKind::Import;
                    }
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

        // rust-analyzer gives no relationships for `impl Trait for T`; they
        // follow from the names of the impl members.
        for o in &doc.occurrences {
            if has_role(o, SymbolRole::Definition) {
                self.trait_impl(&o.symbol, rel, file, &definition_token)?;
            }
        }
        Ok(())
    }

    /// Records a better kind for a symbol whose kind was only guessed. A
    /// node keeps the highest kind any batch gives it, so batches without
    /// this file (and its text) do not undo it.
    fn refine_kind(&mut self, symbol: &str, rel: &str, kind: SymbolKind) -> Result<(), String> {
        let Some(Target::Global { name, .. }) = names::parse(symbol, rel) else {
            return Ok(());
        };
        let definition = if self.global.defined.contains(symbol) {
            DefinitionKind::Explicit
        } else {
            DefinitionKind::None
        };
        let id = self
            .writer
            .symbol(&self.global.delimiter, &name, Some(kind), definition)?;
        self.globals
            .insert(symbol.to_string(), Some((id, Some(kind))));
        Ok(())
    }

    /// For a member of `impl Trait for T`: T inherits from Trait, and the
    /// member overrides Trait's member of the same name, when the index
    /// has exactly one trait with that name.
    fn trait_impl(
        &mut self,
        symbol: &str,
        rel: &str,
        file: i32,
        tokens: &HashMap<&str, Range>,
    ) -> Result<(), String> {
        let Some(Resolved::Global(member, kind)) = self.resolve(symbol, rel)? else {
            return Ok(());
        };
        let Some((trait_name, name)) = self.trait_members.get(symbol).cloned() else {
            return Ok(());
        };
        let Some([trait_symbol]) = self.global.traits.get(&trait_name).map(|v| v.as_slice()) else {
            return Ok(());
        };
        let trait_symbol = trait_symbol.to_string();
        let token = tokens.get(symbol).copied();
        if let Some(Resolved::Global(trait_id, _)) = self.resolve(&trait_symbol, rel)? {
            let self_type = self.writer.symbol(
                &self.global.delimiter,
                &name[..name.len() - 1],
                None,
                DefinitionKind::None,
            )?;
            let r = self
                .writer
                .reference(self_type, trait_id, ReferenceKind::Inheritance)?;
            if let Some(t) = token {
                self.writer.reference_location(r, file, t)?;
            }
            self.stats.relationships += 1;
        }
        if kind.is_some_and(|k| k.is_callable()) {
            let method = name.last().map(|e| e.name.clone()).unwrap_or_default();
            let overridden = format!("{trait_symbol}{method}().");
            if self.global.info.contains_key(overridden.as_str()) {
                if let Some(Resolved::Global(target, _)) = self.resolve(&overridden, rel)? {
                    let r = self
                        .writer
                        .reference(member, target, ReferenceKind::Override)?;
                    if let Some(t) = token {
                        self.writer.reference_location(r, file, t)?;
                    }
                    self.stats.relationships += 1;
                }
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
    // root/relative with one kind of slash. rust-analyzer on Windows writes
    // both with backslashes.
    let root = root.to_string_lossy().replace('\\', "/");
    let root = root.trim_end_matches('/');
    let join = |relative: &str| format!("{root}/{}", relative.replace('\\', "/"));

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
            .map(|d| (comparable(&join(&d.relative_path)), d))
            .collect();
        for s in &options.sources {
            match by_path.get(&comparable(s)) {
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
        trait_members: HashMap::new(),
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
        // Not fatal: rust-analyzer only indexes files reachable from a crate
        // root, and a stray .rs file should not mark the project as broken.
        batch.writer.error(
            "the file is not in the SCIP index (not part of the project?)",
            false,
            file,
            whole,
        )?;
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
    fn comparable_paths_resolve_existing_files() {
        let dir = std::env::temp_dir().join("scip2sourcetrail-comparable");
        std::fs::create_dir_all(dir.join("sub")).unwrap();
        std::fs::write(dir.join("f.cs"), "").unwrap();
        let d = dir.to_string_lossy().replace('\\', "/");
        // `..` and other spellings of an existing file meet; a missing file
        // still compares by its normalized text.
        assert_eq!(
            comparable(&format!("{d}/sub/../f.cs")),
            comparable(&format!("{d}/f.cs"))
        );
        assert_eq!(
            comparable("/no/such/./file.rs"),
            normalize("/no/such/file.rs")
        );
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

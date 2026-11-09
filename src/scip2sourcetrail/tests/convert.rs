//! End-to-end tests: build a SCIP index in memory, convert it, and read the
//! Sourcetrail database back with SQLite.

use protobuf::{Enum, EnumOrUnknown, MessageField};
use rusqlite::Connection;
use scip::types::symbol_information::Kind;
use scip::types::{
    Document, Index, Metadata, Occurrence, Relationship, SymbolInformation, SymbolRole, ToolInfo,
};
use scip2sourcetrail::{convert, Options};
use std::path::PathBuf;

const DEF: i32 = 1; // SymbolRole::Definition
const IMPORT: i32 = 2; // SymbolRole::Import

fn occ(range: &[i32], symbol: &str, roles: i32, enclosing: &[i32]) -> Occurrence {
    Occurrence {
        range: range.to_vec(),
        symbol: symbol.to_string(),
        symbol_roles: roles,
        enclosing_range: enclosing.to_vec(),
        ..Default::default()
    }
}

fn info(symbol: &str, kind: Kind, implements: &[&str]) -> SymbolInformation {
    SymbolInformation {
        symbol: symbol.to_string(),
        kind: EnumOrUnknown::new(kind),
        relationships: implements
            .iter()
            .map(|s| Relationship {
                symbol: s.to_string(),
                is_implementation: true,
                ..Default::default()
            })
            .collect(),
        ..Default::default()
    }
}

fn doc(
    path: &str,
    language: &str,
    occurrences: Vec<Occurrence>,
    symbols: Vec<SymbolInformation>,
) -> Document {
    Document {
        relative_path: path.to_string(),
        language: language.to_string(),
        occurrences,
        symbols,
        ..Default::default()
    }
}

fn index(tool: &str, root: &str, documents: Vec<Document>) -> Index {
    Index {
        metadata: MessageField::some(Metadata {
            tool_info: MessageField::some(ToolInfo {
                name: tool.to_string(),
                ..Default::default()
            }),
            project_root: root.to_string(),
            ..Default::default()
        }),
        documents,
        ..Default::default()
    }
}

const R: &str = "rust-analyzer cargo demo 0.1.0 ";

/// A small Rust crate:
///
/// ```text
/// src/shapes.rs
///  1 pub trait Area { fn area(&self) -> f64; }
///  2 pub struct Square { pub side: f64 }
///  3 impl Area for Square {
///  4     fn area(&self) -> f64 { let s = self.side; s * s }
///  5 }
/// src/main.rs
///  1 use crate::shapes::Square;
///  2 fn main() {
///  3     let q = Square { side: 2.0 };
///  4     println!("{}", total(&q));
///  5 }
///  6 fn total(q: &Square) -> f64 { q.area() + std::f64::consts::PI }
/// ```
fn rust_index() -> Index {
    let area_trait = format!("{R}shapes/Area#");
    let area_trait_m = format!("{R}shapes/Area#area().");
    let square = format!("{R}shapes/Square#");
    let side = format!("{R}shapes/Square#side.");
    let area_impl = format!("{R}shapes/impl#[Square][Area]area().");
    let main = format!("{R}main().");
    let total = format!("{R}total().");
    let q_param = format!("{R}total().(q)");
    let pi = "rust-analyzer cargo std 1.97.1 f64/consts/PI.".to_string();
    let println = "rust-analyzer cargo std 1.97.1 macros/println!".to_string();

    let shapes = doc(
        "src/shapes.rs",
        "rust",
        vec![
            occ(&[0, 10, 14], &area_trait, DEF, &[0, 0, 0, 37]),
            occ(&[0, 20, 24], &area_trait_m, DEF, &[0, 17, 35]),
            occ(&[1, 11, 17], &square, DEF, &[1, 0, 1, 34]),
            occ(&[1, 24, 28], &side, DEF, &[1, 20, 32]),
            occ(&[2, 5, 9], &area_trait, 0, &[]),
            occ(&[2, 14, 20], &square, 0, &[]),
            occ(&[3, 7, 11], &area_impl, DEF, &[3, 4, 3, 51]),
            occ(&[3, 30, 31], "local 0", DEF, &[]),
            occ(&[3, 39, 43], &side, 0, &[]),
            occ(&[3, 45, 46], "local 0", 0, &[]),
        ],
        vec![
            info(&area_trait, Kind::Trait, &[]),
            info(&area_trait_m, Kind::TraitMethod, &[]),
            info(&square, Kind::Struct, &[&area_trait]),
            info(&side, Kind::Field, &[]),
            info(&area_impl, Kind::Method, &[&area_trait_m]),
        ],
    );
    let main_rs = doc(
        "src/main.rs",
        "rust",
        vec![
            occ(&[0, 20, 26], &square, IMPORT, &[]),
            occ(&[1, 3, 7], &main, DEF, &[1, 0, 4, 1]),
            occ(&[2, 8, 9], "local 0", DEF, &[]),
            occ(&[2, 12, 18], &square, 0, &[]),
            occ(&[2, 21, 25], &side, 0, &[]),
            occ(&[3, 4, 11], &println, 0, &[]),
            occ(&[3, 19, 24], &total, 0, &[]),
            occ(&[5, 3, 8], &total, DEF, &[5, 0, 64]),
            occ(&[5, 9, 10], &q_param, DEF, &[]),
            occ(&[5, 13, 19], &square, 0, &[]),
            occ(&[5, 32, 36], &area_trait_m, 0, &[]),
            occ(&[5, 59, 61], &pi, 0, &[]),
        ],
        vec![
            info(&main, Kind::Function, &[]),
            info(&total, Kind::Function, &[]),
        ],
    );
    index("rust-analyzer", "file:///proj/demo", vec![shapes, main_rs])
}

struct Db {
    conn: Connection,
}

impl Db {
    fn open(path: &PathBuf) -> Db {
        Db {
            conn: Connection::open(path).unwrap(),
        }
    }

    fn rows(&self, sql: &str) -> Vec<String> {
        let mut stmt = self.conn.prepare(sql).unwrap();
        let n = stmt.column_count();
        stmt.query_map([], |row| {
            Ok((0..n)
                .map(|i| match row.get_ref(i).unwrap() {
                    rusqlite::types::ValueRef::Null => "NULL".to_string(),
                    rusqlite::types::ValueRef::Integer(v) => v.to_string(),
                    rusqlite::types::ValueRef::Text(t) => String::from_utf8_lossy(t).into_owned(),
                    other => format!("{other:?}"),
                })
                .collect::<Vec<_>>()
                .join(" | "))
        })
        .unwrap()
        .map(|r| r.unwrap())
        .collect()
    }

    fn count(&self, sql: &str) -> i64 {
        self.conn.query_row(sql, [], |r| r.get(0)).unwrap()
    }
}

/// The qualified name of a node, as srctrl prints it.
const NAME: &str =
    "replace(replace(replace(substr(serialized_name, instr(serialized_name, char(9)||'m') + 2), \
    char(9)||'s'||char(9)||'p', ''), char(9)||'s', ''), char(9)||'n', \
    substr(serialized_name, 1, instr(serialized_name, char(9)||'m') - 1))";

fn edges(db: &Db, kind: i32) -> Vec<String> {
    db.rows(&format!(
        "SELECT replace(a.n, char(9)||'p', ''), replace(b.n, char(9)||'p', '') FROM edge e \
         JOIN (SELECT id, {NAME} AS n FROM node) a ON a.id = e.source_node_id \
         JOIN (SELECT id, {NAME} AS n FROM node) b ON b.id = e.target_node_id \
         WHERE e.type = {kind} ORDER BY 1, 2"
    ))
}

fn scratch(name: &str) -> PathBuf {
    let dir = std::env::temp_dir().join("scip2sourcetrail-tests");
    std::fs::create_dir_all(&dir).unwrap();
    let p = dir.join(name);
    let _ = std::fs::remove_file(&p);
    p
}

fn opts(db: &std::path::Path) -> Options {
    Options {
        database: db.to_path_buf(),
        ..Default::default()
    }
}

#[test]
fn rust_crate_whole_index() {
    let db_path = scratch("rust.srctrldb");
    let stats = convert(&rust_index(), &opts(&db_path)).unwrap();
    assert_eq!(stats.documents, 2);
    assert_eq!(stats.missing_sources, 0);
    let db = Db::open(&db_path);

    // Names use the Rust delimiter and start at the crate.
    let names = db.rows(&format!(
        "SELECT replace({NAME}, char(9)||'p', '') FROM node WHERE type <> 262144 ORDER BY 1"
    ));
    for n in [
        "demo::shapes::Square::side",
        "demo::shapes::Area::area()",
        "demo::total()",
        "std::f64::consts::PI",
    ] {
        assert!(names.contains(&n.to_string()), "{n} missing from {names:?}");
    }

    // Calls: main -> total, total -> Area::area. println! is a macro use.
    let calls = edges(&db, 8);
    assert!(
        calls.contains(&"demo::main() | demo::total()".to_string()),
        "{calls:?}"
    );
    assert!(
        calls.contains(&"demo::total() | demo::shapes::Area::area()".to_string()),
        "{calls:?}"
    );
    let macros = edges(&db, 2048);
    assert_eq!(macros, ["demo::main() | std::macros::println"]);

    // Field use, type use, import, inheritance and override.
    let usages = edges(&db, 4);
    assert!(
        usages.contains(&"demo::main() | demo::shapes::Square::side".to_string()),
        "{usages:?}"
    );
    assert!(
        usages
            .iter()
            .any(|u| u.ends_with("| demo::shapes::Square::side") && u.contains("area")),
        "{usages:?}"
    );
    assert!(edges(&db, 2).contains(&"demo::total() | demo::shapes::Square".to_string()));
    assert_eq!(edges(&db, 512).len(), 1);
    assert_eq!(
        edges(&db, 16),
        ["demo::shapes::Square | demo::shapes::Area"]
    );
    assert_eq!(edges(&db, 32).len(), 1);

    // std symbols are only referenced: no definition kind, so non-indexed.
    assert_eq!(
        db.count(&format!(
            "SELECT count(*) FROM node n JOIN symbol s ON s.id = n.id WHERE {NAME} LIKE 'std::%'"
        )),
        0
    );
    // Crate symbols are explicit (2).
    assert_eq!(db.count(&format!(
        "SELECT definition_kind FROM node n JOIN symbol s ON s.id = n.id WHERE replace({NAME}, char(9)||'p', '') = 'demo::total()'")), 2);

    // Kinds from SymbolInformation: Square is a struct (64), Area an interface (256).
    assert_eq!(
        db.count(&format!(
            "SELECT type FROM node WHERE {NAME} = 'demo::shapes::Square'"
        )),
        64
    );
    assert_eq!(
        db.count(&format!(
            "SELECT type FROM node WHERE {NAME} = 'demo::shapes::Area'"
        )),
        256
    );

    // The trait impl method is a method (8192), not a free function, even
    // though type parameters sit between it and its type.
    assert_eq!(db.count(&format!(
        "SELECT type FROM node WHERE replace({NAME}, char(9)||'p', '') LIKE 'demo::shapes::impl::%area()'")), 8192);
    // root/relative with forward slashes.
    assert_eq!(
        db.rows("SELECT path FROM file ORDER BY path"),
        ["/proj/demo/src/main.rs", "/proj/demo/src/shapes.rs"]
    );

    // Locals: `local 0` in each file is a different local; the parameter q too.
    assert_eq!(db.count("SELECT count(*) FROM local_symbol"), 3);

    // Scope of total(): line 6, the whole body.
    assert_eq!(db.rows(&format!(
        "SELECT l.start_line, l.start_column, l.end_line, l.end_column FROM node n \
         JOIN occurrence o ON o.element_id = n.id JOIN source_location l ON l.id = o.source_location_id \
         WHERE replace({NAME}, char(9)||'p', '') = 'demo::total()' AND l.type = 1")), ["6 | 1 | 6 | 64"]);

    // Outside any definition: the import and the two names on the `impl` line.
    assert_eq!(stats.references_without_context, 3);
    assert_eq!(db.count("SELECT count(*) FROM error"), 0);
}

#[test]
fn batches_write_only_their_files() {
    let index = rust_index();
    let db_path = scratch("rust-batch.srctrldb");
    let mut o = opts(&db_path);
    // Sourcetrail passes absolute paths, possibly with other slashes.
    o.sources = vec![
        "/proj/demo/src/main.rs".to_string(),
        "/proj/demo/src/gone.rs".to_string(),
    ];
    let stats = convert(&index, &o).unwrap();
    assert_eq!(stats.documents, 1);
    assert_eq!(stats.missing_sources, 1);
    let db = Db::open(&db_path);
    assert_eq!(
        db.rows("SELECT path FROM file ORDER BY path"),
        ["/proj/demo/src/gone.rs", "/proj/demo/src/main.rs"]
    );
    // Square is defined in the other file, but it is still explicit here.
    assert_eq!(db.count(&format!(
        "SELECT definition_kind FROM node n JOIN symbol s ON s.id = n.id WHERE {NAME} = 'demo::shapes::Square'")), 2);
    // The missing file gets a fatal error.
    assert_eq!(db.count("SELECT count(*) FROM error WHERE fatal = 1"), 1);
}

#[test]
fn csharp_overloads_are_two_symbols() {
    let a = "scip-dotnet nuget . . App/Shape#Scale().";
    let b = "scip-dotnet nuget . . App/Shape#Scale(+1).";
    let caller = "scip-dotnet nuget . . App/Program#Main().";
    let d = doc(
        "Program.cs",
        "CSharp",
        vec![
            occ(&[0, 16, 21], a, DEF, &[0, 0, 40]),
            occ(&[1, 16, 21], b, DEF, &[1, 0, 40]),
            occ(&[3, 16, 20], caller, DEF, &[3, 0, 6, 1]),
            occ(&[4, 8, 13], a, 0, &[]),
            occ(&[5, 8, 13], b, 0, &[]),
        ],
        vec![
            info(a, Kind::Method, &[]),
            info(b, Kind::Method, &[]),
            info(caller, Kind::StaticMethod, &[]),
        ],
    );
    let db_path = scratch("csharp.srctrldb");
    convert(
        &index("scip-dotnet", "file:///proj/app", vec![d]),
        &opts(&db_path),
    )
    .unwrap();
    let db = Db::open(&db_path);
    assert_eq!(
        edges(&db, 8),
        [
            "App.Program.Main() | App.Shape.Scale()",
            "App.Program.Main() | App.Shape.Scale(+1)"
        ]
    );
    assert_eq!(db.rows("SELECT language FROM file"), ["csharp"]);
}

#[test]
fn reads_an_index_file() {
    let scip_path = scratch("rust.scip");
    scip::write_message_to_file(&scip_path, rust_index()).unwrap();
    let db_path = scratch("rust-file.srctrldb");
    let mut o = opts(&db_path);
    o.scip = scip_path;
    // A Windows-style path for a file of the index still matches.
    o.sources = vec![r"\proj\demo\src\shapes.rs".to_string()];
    let stats = scip2sourcetrail::run(&o).unwrap();
    assert_eq!((stats.documents, stats.missing_sources), (1, 0));
}

#[test]
fn bad_input_fails_cleanly() {
    let db_path = scratch("bad.srctrldb");
    let mut o = opts(&db_path);
    o.scip = scratch("not-an-index.scip");
    std::fs::write(&o.scip, b"\xff\xff\xff not protobuf").unwrap();
    assert!(scip2sourcetrail::run(&o).is_err());
    o.scip = scratch("does-not-exist.scip");
    assert!(scip2sourcetrail::run(&o)
        .unwrap_err()
        .contains("cannot read"));
}

#[allow(dead_code)]
fn _roles_match_scip() {
    assert_eq!(SymbolRole::Definition.value(), DEF);
    assert_eq!(SymbolRole::Import.value(), IMPORT);
}

#[test]
fn role_constants() {
    _roles_match_scip();
}

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
const CORE: &str = "rust-analyzer cargo core https://github.com/rust-lang/rust/library/core ";
const STD: &str = "rust-analyzer cargo std https://github.com/rust-lang/rust/library/std ";

/// A small Rust crate, with symbols, roles and ranges shaped like the output
/// of rust-analyzer 1.97 (`rust-analyzer scip`):
/// - a `crate/` root and modules whose definitions enclose their whole file;
/// - `impl#[T]` and `impl#[T][Trait]` members, and the `impl#[T]` alias
///   for `Self`;
/// - one occurrence per character of an operator;
/// - no relationships;
/// - backslashes in relative paths.
///
/// ```text
/// src/shapes.rs
///  1 pub trait Area { fn area(&self) -> f64; }
///  2 pub struct Square { pub side: f64 }
///  3 impl Square { pub fn new(side: f64) -> Self { Square { side } } }
///  4 impl Area for Square {
///  5     fn area(&self) -> f64 { let s = self.side; s * s }
///  6 }
/// src/main.rs
///  1 mod shapes;
///  2 use crate::shapes::{Area, Square};
///  3 fn main() {
///  4     let q = Square::new(2.0);
///  5     println!("{}", total(&q));
///  6 }
///  7 fn total(q: &Square) -> f64 { q.area() + std::f64::consts::PI }
/// ```
fn rust_index() -> Index {
    let krate = format!("{R}crate/");
    let shapes_mod = format!("{R}shapes/");
    let area_trait = format!("{R}shapes/Area#");
    let area_trait_m = format!("{R}shapes/Area#area().");
    let square = format!("{R}shapes/Square#");
    let side = format!("{R}shapes/Square#side.");
    let new = format!("{R}shapes/impl#[Square]new().");
    let self_alias = format!("{R}shapes/impl#[Square]");
    let area_impl = format!("{R}shapes/impl#[Square][Area]area().");
    let main = format!("{R}main().");
    let total = format!("{R}total().");
    let add = format!("{CORE}ops/arith/impl#[f64][`Add<Self>`]add().");
    let mul = format!("{CORE}ops/arith/impl#[f64][`Mul<Self>`]mul().");
    let pi = format!("{CORE}f64/consts/PI.");
    let println = format!("{STD}macros/println!");

    let shapes = doc(
        r"src\shapes.rs",
        "rust",
        vec![
            occ(&[0, 0, 6, 0], &shapes_mod, DEF, &[0, 0, 6, 0]),
            occ(&[0, 10, 14], &area_trait, DEF, &[0, 0, 0, 41]),
            occ(&[0, 20, 24], &area_trait_m, DEF, &[0, 17, 39]),
            occ(&[0, 26, 30], "local 0", DEF, &[0, 25, 30]),
            occ(&[1, 11, 17], &square, DEF, &[1, 0, 1, 34]),
            occ(&[1, 24, 28], &side, DEF, &[1, 20, 32]),
            occ(&[2, 5, 11], &square, 0, &[1, 0, 1, 34]),
            occ(&[2, 21, 24], &new, DEF, &[2, 14, 63]),
            occ(&[2, 25, 29], "local 1", DEF, &[2, 25, 29]),
            occ(&[2, 40, 44], &self_alias, 0, &[2, 0, 65]),
            occ(&[2, 47, 53], &square, 0, &[1, 0, 1, 34]),
            occ(&[2, 56, 60], "local 1", 0, &[2, 25, 29]),
            occ(&[2, 56, 60], &side, 0, &[1, 20, 32]),
            occ(&[3, 5, 9], &area_trait, 0, &[0, 0, 0, 41]),
            occ(&[3, 14, 20], &square, 0, &[1, 0, 1, 34]),
            occ(&[4, 7, 11], &area_impl, DEF, &[4, 4, 4, 55]),
            occ(&[4, 13, 17], "local 2", DEF, &[4, 12, 17]),
            occ(&[4, 32, 33], "local 3", DEF, &[4, 32, 33]),
            occ(&[4, 41, 45], &side, 0, &[1, 20, 32]),
            occ(&[4, 47, 48], "local 3", 0, &[4, 32, 33]),
            occ(&[4, 48, 49], &mul, 0, &[]),
            occ(&[4, 49, 50], &mul, 0, &[]),
            occ(&[4, 50, 51], &mul, 0, &[]),
            occ(&[4, 51, 52], "local 3", 0, &[4, 32, 33]),
        ],
        vec![
            info(&shapes_mod, Kind::Module, &[]),
            info(&area_trait, Kind::Trait, &[]),
            info(&area_trait_m, Kind::TraitMethod, &[]),
            info(&square, Kind::Struct, &[]),
            info(&side, Kind::Field, &[]),
            info(&new, Kind::StaticMethod, &[]),
            info(&self_alias, Kind::TypeAlias, &[]),
            info(&area_impl, Kind::Method, &[]),
        ],
    );
    let main_rs = doc(
        r"src\main.rs",
        "rust",
        vec![
            occ(&[0, 0, 7, 0], &krate, DEF, &[0, 0, 7, 0]),
            occ(&[0, 4, 10], &shapes_mod, 0, &[]),
            occ(&[1, 4, 9], &krate, 0, &[0, 0, 7, 0]),
            occ(&[1, 11, 17], &shapes_mod, 0, &[]),
            occ(&[1, 20, 24], &area_trait, 0, &[]),
            occ(&[1, 26, 32], &square, 0, &[]),
            occ(&[2, 3, 7], &main, DEF, &[2, 0, 5, 1]),
            occ(&[3, 8, 9], "local 0", DEF, &[3, 8, 9]),
            occ(&[3, 12, 18], &square, 0, &[]),
            occ(&[3, 20, 23], &new, 0, &[]),
            occ(&[4, 4, 11], &println, 0, &[]),
            occ(&[4, 19, 24], &total, 0, &[6, 0, 64]),
            occ(&[4, 26, 27], "local 0", 0, &[3, 8, 9]),
            occ(&[6, 3, 8], &total, DEF, &[6, 0, 64]),
            occ(&[6, 9, 10], "local 1", DEF, &[6, 9, 10]),
            occ(&[6, 13, 19], &square, 0, &[]),
            occ(&[6, 30, 31], "local 1", 0, &[6, 9, 10]),
            occ(&[6, 32, 36], &area_impl, 0, &[]),
            occ(&[6, 38, 39], &add, 0, &[]),
            occ(&[6, 39, 40], &add, 0, &[]),
            occ(&[6, 40, 41], &add, 0, &[]),
            occ(&[6, 59, 61], &pi, 0, &[]),
        ],
        vec![
            info(&krate, Kind::Module, &[]),
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

/// The qualified name of a node with the postfix of its last level, e.g.
/// `demo::shapes::Square::area() (Area)`. Prefixes are always empty here.
const NAME: &str = "replace(replace(replace(replace(substr(serialized_name, \
    instr(serialized_name, char(9)||'m') + 2), char(9)||'s'||char(9)||'p'||char(9)||'n', char(9)||'n'), \
    char(9)||'s'||char(9)||'p', ''), char(9)||'s', ''), char(9)||'n', \
    substr(serialized_name, 1, instr(serialized_name, char(9)||'m') - 1))";

fn edges(db: &Db, kind: i32) -> Vec<String> {
    db.rows(&format!(
        "SELECT a.n || ' | ' || b.n FROM edge e \
         JOIN (SELECT id, {NAME} AS n FROM node) a ON a.id = e.source_node_id \
         JOIN (SELECT id, {NAME} AS n FROM node) b ON b.id = e.target_node_id \
         WHERE e.type = {kind} ORDER BY 1"
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

    // Names use the Rust delimiter and start at the crate; `crate/` is the
    // crate itself, and impl members hang from their type.
    let names = db.rows(&format!(
        "SELECT {NAME} FROM node WHERE type <> 262144 ORDER BY 1"
    ));
    for n in [
        "demo",
        "demo::shapes::Square::side",
        "demo::shapes::Square::new()",
        "demo::shapes::Square::area() (Area)",
        "demo::shapes::Area::area()",
        "demo::total()",
        "core::f64::consts::PI",
    ] {
        assert!(names.contains(&n.to_string()), "{n} missing from {names:?}");
    }
    assert!(
        !names
            .iter()
            .any(|n| n.contains("impl") || n.contains("crate")),
        "{names:?}"
    );

    // Calls: main -> new and total; total -> Square::area and one `+`.
    assert_eq!(
        edges(&db, 8),
        [
            "demo::main() | demo::shapes::Square::new()",
            "demo::main() | demo::total()",
            "demo::shapes::Square::area() (Area) | core::ops::arith::f64::mul() (Mul)",
            "demo::total() | core::ops::arith::f64::add() (Add)",
            "demo::total() | demo::shapes::Square::area() (Area)",
        ]
    );
    // The three characters of ` + ` are one call site, columns 39 to 41.
    assert_eq!(
        db.rows(
            "SELECT l.start_column, l.end_column FROM edge e \
             JOIN node t ON t.id = e.target_node_id JOIN occurrence o ON o.element_id = e.id \
             JOIN source_location l ON l.id = o.source_location_id \
             WHERE t.serialized_name LIKE '%add%'"
        ),
        ["39 | 41"]
    );
    assert_eq!(edges(&db, 2048), ["demo::main() | std::macros::println"]);

    // Field uses; `Self` (the impl#[Square] alias) is a type use of Square.
    let usages = edges(&db, 4);
    for u in [
        "demo::shapes::Square::new() | demo::shapes::Square::side",
        "demo::shapes::Square::area() (Area) | demo::shapes::Square::side",
    ] {
        assert!(
            usages.contains(&u.to_string()),
            "{u} missing from {usages:?}"
        );
    }
    let type_uses = edges(&db, 2);
    for u in [
        "demo::shapes::Square::new() | demo::shapes::Square",
        "demo | demo::shapes::Square",
        "demo::shapes | demo::shapes::Square",
    ] {
        assert!(
            type_uses.contains(&u.to_string()),
            "{u} missing from {type_uses:?}"
        );
    }

    // No relationships in the index: inheritance and override follow from
    // impl#[Square][Area]area().
    assert_eq!(
        edges(&db, 16),
        ["demo::shapes::Square | demo::shapes::Area"]
    );
    assert_eq!(
        edges(&db, 32),
        ["demo::shapes::Square::area() (Area) | demo::shapes::Area::area()"]
    );

    // std and core are only referenced: no definition kind, so non-indexed.
    assert_eq!(
        db.count(&format!(
            "SELECT count(*) FROM node n JOIN symbol s ON s.id = n.id \
             WHERE {NAME} LIKE 'std::%' OR {NAME} LIKE 'core::%'"
        )),
        0
    );
    assert_eq!(
        db.count(&format!(
            "SELECT definition_kind FROM node n JOIN symbol s ON s.id = n.id \
             WHERE {NAME} = 'demo::total()'"
        )),
        2
    );

    // Kinds: Square a struct (64) despite the TypeAlias of `Self`, Area an
    // interface (256), the impl members methods (8192), demo a module (8).
    let kind = |n: &str| db.count(&format!("SELECT type FROM node WHERE {NAME} = '{n}'"));
    assert_eq!(kind("demo::shapes::Square"), 64);
    assert_eq!(kind("demo::shapes::Area"), 256);
    assert_eq!(kind("demo::shapes::Square::area() (Area)"), 8192);
    assert_eq!(kind("demo::shapes::Square::new()"), 8192);
    assert_eq!(kind("demo"), 8);

    // Paths with forward slashes, though the index uses backslashes.
    assert_eq!(
        db.rows("SELECT path FROM file ORDER BY path"),
        ["/proj/demo/src/main.rs", "/proj/demo/src/shapes.rs"]
    );

    // `local N` is per file: 4 in shapes.rs, 2 in main.rs.
    assert_eq!(db.count("SELECT count(*) FROM local_symbol"), 6);

    // Scope of total(): line 7, the whole body.
    assert_eq!(
        db.rows(&format!(
            "SELECT l.start_line, l.start_column, l.end_line, l.end_column FROM node n \
             JOIN occurrence o ON o.element_id = n.id \
             JOIN source_location l ON l.id = o.source_location_id \
             WHERE {NAME} = 'demo::total()' AND l.type = 1"
        )),
        ["7 | 1 | 7 | 64"]
    );

    // Modules enclose their file, so every reference has a context.
    assert_eq!(stats.references_without_context, 0);
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
    assert_eq!(
        db.count(&format!(
            "SELECT definition_kind FROM node n JOIN symbol s ON s.id = n.id \
             WHERE {NAME} = 'demo::shapes::Square'"
        )),
        2
    );
    // The missing file gets a fatal error.
    assert_eq!(db.count("SELECT count(*) FROM error WHERE fatal = 1"), 1);
}

#[test]
fn csharp_overloads_and_relationships() {
    let shape = "scip-dotnet nuget . . App/IShape#";
    let shape_area = "scip-dotnet nuget . . App/IShape#Area().";
    let square = "scip-dotnet nuget . . App/Square#";
    let square_area = "scip-dotnet nuget . . App/Square#Area().";
    let a = "scip-dotnet nuget . . App/Square#Scale().";
    let b = "scip-dotnet nuget . . App/Square#Scale(+1).";
    let caller = "scip-dotnet nuget . . App/Program#Main().";
    let using = "scip-dotnet nuget . . System/";
    let d = doc(
        "Program.cs",
        "CSharp",
        vec![
            occ(&[0, 6, 12], using, IMPORT, &[]),
            occ(&[1, 17, 23], shape, DEF, &[1, 0, 1, 40]),
            occ(&[1, 30, 34], shape_area, DEF, &[1, 25, 38]),
            occ(&[2, 13, 19], square, DEF, &[2, 0, 9, 1]),
            occ(&[2, 22, 28], shape, 0, &[]),
            occ(&[3, 18, 22], square_area, DEF, &[3, 4, 3, 40]),
            occ(&[4, 16, 21], a, DEF, &[4, 4, 4, 40]),
            occ(&[5, 16, 21], b, DEF, &[5, 4, 5, 50]),
            occ(&[6, 16, 20], caller, DEF, &[6, 4, 8, 5]),
            occ(&[7, 8, 13], a, 0, &[]),
            occ(&[7, 20, 25], b, 0, &[]),
        ],
        vec![
            info(shape, Kind::Interface, &[]),
            info(shape_area, Kind::AbstractMethod, &[]),
            info(square, Kind::Class, &[shape]),
            info(square_area, Kind::Method, &[shape_area]),
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
    // Overloads are two symbols and two calls; `.` is the delimiter.
    assert_eq!(
        edges(&db, 8),
        [
            "App.Program.Main() | App.Square.Scale()",
            "App.Program.Main() | App.Square.Scale(+1)"
        ]
    );
    // Relationships from SymbolInformation.
    assert_eq!(edges(&db, 16), ["App.Square | App.IShape"]);
    assert_eq!(edges(&db, 32), ["App.Square.Area() | App.IShape.Area()"]);
    // The using directive is an import from the file.
    assert_eq!(edges(&db, 512).len(), 1);
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

/// Shaped like scip-dotnet 0.2.16: no enclosing ranges, no kinds,
/// `.ctor` names, and assembly names as packages.
#[test]
fn csharp_without_scopes_or_kinds_uses_the_text() {
    const D: &str = "scip-dotnet nuget . . ";
    let text = "namespace Demo;\n\
                public class Square\n\
                {\n\
                \x20   public double Side { get; }\n\
                \x20   public Square(double side) { Side = side; }\n\
                \x20   public double Twice() => Side * 2;\n\
                }\n";
    let square = format!("{D}Demo/Square#");
    let side = format!("{D}Demo/Square#Side.");
    let ctor = format!("{D}Demo/Square#`.ctor`().");
    let twice = format!("{D}Demo/Square#Twice().");
    let console = "scip-dotnet nuget System.Console 10.0.0.0 System/Console#";
    let mut d = doc(
        "Shapes.cs",
        "C#",
        vec![
            occ(&[0, 10, 14], &format!("{D}Demo/"), 0, &[]),
            occ(&[1, 13, 19], &square, DEF, &[]),
            occ(&[3, 18, 22], &side, DEF, &[]),
            occ(&[4, 11, 17], &ctor, DEF, &[]),
            occ(&[4, 33, 37], &side, 0, &[]),
            occ(&[5, 18, 23], &twice, DEF, &[]),
            occ(&[5, 29, 33], &side, 0, &[]),
            occ(&[5, 37, 38], console, 0, &[]),
        ],
        [&square, &side, &ctor, &twice]
            .iter()
            .map(|s| info(s, Kind::UnspecifiedKind, &[]))
            .collect(),
    );
    d.text = text.to_string();
    let db_path = scratch("csharp-text.srctrldb");
    let stats = convert(
        &index("scip-dotnet", "file:///proj/app", vec![d]),
        &opts(&db_path),
    )
    .unwrap();
    let db = Db::open(&db_path);
    // Four bodies from the text; only `namespace Demo;` hangs from the file.
    assert_eq!(stats.scopes_from_text, 4);
    assert_eq!(stats.references_without_context, 1);
    assert_eq!(
        edges(&db, 4),
        [
            "/proj/app/Shapes.cs | Demo",
            "Demo.Square.Square() | Demo.Square.Side",
            "Demo.Square.Twice() | Demo.Square.Side",
        ]
    );
    // `class` before the name: a class (128), not a plain type.
    assert_eq!(
        db.count(&format!(
            "SELECT type FROM node WHERE {NAME} = 'Demo.Square'"
        )),
        128
    );
    // The assembly is not a level of the name.
    assert_eq!(
        db.count(&format!(
            "SELECT count(*) FROM node WHERE {NAME} = 'System.Console'"
        )),
        1
    );
    // The scope of Twice() ends at its `;`.
    assert_eq!(
        db.rows(&format!(
            "SELECT l.start_line, l.end_line, l.end_column FROM node n \
             JOIN occurrence o ON o.element_id = n.id \
             JOIN source_location l ON l.id = o.source_location_id \
             WHERE {NAME} = 'Demo.Square.Twice()' AND l.type = 1"
        )),
        ["6 | 6 | 38"]
    );
}

#[test]
fn role_constants() {
    assert_eq!(SymbolRole::Definition.value(), DEF);
    assert_eq!(SymbolRole::Import.value(), IMPORT);
}

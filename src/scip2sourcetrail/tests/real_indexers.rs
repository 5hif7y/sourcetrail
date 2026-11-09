//! Runs the real SCIP indexers on the fixtures in tests/fixtures and checks
//! the converted databases. Each test is skipped, with a note, when its
//! indexer is not installed:
//!
//! - rust-analyzer: `rustup component add rust-analyzer`
//!   (SCIP_RUST_ANALYZER overrides the program);
//! - scip-dotnet: `dotnet tool install -g scip-dotnet` and a .NET SDK
//!   (SCIP_DOTNET overrides the program).
//!
//! The fixtures are copied to a temporary directory first, because the
//! indexers write build output (target/, obj/) next to the sources.

use rusqlite::Connection;
use scip2sourcetrail::generate::{ensure_index, Indexer};
use scip2sourcetrail::{run, Options};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

fn available(variable: &str, program: &str) -> bool {
    let program = std::env::var(variable).unwrap_or_else(|_| program.to_string());
    Command::new(program)
        .arg("--version")
        .output()
        .is_ok_and(|o| o.status.success())
}

fn copy_dir(from: &Path, to: &Path) {
    fs::create_dir_all(to).unwrap();
    for entry in fs::read_dir(from).unwrap() {
        let entry = entry.unwrap();
        let target = to.join(entry.file_name());
        if entry.file_type().unwrap().is_dir() {
            copy_dir(&entry.path(), &target);
        } else {
            fs::copy(entry.path(), target).unwrap();
        }
    }
}

/// A fresh copy of a fixture, and where its index and database go.
fn workspace(fixture: &str) -> (PathBuf, PathBuf, PathBuf) {
    let root = std::env::temp_dir()
        .join("scip2sourcetrail-real")
        .join(fixture);
    let _ = fs::remove_dir_all(&root);
    let project = root.join("project");
    copy_dir(
        &Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("tests/fixtures")
            .join(fixture),
        &project,
    );
    (project, root.join("index.scip"), root.join("db.srctrldb"))
}

/// `source -> target` for the edges of one type, by qualified name.
fn edges(db: &Connection, kind: i32) -> Vec<String> {
    const NAME: &str = "replace(replace(replace(replace(substr(serialized_name, \
        instr(serialized_name, char(9)||'m') + 2), char(9)||'s'||char(9)||'p'||char(9)||'n', char(9)||'n'), \
        char(9)||'s'||char(9)||'p', ''), char(9)||'s', ''), char(9)||'n', \
        substr(serialized_name, 1, instr(serialized_name, char(9)||'m') - 1))";
    let sql = format!(
        "SELECT DISTINCT a.n || ' -> ' || b.n FROM edge e \
         JOIN (SELECT id, {NAME} AS n FROM node) a ON a.id = e.source_node_id \
         JOIN (SELECT id, {NAME} AS n FROM node) b ON b.id = e.target_node_id \
         WHERE e.type = {kind} ORDER BY 1"
    );
    let mut stmt = db.prepare(&sql).unwrap();
    stmt.query_map([], |r| r.get::<_, String>(0))
        .unwrap()
        .map(|r| r.unwrap())
        .collect()
}

fn assert_has(list: &[String], items: &[&str]) {
    for i in items {
        assert!(list.contains(&i.to_string()), "{i} missing from {list:#?}");
    }
}

#[test]
fn rust_analyzer_fixture() {
    if !available("SCIP_RUST_ANALYZER", "rust-analyzer") {
        eprintln!("skipped: rust-analyzer is not installed");
        return;
    }
    let (project, scip, db_path) = workspace("rust-demo");
    assert!(ensure_index(Indexer::Rust, &project, &scip).unwrap());
    // Fresh: a second call reuses it.
    assert!(!ensure_index(Indexer::Rust, &project, &scip).unwrap());

    let stats = run(&Options {
        scip,
        database: db_path.clone(),
        ..Default::default()
    })
    .unwrap();
    assert_eq!(stats.documents, 2);
    assert_eq!(stats.references_without_context, 0);
    assert_eq!(stats.missing_sources, 0);

    let db = Connection::open(&db_path).unwrap();
    assert_has(
        &edges(&db, 8),
        &[
            "demo::main() -> demo::shapes::Square::new()",
            "demo::main() -> demo::total()",
            "demo::total() -> demo::shapes::Square::area() (Area)",
            "demo::total() -> demo::shapes::doubled()",
            "demo::shapes::doubled() -> demo::shapes::Square::area() (Area)",
        ],
    );
    assert_has(
        &edges(&db, 2048),
        &["demo::shapes::doubled() -> demo::shapes::twice"],
    );
    // `use crate::shapes::doubled` at module level is an import, not a call.
    assert_has(&edges(&db, 512), &["demo -> demo::shapes::doubled()"]);
    assert!(
        !edges(&db, 8).iter().any(|e| e.starts_with("demo -> ")),
        "no calls from the crate module: {:#?}",
        edges(&db, 8)
    );
    assert_eq!(
        edges(&db, 16),
        ["demo::shapes::Square -> demo::shapes::Area"]
    );
    assert_eq!(
        edges(&db, 32),
        ["demo::shapes::Square::area() (Area) -> demo::shapes::Area::area()"]
    );
    let std_defined: i64 = db
        .query_row(
            "SELECT count(*) FROM node n JOIN symbol s ON s.id = n.id \
             WHERE n.serialized_name LIKE '::'||char(9)||'mstd%' \
                OR n.serialized_name LIKE '::'||char(9)||'mcore%'",
            [],
            |r| r.get(0),
        )
        .unwrap();
    assert_eq!(std_defined, 0, "std and core stay non-indexed");
}

#[test]
fn scip_dotnet_fixture() {
    if !available("SCIP_DOTNET", "scip-dotnet") {
        eprintln!("skipped: scip-dotnet is not installed");
        return;
    }
    let (project, scip, db_path) = workspace("csharp-demo");
    assert!(ensure_index(Indexer::CSharp, &project, &scip).unwrap());

    let sources: Vec<String> = ["Program.cs", "Shapes.cs"]
        .iter()
        .map(|f| project.join(f).to_string_lossy().into_owned())
        .collect();
    let stats = run(&Options {
        scip,
        database: db_path.clone(),
        sources,
        ..Default::default()
    })
    .unwrap();
    assert_eq!((stats.documents, stats.missing_sources), (2, 0));
    // scip-dotnet gives no enclosing ranges: the bodies come from the text.
    assert!(stats.scopes_from_text >= 10, "{stats:?}");
    // Only `using System;` and the two `namespace Demo;` hang from a file.
    assert_eq!(stats.references_without_context, 3, "{stats:?}");

    let db = Connection::open(&db_path).unwrap();
    assert_has(
        &edges(&db, 8),
        &[
            "Demo.Program.Main() -> Demo.Square.Scale(+1)",
            "Demo.Program.Main() -> Demo.Program.Total()",
            "Demo.Program.Main() -> System.Console.WriteLine(+6)",
            "Demo.Program.Total() -> Demo.IShape.Area()",
            "Demo.Square.Scale(+1) -> Demo.Square.Scale()",
        ],
    );
    assert_eq!(edges(&db, 16), ["Demo.Square -> Demo.IShape"]);
    assert_eq!(edges(&db, 32), ["Demo.Square.Area() -> Demo.IShape.Area()"]);
    let kind = |name: &str| -> i64 {
        db.query_row(
            "SELECT type FROM node WHERE serialized_name LIKE '%'||char(9)||'n'||?1||char(9)||'s'||char(9)||'p'",
            [name],
            |r| r.get(0),
        )
        .unwrap()
    };
    assert_eq!(kind("Square"), 128, "class");
    assert_eq!(kind("IShape"), 256, "interface");
}

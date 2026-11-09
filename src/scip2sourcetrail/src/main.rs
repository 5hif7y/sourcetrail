//! scip2sourcetrail: writes a Sourcetrail database from a SCIP index.
//!
//! The arguments follow the custom-command indexers of Sourcetrail, so the
//! command can run in batches:
//!
//!   scip2sourcetrail index --scip=<index.scip> --database-file-path=<db>
//!       [--source-file-path=<file>]... [--project-root=<dir>]
//!       [--delimiter=<sep>] [--language=<id>] [--stats]
//!       [--generate=rust|csharp --project=<path>]
//!
//! With --generate, <index.scip> is a cache: it is (re)generated with
//! rust-analyzer or scip-dotnet when the project's sources are newer.

use scip2sourcetrail::generate::{ensure_index, Indexer};
use scip2sourcetrail::{run, Options};
use std::path::PathBuf;
use std::process::ExitCode;

const USAGE: &str = "usage: scip2sourcetrail index --scip=<index.scip> --database-file-path=<db> \
[--source-file-path=<file>]... [--project-root=<dir>] [--delimiter=<sep>] [--language=<id>] [--stats] \
[--generate=rust|csharp --project=<path>]

Without --source-file-path, every document of the index is written.
With --generate, the index is a cache regenerated when <path> has newer sources:
  rust:   rust-analyzer scip <path>   (SCIP_RUST_ANALYZER overrides the program)
  csharp: scip-dotnet index <.sln/.csproj in path>   (SCIP_DOTNET overrides it)";

fn main() -> ExitCode {
    let mut args = std::env::args().skip(1);
    if args.next().as_deref() != Some("index") {
        eprintln!("{USAGE}");
        return ExitCode::from(2);
    }
    let mut options = Options::default();
    let mut scip = None;
    let mut database = None;
    let mut stats = false;
    let mut generate = None;
    let mut project = None;
    for a in args {
        let (key, value) = match a.split_once('=') {
            Some((k, v)) => (k.to_string(), v.to_string()),
            None => (a.clone(), String::new()),
        };
        match key.as_str() {
            "--scip" => scip = Some(PathBuf::from(value)),
            "--database-file-path" => database = Some(PathBuf::from(value)),
            "--source-file-path" => options.sources.push(value),
            "--project-root" => options.project_root = Some(PathBuf::from(value)),
            "--delimiter" => options.delimiter = Some(value),
            "--language" => options.language = Some(value),
            "--stats" => stats = true,
            "--generate" => match Indexer::parse(&value) {
                Some(i) => generate = Some(i),
                None => {
                    eprintln!("--generate expects rust or csharp, not {value:?}\n{USAGE}");
                    return ExitCode::from(2);
                }
            },
            "--project" => project = Some(PathBuf::from(value)),
            _ => {
                eprintln!("unknown argument: {a}\n{USAGE}");
                return ExitCode::from(2);
            }
        }
    }
    let (Some(scip), Some(database)) = (scip, database) else {
        eprintln!("{USAGE}");
        return ExitCode::from(2);
    };
    if scip.as_os_str().is_empty() || database.as_os_str().is_empty() {
        eprintln!("{USAGE}");
        return ExitCode::from(2);
    }
    match (generate, &project) {
        (Some(indexer), Some(project)) if !project.as_os_str().is_empty() => {
            if let Err(e) = ensure_index(indexer, project, &scip) {
                eprintln!("scip2sourcetrail: {e}");
                return ExitCode::FAILURE;
            }
        }
        (None, None) => {}
        _ => {
            eprintln!("--generate and --project go together\n{USAGE}");
            return ExitCode::from(2);
        }
    }
    options.scip = scip;
    options.database = database;
    match run(&options) {
        Ok(s) => {
            if stats {
                println!("{s:#?}");
            }
            ExitCode::SUCCESS
        }
        Err(e) => {
            eprintln!("scip2sourcetrail: {e}");
            ExitCode::FAILURE
        }
    }
}

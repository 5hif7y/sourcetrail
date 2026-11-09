//! Runs the SCIP indexer of a language, once per refresh.
//!
//! Sourcetrail runs a custom-command indexer once per batch of files, but a
//! SCIP indexer indexes the whole project (and takes tens of seconds). So the
//! index is cached: it is regenerated only when a source or project file is
//! newer than it, and a lock file makes concurrent batches wait for the one
//! that generates it.
//!
//! A failure is cached too, in `<scip>.failed`: until a source changes (or
//! the file is deleted), the other batches report it instead of running the
//! indexer again, which on a large project would cost minutes per batch.
//!
//! rust-analyzer can panic on code it fails to type-check (1.97 does on
//! some closures). Test code is the usual culprit and the least needed in
//! a code map, so after a panic the index is generated again without
//! `#[cfg(test)]` code and the tests, and `<scip>.without-tests` says so.

use std::fs;
use std::io::ErrorKind;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::mpsc;
use std::thread::JoinHandle;
use std::time::{Duration, Instant, SystemTime};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Indexer {
    /// `rust-analyzer scip` (rustup component add rust-analyzer).
    Rust,
    /// `scip-dotnet index` (dotnet tool install -g scip-dotnet).
    CSharp,
}

impl Indexer {
    pub fn parse(s: &str) -> Option<Indexer> {
        match s.to_ascii_lowercase().as_str() {
            "rust" => Some(Indexer::Rust),
            "csharp" | "c#" | "cs" => Some(Indexer::CSharp),
            _ => None,
        }
    }

    /// Files whose change invalidates the index.
    fn watches(self, path: &Path) -> bool {
        let name = path.file_name().and_then(|n| n.to_str()).unwrap_or("");
        let ext = path
            .extension()
            .and_then(|e| e.to_str())
            .unwrap_or("")
            .to_ascii_lowercase();
        match self {
            Indexer::Rust => ext == "rs" || name == "Cargo.toml" || name == "Cargo.lock",
            Indexer::CSharp => matches!(
                ext.as_str(),
                "cs" | "csproj" | "sln" | "slnx" | "props" | "targets"
            ),
        }
    }

    /// Directories that hold build output, not sources.
    fn skips(self, dir: &str) -> bool {
        dir.starts_with('.')
            || match self {
                Indexer::Rust => dir == "target",
                Indexer::CSharp => dir == "bin" || dir == "obj",
            }
    }
}

/// The newest modification time of the files `indexer` watches under
/// `root`, or None if there are none.
pub fn newest_source(indexer: Indexer, root: &Path) -> Option<SystemTime> {
    let mut newest = None;
    let mut stack = vec![root.to_path_buf()];
    if root.is_file() {
        return fs::metadata(root).and_then(|m| m.modified()).ok();
    }
    while let Some(dir) = stack.pop() {
        let Ok(entries) = fs::read_dir(&dir) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            let Ok(kind) = entry.file_type() else {
                continue;
            };
            if kind.is_dir() {
                let name = entry.file_name();
                if !indexer.skips(&name.to_string_lossy()) {
                    stack.push(path);
                }
            } else if indexer.watches(&path) {
                if let Ok(t) = entry.metadata().and_then(|m| m.modified()) {
                    newest = Some(newest.map_or(t, |n: SystemTime| n.max(t)));
                }
            }
        }
    }
    newest
}

/// Whether the index at `scip` is missing or older than the sources.
pub fn is_stale(indexer: Indexer, project: &Path, scip: &Path) -> bool {
    let Ok(index_time) = fs::metadata(scip).and_then(|m| m.modified()) else {
        return true;
    };
    newest_source(indexer, project).is_some_and(|t| t > index_time)
}

/// Holds `<scip>.lock` while it lives, touching it every `stale / 4` so
/// that the waiting batches can tell a live holder from a crashed one.
struct Lock {
    path: PathBuf,
    heartbeat: Option<(mpsc::Sender<()>, JoinHandle<()>)>,
}

impl Lock {
    /// Waits for the lock, up to `wait`. A lock not touched for `stale` is
    /// left over from a crashed run and is taken over.
    fn acquire(path: PathBuf, wait: Duration, stale: Duration) -> Result<Lock, String> {
        let start = Instant::now();
        loop {
            match fs::OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(&path)
            {
                Ok(_) => return Ok(Lock::held(path, stale / 4)),
                Err(e) if e.kind() == ErrorKind::AlreadyExists => {
                    let age = fs::metadata(&path)
                        .and_then(|m| m.modified())
                        .ok()
                        .and_then(|t| t.elapsed().ok());
                    if age.is_some_and(|a| a > stale) {
                        let _ = fs::remove_file(&path);
                        continue;
                    }
                    if start.elapsed() > wait {
                        return Err(format!("timed out waiting for {}", path.display()));
                    }
                    std::thread::sleep(Duration::from_millis(250));
                }
                Err(e) => return Err(format!("cannot create {}: {e}", path.display())),
            }
        }
    }
}

impl Lock {
    fn held(path: PathBuf, every: Duration) -> Lock {
        let (stop, ticks) = mpsc::channel::<()>();
        let touched = path.clone();
        let beat = std::thread::spawn(move || {
            while let Err(mpsc::RecvTimeoutError::Timeout) = ticks.recv_timeout(every) {
                if let Ok(f) = fs::OpenOptions::new().write(true).open(&touched) {
                    let _ = f.set_modified(SystemTime::now());
                }
            }
        });
        Lock {
            path,
            heartbeat: Some((stop, beat)),
        }
    }
}

impl Drop for Lock {
    fn drop(&mut self) {
        if let Some((stop, beat)) = self.heartbeat.take() {
            drop(stop);
            let _ = beat.join();
        }
        let _ = fs::remove_file(&self.path);
    }
}

/// `<scip><suffix>`.
fn beside(scip: &Path, suffix: &str) -> PathBuf {
    let mut p = scip.as_os_str().to_owned();
    p.push(suffix);
    PathBuf::from(p)
}

/// Why the index at `scip` leaves the tests out, if it does.
pub fn tests_left_out(scip: &Path) -> Option<String> {
    fs::read_to_string(beside(scip, ".without-tests")).ok()
}

/// The first `panicked at` line of an indexer's error and the message
/// after it.
fn panic_message(error: &str) -> Option<String> {
    let mut lines = error.lines().skip_while(|l| !l.contains("panicked at"));
    let at = lines.next()?.trim();
    Some(
        match lines.next().map(str::trim).filter(|l| !l.is_empty()) {
            Some(message) => format!("{at} {message}"),
            None => at.to_string(),
        },
    )
}

/// A backtrace frame (`  12: name` or `      at file:line`) or its framing.
fn is_backtrace(line: &str) -> bool {
    let t = line.trim_start();
    let frame = t
        .split_once(": ")
        .is_some_and(|(n, _)| !n.is_empty() && n.bytes().all(|b| b.is_ascii_digit()));
    (frame && line.starts_with(' '))
        || (t.starts_with("at ") && line.starts_with(' '))
        || t == "stack backtrace:"
        || t.starts_with("note: Some details are omitted")
        || t.starts_with("note: run with `RUST_BACKTRACE")
}

/// The program for a tool: the environment variable when set, else the
/// name on the PATH.
fn program(variable: &str, name: &str) -> String {
    std::env::var(variable)
        .ok()
        .filter(|v| !v.is_empty())
        .unwrap_or_else(|| name.to_string())
}

/// The solution or project scip-dotnet should index: `project` itself if it
/// is a file, else the only .sln/.slnx in it, else the only .csproj.
pub fn dotnet_target(project: &Path) -> Result<PathBuf, String> {
    if project.is_file() {
        return Ok(project.to_path_buf());
    }
    let entries: Vec<PathBuf> = fs::read_dir(project)
        .map_err(|e| format!("cannot read {}: {e}", project.display()))?
        .flatten()
        .map(|e| e.path())
        .collect();
    for exts in [&["sln", "slnx"][..], &["csproj"][..]] {
        let found: Vec<&PathBuf> = entries
            .iter()
            .filter(|p| {
                p.extension()
                    .and_then(|e| e.to_str())
                    .is_some_and(|e| exts.contains(&e))
            })
            .collect();
        match found.as_slice() {
            [one] => return Ok((*one).clone()),
            [] => continue,
            _ => {
                return Err(format!(
                    "{} has several {} files; point the source path at one",
                    project.display(),
                    exts.join("/")
                ))
            }
        }
    }
    Err(format!(
        "no .sln, .slnx or .csproj in {}",
        project.display()
    ))
}

fn run(mut command: Command, what: &str) -> Result<(), String> {
    let output = command
        .output()
        .map_err(|e| format!("cannot run {what}: {e}"))?;
    if output.status.success() {
        Ok(())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);
        // Without the backtrace, so that a panic message is not pushed out.
        let tail: Vec<&str> = stderr
            .lines()
            .chain(stdout.lines())
            .filter(|l| !is_backtrace(l))
            .rev()
            .take(15)
            .collect();
        Err(format!(
            "{what} failed ({}):\n{}",
            output.status,
            tail.into_iter().rev().collect::<Vec<_>>().join("\n")
        ))
    }
}

/// Makes sure `scip` is an up-to-date index of `project`, generating it if
/// needed. Returns whether it was generated.
pub fn ensure_index(indexer: Indexer, project: &Path, scip: &Path) -> Result<bool, String> {
    if let Some(dir) = scip.parent() {
        if !dir.as_os_str().is_empty() {
            fs::create_dir_all(dir).map_err(|e| format!("cannot create {}: {e}", dir.display()))?;
        }
    }
    // The holder touches the lock every minute, so a waiter gives up on a
    // dead holder after 4 minutes but waits for a live one for hours.
    let _lock = Lock::acquire(
        beside(scip, ".lock"),
        Duration::from_secs(12 * 60 * 60),
        Duration::from_secs(4 * 60),
    )?;
    if !is_stale(indexer, project, scip) {
        return Ok(false);
    }
    let failed = beside(scip, ".failed");
    if !is_stale(indexer, project, &failed) {
        let previous = fs::read_to_string(&failed).unwrap_or_default();
        return Err(format!(
            "{previous}\n(cached from an earlier run in {}; change a source or delete it to retry)",
            failed.display()
        ));
    }
    let result = generate(indexer, project, scip);
    match &result {
        Ok(()) => {
            let _ = fs::remove_file(&failed);
        }
        Err(e) => {
            let _ = fs::write(&failed, e);
        }
    }
    result.map(|()| true)
}

fn generate(indexer: Indexer, project: &Path, scip: &Path) -> Result<(), String> {
    let tmp = beside(scip, ".tmp");
    let _ = fs::remove_file(&tmp);
    let without_tests = beside(scip, ".without-tests");
    match indexer {
        Indexer::Rust => {
            let rust_analyzer = |config: Option<&Path>| {
                let mut c = Command::new(program("SCIP_RUST_ANALYZER", "rust-analyzer"));
                c.arg("scip").arg(project).arg("--output").arg(&tmp);
                if let Some(config) = config {
                    c.arg("--config-path").arg(config);
                }
                run(c, "rust-analyzer scip")
            };
            match rust_analyzer(None) {
                Ok(()) => {
                    let _ = fs::remove_file(&without_tests);
                }
                Err(first) => {
                    let Some(panic) = panic_message(&first) else {
                        return Err(first);
                    };
                    let config = beside(scip, ".no-tests.json");
                    fs::write(&config, r#"{"cfg":{"setTest":false}}"#)
                        .map_err(|e| format!("cannot write {}: {e}", config.display()))?;
                    let _ = fs::remove_file(&tmp);
                    let second = rust_analyzer(Some(&config));
                    let _ = fs::remove_file(&config);
                    if let Err(second) = second {
                        return Err(format!("{first}\nwithout the tests too:\n{second}"));
                    }
                    fs::write(&without_tests, &panic)
                        .map_err(|e| format!("cannot write {}: {e}", without_tests.display()))?;
                    eprintln!(
                        "scip2sourcetrail: rust-analyzer crashed ({panic}); indexed without the tests"
                    );
                }
            }
        }
        Indexer::CSharp => {
            let target = dotnet_target(project)?;
            let dir = target.parent().unwrap_or(Path::new("."));
            let mut c = Command::new(program("SCIP_DOTNET", "scip-dotnet"));
            c.arg("index")
                .arg(&target)
                .arg("--working-directory")
                .arg(if dir.as_os_str().is_empty() {
                    Path::new(".")
                } else {
                    dir
                })
                .arg("--output")
                .arg(&tmp);
            run(c, "scip-dotnet index")?;
        }
    }
    if !tmp.exists() {
        return Err(format!("the indexer wrote no {}", tmp.display()));
    }
    fs::rename(&tmp, scip).map_err(|e| format!("cannot move the index to {}: {e}", scip.display()))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    /// Serializes the tests that set SCIP_RUST_ANALYZER.
    static ENV: Mutex<()> = Mutex::new(());

    fn scratch(name: &str) -> PathBuf {
        let dir = std::env::temp_dir()
            .join("scip2sourcetrail-generate")
            .join(name);
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn freshness_ignores_build_output() {
        let root = scratch("fresh");
        fs::create_dir_all(root.join("src")).unwrap();
        fs::create_dir_all(root.join("target/debug")).unwrap();
        fs::write(root.join("src/main.rs"), "fn main() {}").unwrap();
        let scip = root.join("index.scip");
        assert!(
            is_stale(Indexer::Rust, &root, &scip),
            "missing index is stale"
        );
        std::thread::sleep(Duration::from_millis(50));
        fs::write(&scip, b"").unwrap();
        assert!(!is_stale(Indexer::Rust, &root, &scip));
        // Build output and unrelated files do not invalidate it.
        std::thread::sleep(Duration::from_millis(50));
        fs::write(root.join("target/debug/out.rs"), "").unwrap();
        fs::write(root.join("notes.txt"), "").unwrap();
        assert!(!is_stale(Indexer::Rust, &root, &scip));
        // A source change does.
        fs::write(root.join("src/main.rs"), "fn main() { }").unwrap();
        assert!(is_stale(Indexer::Rust, &root, &scip));
    }

    #[test]
    fn dotnet_targets() {
        let root = scratch("dotnet");
        assert!(dotnet_target(&root).is_err());
        fs::write(root.join("App.csproj"), "").unwrap();
        assert_eq!(dotnet_target(&root).unwrap(), root.join("App.csproj"));
        fs::write(root.join("App.sln"), "").unwrap();
        assert_eq!(dotnet_target(&root).unwrap(), root.join("App.sln"));
        fs::write(root.join("Other.sln"), "").unwrap();
        assert!(dotnet_target(&root).unwrap_err().contains("several"));
        assert_eq!(
            dotnet_target(&root.join("App.csproj")).unwrap(),
            root.join("App.csproj")
        );
    }

    #[test]
    fn lock_is_exclusive_and_released() {
        let root = scratch("lock");
        let path = root.join("index.scip.lock");
        let first = Lock::acquire(
            path.clone(),
            Duration::from_secs(5),
            Duration::from_secs(60),
        )
        .unwrap();
        assert!(Lock::acquire(
            path.clone(),
            Duration::from_millis(300),
            Duration::from_secs(60)
        )
        .is_err());
        drop(first);
        assert!(!path.exists());
        assert!(Lock::acquire(path, Duration::from_secs(1), Duration::from_secs(60)).is_ok());
    }

    #[test]
    fn a_failing_indexer_reports_its_output() {
        let _env = ENV.lock().unwrap_or_else(|e| e.into_inner());
        let root = scratch("fail");
        std::env::set_var("SCIP_RUST_ANALYZER", root.join("no-such-rust-analyzer"));
        let err = ensure_index(Indexer::Rust, &root, &root.join("index.scip")).unwrap_err();
        std::env::remove_var("SCIP_RUST_ANALYZER");
        assert!(err.contains("cannot run rust-analyzer scip"), "{err}");
        assert!(!root.join("index.scip.lock").exists());
    }

    /// A program that runs and fails at once: `cargo scip …` is not a
    /// cargo command.
    #[test]
    fn a_failure_is_cached_until_a_source_changes() {
        let _env = ENV.lock().unwrap_or_else(|e| e.into_inner());
        let root = scratch("cached");
        fs::write(root.join("main.rs"), "fn main() {}").unwrap();
        let scip = root.join("index.scip");
        std::env::set_var("SCIP_RUST_ANALYZER", "cargo");
        let first = ensure_index(Indexer::Rust, &root, &scip).unwrap_err();
        let failed = root.join("index.scip.failed");
        assert!(first.contains("rust-analyzer scip failed"), "{first}");
        assert_eq!(fs::read_to_string(&failed).unwrap(), first);

        // The next batch reports it without running the indexer.
        let second = ensure_index(Indexer::Rust, &root, &scip).unwrap_err();
        assert!(
            second.starts_with(&first) && second.contains("cached"),
            "{second}"
        );

        // A source change retries.
        std::thread::sleep(Duration::from_millis(50));
        fs::write(root.join("main.rs"), "fn main() { }").unwrap();
        let third = ensure_index(Indexer::Rust, &root, &scip).unwrap_err();
        std::env::remove_var("SCIP_RUST_ANALYZER");
        assert!(!third.contains("cached"), "{third}");
    }

    /// A rust-analyzer that panics unless it gets `--config-path`; then it
    /// writes an (empty) index to its `--output`, the 4th argument.
    fn panicking_rust_analyzer(dir: &Path) -> PathBuf {
        if cfg!(windows) {
            let path = dir.join("fake-ra.cmd");
            fs::write(
                &path,
                "@echo off\r\n\
                 if \"%5\"==\"--config-path\" (type nul > \"%4\" & exit /b 0)\r\n\
                 echo thread 'main' panicked at callee.rs:587:17: 1>&2\r\n\
                 echo no suitable Fn implementation 1>&2\r\n\
                 exit /b 101\r\n",
            )
            .unwrap();
            path
        } else {
            let path = dir.join("fake-ra.sh");
            fs::write(
                &path,
                "#!/bin/sh\n\
                 if [ \"$5\" = --config-path ]; then : > \"$4\"; exit 0; fi\n\
                 echo \"thread 'main' panicked at callee.rs:587:17:\" >&2\n\
                 echo 'no suitable Fn implementation' >&2\n\
                 exit 101\n",
            )
            .unwrap();
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                fs::set_permissions(&path, fs::Permissions::from_mode(0o755)).unwrap();
            }
            path
        }
    }

    #[test]
    fn a_panic_retries_without_the_tests() {
        let _env = ENV.lock().unwrap_or_else(|e| e.into_inner());
        let root = scratch("without-tests");
        let project = root.join("project");
        fs::create_dir_all(&project).unwrap();
        fs::write(project.join("main.rs"), "fn main() {}").unwrap();
        let scip = root.join("index.scip");
        std::env::set_var("SCIP_RUST_ANALYZER", panicking_rust_analyzer(&root));
        let generated = ensure_index(Indexer::Rust, &project, &scip);
        std::env::remove_var("SCIP_RUST_ANALYZER");
        assert_eq!(generated, Ok(true));
        assert!(scip.exists());
        assert_eq!(
            tests_left_out(&scip).unwrap(),
            "thread 'main' panicked at callee.rs:587:17: no suitable Fn implementation"
        );
        assert!(!root.join("index.scip.failed").exists());
        assert!(!root.join("index.scip.no-tests.json").exists());
    }

    #[test]
    fn panic_messages() {
        let error = "rust-analyzer scip failed (exit code: 101):\n\
            ERROR Encountered enclosing definition with no name\n\
            thread 'main' (19696) panicked at crates/hir-ty/src/infer/callee.rs:587:17:\n\
            Expected to find a suitable `Fn` implementation\n\
            stack backtrace:";
        assert_eq!(
            panic_message(error).unwrap(),
            "thread 'main' (19696) panicked at crates/hir-ty/src/infer/callee.rs:587:17: \
             Expected to find a suitable `Fn` implementation"
        );
        assert_eq!(
            panic_message("cannot run rust-analyzer scip: not found"),
            None
        );
    }

    #[test]
    fn backtraces_are_left_out_of_errors() {
        for l in [
            "stack backtrace:",
            "   0: std::panicking::begin_panic",
            "  22: std::rt::lang_start_internal",
            "             at /rustc/abc/library/std/src/rt.rs:171",
            "note: Some details are omitted, run with `RUST_BACKTRACE=full` for a verbose backtrace.",
        ] {
            assert!(is_backtrace(l), "{l}");
        }
        for l in [
            "thread 'main' panicked at crates/ide/src/lib.rs:10:5:",
            "index out of bounds: the len is 3 but the index is 7",
            "error: 2: something",
            "at least one crate failed",
        ] {
            assert!(!is_backtrace(l), "{l}");
        }
    }

    #[test]
    fn a_live_holder_keeps_its_lock() {
        let root = scratch("heartbeat");
        let path = root.join("index.scip.lock");
        let stale = Duration::from_millis(400);
        let held = Lock::acquire(path.clone(), Duration::from_secs(1), stale).unwrap();
        // Well past `stale`, but the holder keeps touching it.
        std::thread::sleep(Duration::from_millis(1200));
        assert!(Lock::acquire(path.clone(), Duration::from_millis(300), stale).is_err());
        drop(held);
        // A dead holder's lock is taken over once it is stale.
        fs::write(&path, "").unwrap();
        std::thread::sleep(Duration::from_millis(600));
        assert!(Lock::acquire(path, Duration::from_millis(100), stale).is_ok());
    }
}

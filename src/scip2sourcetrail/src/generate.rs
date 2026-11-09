//! Runs the SCIP indexer of a language, once per refresh.
//!
//! Sourcetrail runs a custom-command indexer once per batch of files, but a
//! SCIP indexer indexes the whole project (and takes tens of seconds). So the
//! index is cached: it is regenerated only when a source or project file is
//! newer than it, and a lock file makes concurrent batches wait for the one
//! that generates it.

use std::fs;
use std::io::ErrorKind;
use std::path::{Path, PathBuf};
use std::process::Command;
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

/// Holds `<scip>.lock` while it lives.
struct Lock(PathBuf);

impl Lock {
    /// Waits for the lock, up to `wait`. A lock older than `stale` is left
    /// over from a crashed run and is taken over; it must be longer than any
    /// indexer run.
    fn acquire(path: PathBuf, wait: Duration, stale: Duration) -> Result<Lock, String> {
        let start = Instant::now();
        loop {
            match fs::OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(&path)
            {
                Ok(_) => return Ok(Lock(path)),
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

impl Drop for Lock {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.0);
    }
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
        let tail: Vec<&str> = stderr
            .lines()
            .chain(stdout.lines())
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
    let mut lock_path = scip.as_os_str().to_owned();
    lock_path.push(".lock");
    let hour = Duration::from_secs(60 * 60);
    let _lock = Lock::acquire(PathBuf::from(lock_path), hour, hour)?;
    if !is_stale(indexer, project, scip) {
        return Ok(false);
    }

    let mut tmp = scip.as_os_str().to_owned();
    tmp.push(".tmp");
    let tmp = PathBuf::from(tmp);
    let _ = fs::remove_file(&tmp);
    match indexer {
        Indexer::Rust => {
            let mut c = Command::new(program("SCIP_RUST_ANALYZER", "rust-analyzer"));
            c.arg("scip").arg(project).arg("--output").arg(&tmp);
            run(c, "rust-analyzer scip")?;
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
    fs::rename(&tmp, scip)
        .map_err(|e| format!("cannot move the index to {}: {e}", scip.display()))?;
    Ok(true)
}

#[cfg(test)]
mod tests {
    use super::*;

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
        let root = scratch("fail");
        std::env::set_var("SCIP_RUST_ANALYZER", root.join("no-such-rust-analyzer"));
        let err = ensure_index(Indexer::Rust, &root, &root.join("index.scip")).unwrap_err();
        std::env::remove_var("SCIP_RUST_ANALYZER");
        assert!(err.contains("cannot run rust-analyzer scip"), "{err}");
        assert!(!root.join("index.scip.lock").exists());
    }
}

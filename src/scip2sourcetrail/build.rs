//! Links the SourcetrailDB writer shim (src/odin_writer, ABI v2).
//!
//! The shim is found in SODB_WRITER_DIR, or else in build/odin_writer at the
//! root of the repository: the directory that `cmake -S src/odin_writer -B
//! build/odin_writer` (or src/odin_indexer/build.ps1) creates. On Windows the
//! DLL is copied next to the binaries and tests so they run from the target
//! directory.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};

const LIB: &str = "sourcetrail_odin_writer";

fn main() {
    println!("cargo:rerun-if-env-changed=SODB_WRITER_DIR");
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let dir = match env::var_os("SODB_WRITER_DIR") {
        Some(d) => PathBuf::from(d),
        None => manifest.join("../../build/odin_writer"),
    };

    // CMake puts the import library in lib/ and the DLL in bin/ (see
    // src/odin_writer/CMakeLists.txt); on Unix the shared library lands in the
    // build directory itself or in a configuration subdirectory.
    let candidates = [
        dir.join("lib"),
        dir.join("bin"),
        dir.clone(),
        dir.join("Release"),
        dir.join("Debug"),
    ];
    let mut found = false;
    for c in &candidates {
        if c.is_dir() {
            println!("cargo:rustc-link-search=native={}", c.display());
            if has_library(c) {
                found = true;
            }
        }
    }
    if !found {
        panic!(
            "{LIB} not found under {}. Build it first: \
             cmake -S src/odin_writer -B build/odin_writer && \
             cmake --build build/odin_writer --config Release, \
             or set SODB_WRITER_DIR.",
            dir.display()
        );
    }
    println!("cargo:rustc-link-lib=dylib={LIB}");

    if cfg!(not(windows)) {
        // Let the binaries find the shared library where it was built.
        for c in &candidates {
            if c.is_dir() && has_library(c) {
                println!("cargo:rustc-link-arg=-Wl,-rpath,{}", c.display());
            }
        }
    }

    if cfg!(windows) {
        let dll = dir.join("bin").join(format!("{LIB}.dll"));
        println!("cargo:rerun-if-changed={}", dll.display());
        if dll.exists() {
            // OUT_DIR is target/<profile>/build/<crate>-<hash>/out.
            let out = PathBuf::from(env::var("OUT_DIR").unwrap());
            if let Some(profile) = out.ancestors().nth(3) {
                copy(&dll, &profile.join(format!("{LIB}.dll")));
                copy(&dll, &profile.join("deps").join(format!("{LIB}.dll")));
            }
        }
    }
}

fn has_library(dir: &Path) -> bool {
    [
        format!("{LIB}.lib"),
        format!("lib{LIB}.so"),
        format!("lib{LIB}.dylib"),
    ]
    .iter()
    .any(|f| dir.join(f).exists())
}

fn copy(from: &Path, to: &Path) {
    if let Some(parent) = to.parent() {
        let _ = fs::create_dir_all(parent);
    }
    let _ = fs::copy(from, to);
}

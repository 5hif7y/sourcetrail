# Embedded / low-resource profile

Neo-Sourcetrail builds a headless CLI by default: no Qt GUI, no LLVM/C++ indexer and no
Java indexer. The desktop application is opt-in.

## Build

```cmd
cmake -B build-embedded -S . ^
  -DCMAKE_TOOLCHAIN_FILE="[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake"
cmake --build build-embedded --config Release --parallel
```

The configure installs only the base dependencies (Boost, ICU, SQLite, tinyxml,
nlohmann-json); the `gui`, `cxx` and `tests` vcpkg features are off. The result is a
small CLI linked against a static core (no `Sourcetrail_lib.dll` in the default profile).

To add a language plugin without the GUI:

```cmd
# Java (JNI + JDT)
cmake -B build-embedded -S . -DBUILD_JAVA_LANGUAGE_PACKAGE=ON ...
# C++ (LLVM; large)
cmake -B build-embedded -S . -DBUILD_CXX_LANGUAGE_PACKAGE=ON ...
```

Both force `SOURCETRAIL_SHARED_CORE=ON`, because the plugins are DLLs.

## Resource controls

| Environment variable | Default | Meaning |
|---|---|---|
| `SOURCETRAIL_INDEXER_THREADS` | `min(cores, 2)` headless | Number of indexing workers. |
| `SOURCETRAIL_JAVA_HEAP` | 25% of RAM, 256..2048 MB | JVM `-Xmx` in MB (accepts `512` or `512m`). |
| `SOURCETRAIL_PYTHON_SHALLOW` | off | Use the parso shallow indexer (less precise, lighter). |
| `SOURCETRAIL_FTS` | `0` in CLI, on in GUI | Build the in-memory full-text search index. |
| `SOURCETRAIL_PYTHON_DAEMON` | on | Keep a Python daemon per database (client + warm imports). Set `0` for one process per file. |
| `SOURCETRAIL_PYTHON_DAEMON_IDLE` | 60 | Seconds before an idle daemon exits. |
| `SOURCETRAIL_PYTHON_DAEMON_DEBUG` | off | Log daemon/client connection details to stderr. |

The CLI exposes the same controls:

```cmd
sourcetrail_cli.exe --refresh=updated --threads=1 --java-heap=512 --python-shallow project.srctrlprj
```

`--refresh=updated` (default) indexes only changed/incomplete files; use
`--refresh=all` for a full rebuild. The first run on an empty database is a full index.

## Data deployment

Only `fallback`, `license`, `python` and (when enabled) `java` are copied. Color schemes,
fonts, syntax rules, the GUI templates and the tutorial projects are not copied. Set
`-DCOPY_SAMPLE_PROJECTS=ON` if you want the tutorials in the app data.

## Notes

- The Python indexer needs `jedi`, `parso` and `numbat` in the `python` environment used
  to run it.
- The C++ indexer needs LLVM and is only recommended on larger machines.
- The Java indexer needs a JDK; its heap is capped automatically.

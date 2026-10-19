# Building Neo-Sourcetrail on Windows

This guide explains how to build the Neo-Sourcetrail project on Windows using MSVC (Microsoft Visual C++) and vcpkg.

## Prerequisites

1. **Visual Studio 2022 or newer** (2026 is supported) with the "Desktop development with C++" workload installed.
2. **CMake** (version 3.27 or higher; required by `cmake_minimum_required`).
3. **Git**.
4. **vcpkg** (C++ Library Manager).

## Step-by-Step Guide

### 1. Install vcpkg

If you use the bundled submodule, initialize it; otherwise clone and bootstrap vcpkg yourself:

```cmd
git submodule update --init --recursive
cd vcpkg
.\bootstrap-vcpkg.bat
```

Make sure the `vcpkg` executable is in your PATH, or note its installation directory.

### 2. Clone the Repository

```cmd
git clone https://github.com/your-org/neo-sourcetrail.git
cd neo-sourcetrail
git submodule update --init --recursive
```

The `SourcetrailPythonIndexer` sources are part of `bin/app/data/python/`; no extra
download is needed for the indexer itself.

### 3. Configure the Project with CMake

Neo-Sourcetrail uses `vcpkg.json` (manifest mode) to manage dependencies automatically.

```cmd
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake"
```

**The default is the embedded/headless profile**: CLI only, no Qt GUI, no C++ indexer
and no Java indexer. The configure step installs only the base dependencies (Boost,
ICU, SQLite, tinyxml, nlohmann-json); it does **not** install Qt, LLVM or the JVM.

Dependencies are split into **vcpkg manifest features**:

- `gui` (Qt) is only installed when the GUI is enabled.
- `cxx` (LLVM/Clang) is only installed when the C/C++ indexer is enabled.
- `tests` (Catch2/GoogleTest) is only installed when the unit tests are enabled.

To build the full desktop application (GUI + C++ + Java + tests):

```cmd
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake" ^
  -DBUILD_GUI=ON ^
  -DBUILD_CXX_LANGUAGE_PACKAGE=ON ^
  -DBUILD_JAVA_LANGUAGE_PACKAGE=ON ^
  -DBUILD_UNIT_TESTS_PACKAGE=ON
```

Other useful options:

| Option | Default | Meaning |
|---|---|---|
| `BUILD_GUI` | `OFF` | Build the Qt GUI (`Sourcetrail.exe`). |
| `BUILD_CXX_LANGUAGE_PACKAGE` | `OFF` | Build the C/C++ indexer (installs LLVM; hours from source). |
| `BUILD_JAVA_LANGUAGE_PACKAGE` | `OFF` | Build the Java indexer (requires JDK 17+ and Maven). |
| `BUILD_UNIT_TESTS_PACKAGE` | `OFF` | Build the unit tests (requires `BUILD_GUI=ON`; installs Catch2/GoogleTest). |
| `COPY_SAMPLE_PROJECTS` | `OFF` | Copy the tutorial projects into the app data. |
| `SOURCETRAIL_SHARED_CORE` | `OFF` | Shared core for plugins; forced `ON` when CXX or Java is enabled. |

The `gui` feature keeps Qt out of the dependency tree in embedded builds; the `cxx`
feature keeps LLVM out. This is what makes a fresh embedded configure fast.

### 4. Build the Project

```cmd
cmake --build build --config Release --parallel
```

The default `ALL` target of the embedded profile builds only the headless pieces. To
build explicitly:

```cmd
cmake --build build --config Release --target sourcetrail_cli --parallel
```

The GUI target (`Sourcetrail`) only exists when `BUILD_GUI=ON`, and the language plugins
only exist when their options are enabled.

### 5. Running the Application

The built executables are located in `build/Release/app` (or `build/Debug/app`):

```cmd
.\build\Release\app\sourcetrail_cli.exe
```

With `BUILD_GUI=ON` the GUI is also available:

```cmd
.\build\Release\app\Sourcetrail.exe
```

### 6. Python indexing

The Python source group needs `jedi`, `parso` and `numbat` available to the `python`
executable used to run the indexer:

```cmd
python -m pip install jedi parso numbat
```

The indexer is deployed to `app/data/python/SourcetrailPythonIndexer` and is found
automatically. You can override its location with the `SourcetrailPythonIndexer`
environment variable pointing to the `SourcetrailPythonIndexer` directory.

## Coding Standards

The project uses `.clang-format` and `.clang-tidy` to enforce coding standards. Ensure you have the Clang tools installed (often included with Visual Studio C++ workloads) or configure your IDE to respect these files when formatting C++ code.

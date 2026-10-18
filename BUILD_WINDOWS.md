# Building Neo-Sourcetrail on Windows

This guide explains how to build the Neo-Sourcetrail project on Windows using MSVC (Microsoft Visual C++) and vcpkg.

## Prerequisites

1. **Visual Studio 2022** (or 2019) with the "Desktop development with C++" workload installed.
2. **CMake** (version 3.15 or higher).
3. **Git**.
4. **vcpkg** (C++ Library Manager).

## Step-by-Step Guide

### 1. Install vcpkg
If you haven't installed vcpkg yet, clone it and bootstrap:
```cmd
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```
Make sure the `vcpkg` executable is in your PATH, or note its installation directory.

### 2. Clone the Repository
Clone the Neo-Sourcetrail repository and its submodules (like `SourcetrailPythonIndexer` if applicable):
```cmd
git clone https://github.com/your-org/neo-sourcetrail.git
cd neo-sourcetrail
```

### 3. Configure the Project with CMake
Neo-Sourcetrail uses `vcpkg.json` (manifest mode) to manage dependencies automatically. Run CMake and point it to the vcpkg toolchain file.

```cmd
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake"
```
*Note: Replace `[path_to_vcpkg]` with the actual path where you installed vcpkg.*

### 4. Build the Project
Once configured, you can build the specific targets (e.g., `sourcetrail_cli`) or the entire project using CMake:

```cmd
cmake --build build --target sourcetrail_cli --config Debug
```
*(You can change `Debug` to `Release` for an optimized build).*

### 5. Running the Application
The built executables will be located in the `build/Debug/app` (or `build/Release/app`) directory.

```cmd
.\build\Debug\app\sourcetrail_cli.exe
```

## Coding Standards
The project uses `.clang-format` and `.clang-tidy` to enforce coding standards. Ensure you have the Clang tools installed (often included with Visual Studio C++ workloads) or configure your IDE to respect these files when formatting C++ code.

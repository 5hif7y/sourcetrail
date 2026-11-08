# Phase 6: Hybrid Architecture (Option A + Option B Preparation)

## Overview
Phase 6 ("Separar GUI") aims to decouple the user interface from the heavy language indexers.
We chose to implement an **Intermediate (Hybrid) Architecture**.

This hybrid architecture accomplishes two things simultaneously:
1. **Option A (CLI Delegation)**: The GUI is fully decoupled from direct static linkage to the C++ and Java indexers. Instead, it delegates all indexing work to the `sourcetrail_cli` executable via custom commands (`SourceGroupProxy`). This is already fully functional.
2. **Option B (Shared Library Prep)**: We laid the groundwork for `Sourcetrail_lib` to eventually become a shared library (`libsourcetrail.dll`), which the GUI will consume dynamically.

## Hybrid Implementation Details

### 1. CMake Preparation for Exporting C++ Symbols
In `src/lib/CMakeLists.txt`, we added a CMake option:
```cmake
option(BUILD_SHARED_LIBS_SOURCETRAIL "Build Sourcetrail_lib as a shared library" OFF)
```
When this option is toggled to `ON` in the future, it automatically injects:
- `SOURCETRAIL_SHARED_BUILD` (for the library itself to export symbols).
- `SOURCETRAIL_SHARED_CONSUMER` (for the GUI to import those symbols).

This structural change allows developers to start wrapping internal C++ classes with export macros (e.g., `#ifdef SOURCETRAIL_SHARED_BUILD ... __declspec(dllexport)`) progressively without breaking the current static build.

### 2. Dual Consumption (Static Core + Dynamic API)
In `src/app/CMakeLists.txt`, the `Sourcetrail` GUI target now links against both:
- `Sourcetrail_lib` (statically, for complex Graph and UI components that are not yet exported).
- `sourcetrail_api` (dynamically, for consuming the clean C-API).

As a proof of concept, `src/app/main.cpp` now includes `<libsourcetrail.hpp>` and queries the C-API version dynamically on startup. This proves that the application can successfully load the shared DLL while still using its statically linked core for rendering.

## Future Restructuring (The Road to Full Option B)
When the team is ready to execute the complete restructuring (Option B), the following steps should be taken:
1. Turn `BUILD_SHARED_LIBS_SOURCETRAIL` to `ON`.
2. Define a `SOURCETRAIL_EXPORT` macro in a central header.
3. Apply `SOURCETRAIL_EXPORT` to all classes in `src/lib` that the GUI needs (e.g., `Application`, `Controller`, `View`).
4. Remove static linking of `Sourcetrail_lib` from `src/app/CMakeLists.txt`.

Until then, this hybrid architecture ensures stability while allowing gradual refactoring.

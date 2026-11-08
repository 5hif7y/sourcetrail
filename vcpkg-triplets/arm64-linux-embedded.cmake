# Cross-compilation triplet for 64-bit ARM Linux boards.
# Use it together with a chainloaded toolchain file, e.g.:
#   -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake
#   -DVCPKG_TARGET_TRIPLET=arm64-linux-embedded
#   -DVCPKG_OVERLAY_TRIPLETS=vcpkg-triplets

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CMAKE_SYSTEM_PROCESSOR aarch64)

# Static libraries to avoid shipping a pile of .so files; the C runtime stays
# dynamic (glibc) because fully static glibc builds are fragile.
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CRT_LINKAGE dynamic)

# Embedded images do not need the debug variants.
set(VCPKG_BUILD_TYPE release)

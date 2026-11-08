# Chainload this file with the vcpkg toolchain:
#   -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake
# It expects the GNU ARM64 cross compiler on PATH
# (Debian/Ubuntu: gcc-aarch64-linux-gnu g++-aarch64-linux-gnu).

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

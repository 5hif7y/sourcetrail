# Makefile

# Default target.
all:
.PHONY: all


# ---- Configuration ----
# Installation directory from a binary distribution.
CLANG_LLVM_INSTALL_DIR = $(HOME)/opt/clang+llvm-16.0.0-x86_64-linux-gnu-ubuntu-18.04

# ---- llvm-config query results ----
# Program to query the various LLVM configuration options.
LLVM_CONFIG := $(CLANG_LLVM_INSTALL_DIR)/bin/llvm-config

# C++ compiler options to ensure ABI compatibility.
LLVM_CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags)

# Directory containing the clang library files, both static and dynamic.
LLVM_LIBDIR := $(shell $(LLVM_CONFIG) --libdir)

# Other flags needed for linking, whether statically or dynamically.
LLVM_LDFLAGS_AND_SYSTEM_LIBS := $(shell $(LLVM_CONFIG) --ldflags --system-libs)


# ---- Compiler options ----
# C++ compiler.
#CXX = g++
CXX := $(CLANG_LLVM_INSTALL_DIR)/bin/clang++

# Compiler options, including preprocessor options.
CXXFLAGS =
CXXFLAGS += -Wall
CXXFLAGS += -Werror

# Silence a warning about a multi-line comment in DeclOpenMP.h.
CXXFLAGS += -Wno-comment

# Get llvm compilation flags.
CXXFLAGS += $(LLVM_CXXFLAGS)

# Tell the source code where the clang installation directory is.
CXXFLAGS += -DCLANG_LLVM_INSTALL_DIR='"$(CLANG_LLVM_INSTALL_DIR)"'

# Linker options.
LDFLAGS =

# Pull in clang+llvm via libclang-cpp.so, which has everything, but is
# only available as a dynamic library.
LDFLAGS += -lclang-cpp

# Arrange for the compiled binary to search the libdir for that library.
# Otherwise, one can set the LD_LIBRARY_PATH envvar before running it.
# Note: the -rpath switch does not work on Windows.
LDFLAGS += -Wl,-rpath=$(LLVM_LIBDIR)

# It appears that llvm::raw_os_ostream::~raw_os_ostream is missing from
# libclang-cpp, so I have to link with LLVMSupport statically.
LDFLAGS += -lLLVMSupport

# Get the needed -L search path, plus things like -ldl.
LDFLAGS += $(LLVM_LDFLAGS_AND_SYSTEM_LIBS)


# ---- Recipes ----
# Compile a C++ source file.
%.o: %.cc
    $(CXX) -c -o $@ $(CXXFLAGS) $<

# Executable.
all: implicit-dtors.exe
implicit-dtors.exe: implicit-dtors.o
    $(CXX) -g -Wall -o $@ $^ $(LDFLAGS)

# Test.
.PHONY: run
run: implicit-dtors.exe
    ./implicit-dtors.exe test.cc

.PHONY: clean
clean:
    $(RM) *.o *.exe


# EOF

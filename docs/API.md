# Neo-Sourcetrail Public API

Neo-Sourcetrail provides a public C API (`libsourcetrail`) that allows external applications, plugins, and scripts to query Sourcetrail project databases programmatically without being coupled to the internal C++ architecture.

## Supported Interfaces

1. **C API (`libsourcetrail.h`)**: The canonical, ABI-stable interface. Suitable for FFI across different languages.
2. **C++ Wrapper (`libsourcetrail.hpp`)**: A modern C++ header-only wrapper providing RAII capabilities over the C API.
3. **Python Bindings (`sourcetrail.py`)**: A `ctypes`-based Python wrapper located in `bindings/python`.

## Getting Started (Python Example)

```python
import sourcetrail

# 1. Initialize the library
sourcetrail.init()

# 2. Load a project
try:
    project = sourcetrail.Project("path/to/my_project.srctrlprj")
    
    # 3. Query data
    print(f"Nodes: {project.get_node_count()}")
    print(f"Edges: {project.get_edge_count()}")
    
except RuntimeError as e:
    print(f"Error loading project: {e}")
```

## Getting Started (C++ Example)

```cpp
#include "libsourcetrail.hpp"
#include <iostream>

int main() {
    sourcetrail::init();
    
    try {
        sourcetrail::Project project("path/to/my_project.srctrlprj");
        std::cout << "Nodes: " << project.getNodeCount() << std::endl;
        std::cout << "Edges: " << project.getEdgeCount() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

## Compilation

The API is compiled into a shared library (`sourcetrail.dll`, `libsourcetrail.so`, or `libsourcetrail.dylib`). When integrating with C/C++, ensure you link against this shared library and include the `src/lib_api` headers.

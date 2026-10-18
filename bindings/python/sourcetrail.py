import ctypes
import os
import sys

# Load the library
def _load_lib():
    base_dir = os.path.dirname(__file__)
    # Determine the library name based on the OS
    if sys.platform == 'win32':
        # Default build directory for CMake
        lib_path = os.path.join(base_dir, '..', '..', 'build', 'src', 'lib_api', 'Debug', 'sourcetrail.dll')
        if not os.path.exists(lib_path):
            lib_path = os.path.join(base_dir, '..', '..', 'build', 'src', 'lib_api', 'Release', 'sourcetrail.dll')
    elif sys.platform == 'darwin':
        lib_path = os.path.join(base_dir, '..', '..', 'build', 'libsourcetrail.dylib')
    else:
        lib_path = os.path.join(base_dir, '..', '..', 'build', 'libsourcetrail.so')
    
    if not os.path.exists(lib_path):
        # Fallback to current directory or system path
        lib_path = "sourcetrail.dll" if sys.platform == 'win32' else "libsourcetrail.so"
        
    try:
        if sys.platform == 'win32' and hasattr(os, 'add_dll_directory'):
            os.add_dll_directory(os.path.dirname(os.path.abspath(lib_path)))
        return ctypes.CDLL(lib_path)
    except OSError as e:
        raise RuntimeError(f"Failed to load Sourcetrail library from {lib_path}: {e}")

_lib = _load_lib()

# Define argument and return types
_lib.srctrl_init.argtypes = []
_lib.srctrl_init.restype = None

_lib.srctrl_shutdown.argtypes = []
_lib.srctrl_shutdown.restype = None

_lib.srctrl_project_load.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)]
_lib.srctrl_project_load.restype = ctypes.c_void_p

_lib.srctrl_project_free.argtypes = [ctypes.c_void_p]
_lib.srctrl_project_free.restype = None

_lib.srctrl_project_get_node_count.argtypes = [ctypes.c_void_p]
_lib.srctrl_project_get_node_count.restype = ctypes.c_int

_lib.srctrl_project_get_edge_count.argtypes = [ctypes.c_void_p]
_lib.srctrl_project_get_edge_count.restype = ctypes.c_int

_lib.srctrl_free_string.argtypes = [ctypes.c_void_p]
_lib.srctrl_free_string.restype = None


def init():
    """Initialize the Sourcetrail backend."""
    _lib.srctrl_init()

def shutdown():
    """Shutdown the Sourcetrail backend."""
    _lib.srctrl_shutdown()

class Project:
    def __init__(self, project_path: str):
        """Loads a Sourcetrail project from the given path (.srctrlprj file)."""
        error_ptr = ctypes.c_char_p()
        
        path_bytes = project_path.encode('utf-8')
        self._handle = _lib.srctrl_project_load(path_bytes, ctypes.byref(error_ptr))
        
        if not self._handle:
            error_msg = "Unknown error"
            if error_ptr:
                error_msg = error_ptr.value.decode('utf-8')
                _lib.srctrl_free_string(error_ptr)
            raise RuntimeError(f"Failed to load project '{project_path}': {error_msg}")

    def __del__(self):
        if hasattr(self, '_handle') and self._handle:
            _lib.srctrl_project_free(self._handle)

    def get_node_count(self) -> int:
        """Returns the total number of nodes in the project."""
        return _lib.srctrl_project_get_node_count(self._handle)
        
    def get_edge_count(self) -> int:
        """Returns the total number of edges in the project."""
        return _lib.srctrl_project_get_edge_count(self._handle)


#ifndef LIBSOURCETRAIL_H
#define LIBSOURCETRAIL_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(LIBSOURCETRAIL_EXPORTS)
    #define SRCTRL_API __declspec(dllexport)
  #else
    #define SRCTRL_API __declspec(dllimport)
  #endif
#else
  #define SRCTRL_API __attribute__((visibility("default")))
#endif

// Opaque types
typedef struct srctrl_project_t* srctrl_project;

// Initialize Sourcetrail backend (logging, etc)
SRCTRL_API void srctrl_init(void);

// Shutdown Sourcetrail backend and clean up resources
SRCTRL_API void srctrl_shutdown(void);

// Load a project. Returns NULL on failure.
// If error_msg is provided, it must be freed with srctrl_free_string() if an error occurs.
SRCTRL_API srctrl_project srctrl_project_load(const char* project_file_path, char** error_msg);

// Close and free the project
SRCTRL_API void srctrl_project_free(srctrl_project project);

// Get the node count for the project
SRCTRL_API int srctrl_project_get_node_count(srctrl_project project);

// Get the edge count for the project
SRCTRL_API int srctrl_project_get_edge_count(srctrl_project project);

// Free strings allocated by the API
SRCTRL_API void srctrl_free_string(char* str);

#ifdef __cplusplus
}
#endif

#endif // LIBSOURCETRAIL_H

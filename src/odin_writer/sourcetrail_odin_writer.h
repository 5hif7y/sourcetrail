#ifndef SOURCETRAIL_ODIN_WRITER_H
#define SOURCETRAIL_ODIN_WRITER_H

/*
 * Small C ABI around SourcetrailDBWriter, used by the native Odin indexer.
 *
 * The writer is built from the standalone SourcetrailDB core (bundled SQLite),
 * so the Odin executable does not need C++ or Python at runtime.
 */

#ifdef _WIN32
#  define SODB_API __declspec(dllexport)
#else
#  define SODB_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef void* sodb_writer;

SODB_API sodb_writer sodb_open(const char* database_path);
SODB_API void sodb_close(sodb_writer writer);

SODB_API int sodb_begin(sodb_writer writer);
SODB_API int sodb_commit(sodb_writer writer);

/* full_name is split on delimiter; e.g. "main.User.name" with "." */
SODB_API int sodb_symbol(sodb_writer writer, const char* full_name,
						 const char* delimiter, int symbol_kind);
SODB_API int sodb_symbol_location(sodb_writer writer, int symbol_id, int file_id,
								  int start_line, int start_column,
								  int end_line, int end_column);
SODB_API int sodb_symbol_scope_location(sodb_writer writer, int symbol_id, int file_id,
										int start_line, int start_column,
										int end_line, int end_column);

SODB_API int sodb_reference(sodb_writer writer, int source_id, int target_id,
							int reference_kind);
SODB_API int sodb_reference_location(sodb_writer writer, int reference_id, int file_id,
									 int start_line, int start_column,
									 int end_line, int end_column);
SODB_API int sodb_unsolved_reference(sodb_writer writer, int source_id,
									 int reference_kind, int file_id,
									 int start_line, int start_column,
									 int end_line, int end_column);

SODB_API int sodb_file(sodb_writer writer, const char* path);
SODB_API int sodb_file_language(sodb_writer writer, int file_id, const char* language);

SODB_API int sodb_error(sodb_writer writer, const char* message, int fatal,
						int file_id, int start_line, int start_column,
						int end_line, int end_column);

SODB_API const char* sodb_last_error(sodb_writer writer);

#ifdef __cplusplus
}
#endif

#endif	  // SOURCETRAIL_ODIN_WRITER_H

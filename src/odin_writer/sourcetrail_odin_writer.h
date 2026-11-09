#ifndef SOURCETRAIL_ODIN_WRITER_H
#define SOURCETRAIL_ODIN_WRITER_H

/*
 * Small C ABI around SourcetrailDBWriter, used by the native Odin indexer and
 * by any other indexer that can call C (Rust, C# through P/Invoke, ...).
 *
 * The writer is built from the standalone SourcetrailDB core (bundled SQLite),
 * so an indexer does not need C++ or Python at runtime.
 *
 * ABI v1 is the original set of functions; their behavior does not change.
 * ABI v2 adds the functions after "ABI v2" below. Every v2 function returns 0
 * on failure; sodb_last_error() then says why.
 *
 * Source ranges are 1-based lines and columns, both ends inclusive, as in
 * SourcetrailDB.
 */

/* The library defines SODB_BUILDING; a C or C++ caller that includes this
 * header imports the functions instead. */
#ifdef _WIN32
#  ifdef SODB_BUILDING
#    define SODB_API __declspec(dllexport)
#  else
#    define SODB_API __declspec(dllimport)
#  endif
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

/* ---- ABI v2 ------------------------------------------------------------ */

#define SODB_ABI_VERSION 2

/* The ABI version this library implements; a caller compiled against an
 * older header can check it before using newer functions. */
SODB_API int sodb_abi_version(void);

/* Symbol kinds, in SourcetrailDB order (sourcetrail::SymbolKind). */
enum
{
	SODB_SYMBOL_TYPE = 0,
	SODB_SYMBOL_BUILTIN_TYPE = 1,
	SODB_SYMBOL_MODULE = 2,
	SODB_SYMBOL_NAMESPACE = 3,
	SODB_SYMBOL_PACKAGE = 4,
	SODB_SYMBOL_STRUCT = 5,
	SODB_SYMBOL_CLASS = 6,
	SODB_SYMBOL_INTERFACE = 7,
	SODB_SYMBOL_ANNOTATION = 8,
	SODB_SYMBOL_GLOBAL_VARIABLE = 9,
	SODB_SYMBOL_FIELD = 10,
	SODB_SYMBOL_FUNCTION = 11,
	SODB_SYMBOL_METHOD = 12,
	SODB_SYMBOL_ENUM = 13,
	SODB_SYMBOL_ENUM_CONSTANT = 14,
	SODB_SYMBOL_TYPEDEF = 15,
	SODB_SYMBOL_TYPE_PARAMETER = 16,
	SODB_SYMBOL_MACRO = 17,
	SODB_SYMBOL_UNION = 18,
	/* sodb_symbol_ex only: do not record a kind. */
	SODB_SYMBOL_KIND_NONE = -1
};

/* Reference kinds, in SourcetrailDB order (sourcetrail::ReferenceKind). */
enum
{
	SODB_REFERENCE_TYPE_USAGE = 0,
	SODB_REFERENCE_USAGE = 1,
	SODB_REFERENCE_CALL = 2,
	SODB_REFERENCE_INHERITANCE = 3,
	SODB_REFERENCE_OVERRIDE = 4,
	SODB_REFERENCE_TYPE_ARGUMENT = 5,
	SODB_REFERENCE_TEMPLATE_SPECIALIZATION = 6,
	SODB_REFERENCE_INCLUDE = 7,
	SODB_REFERENCE_IMPORT = 8,
	SODB_REFERENCE_MACRO_USAGE = 9,
	SODB_REFERENCE_ANNOTATION_USAGE = 10
};

/* Definition kinds for sodb_symbol_ex. NONE leaves the symbol non-indexed:
 * the right choice for a symbol that is only referenced, such as one from the
 * standard library or a dependency. A later call for the same symbol with
 * IMPLICIT or EXPLICIT overwrites the earlier kind. */
enum
{
	SODB_DEFINITION_NONE = 0,
	SODB_DEFINITION_IMPLICIT = 1,
	SODB_DEFINITION_EXPLICIT = 2
};

/* One level of a qualified name. prefix and postfix may be NULL (empty); name
 * must not be empty. The postfix carries the signature that tells overloads
 * apart, e.g. {"void", "f", "(int)"} and {"void", "f", "(string)"} are two
 * symbols. */
typedef struct sodb_name_element
{
	const char* prefix;
	const char* name;
	const char* postfix;
} sodb_name_element;

/* Records the symbol whose name is elements[0..element_count), joined by
 * delimiter ("::", "."). Recording the same name again returns the same id.
 * symbol_kind is a SODB_SYMBOL_* value or SODB_SYMBOL_KIND_NONE;
 * definition_kind is a SODB_DEFINITION_* value. Returns the symbol id. */
SODB_API int sodb_symbol_ex(sodb_writer writer, const char* delimiter,
							const sodb_name_element* elements, int element_count,
							int symbol_kind, int definition_kind);

/* The range of the symbol's signature (shown in the code view). */
SODB_API int sodb_symbol_signature_location(sodb_writer writer, int symbol_id, int file_id,
											int start_line, int start_column,
											int end_line, int end_column);

/* A qualifier that names the symbol, e.g. the "Foo" in "Foo::bar()". */
SODB_API int sodb_qualifier_location(sodb_writer writer, int symbol_id, int file_id,
									 int start_line, int start_column,
									 int end_line, int end_column);

/* Marks a reference (an id from sodb_reference) as ambiguous. */
SODB_API int sodb_reference_ambiguous(sodb_writer writer, int reference_id);

/* Records a local symbol (variable, parameter) and returns its id. The name
 * is the identity: recording it again returns the same id, so it must be
 * unique per local, e.g. "<file>:<line>:<column>" of its declaration. */
SODB_API int sodb_local_symbol(sodb_writer writer, const char* name);
SODB_API int sodb_local_symbol_location(sodb_writer writer, int local_symbol_id, int file_id,
										int start_line, int start_column,
										int end_line, int end_column);

/* A range the code view must not split, such as a multi-line comment or
 * string. */
SODB_API int sodb_atomic_source_range(sodb_writer writer, int file_id,
									  int start_line, int start_column,
									  int end_line, int end_column);

#ifdef __cplusplus
}
#endif

#endif	  // SOURCETRAIL_ODIN_WRITER_H

#include "sourcetrail_odin_writer.h"

#include "DefinitionKind.h"
#include "NameHierarchy.h"
#include "ReferenceKind.h"
#include "SourceRange.h"
#include "SourcetrailDBWriter.h"
#include "SymbolKind.h"

#include <string>
#include <vector>

namespace
{
struct Writer
{
	sourcetrail::SourcetrailDBWriter impl;
	// Set when a call rejects its arguments before reaching impl; cleared at
	// the start of every call.
	std::string shimError;
};

// The public SODB_* constants mirror the SourcetrailDB enums by value.
static_assert(SODB_SYMBOL_TYPE == static_cast<int>(sourcetrail::SymbolKind::TYPE), "");
static_assert(SODB_SYMBOL_STRUCT == static_cast<int>(sourcetrail::SymbolKind::STRUCT), "");
static_assert(SODB_SYMBOL_METHOD == static_cast<int>(sourcetrail::SymbolKind::METHOD), "");
static_assert(SODB_SYMBOL_UNION == static_cast<int>(sourcetrail::SymbolKind::UNION), "");
static_assert(SODB_REFERENCE_TYPE_USAGE == static_cast<int>(sourcetrail::ReferenceKind::TYPE_USAGE), "");
static_assert(SODB_REFERENCE_CALL == static_cast<int>(sourcetrail::ReferenceKind::CALL), "");
static_assert(SODB_REFERENCE_IMPORT == static_cast<int>(sourcetrail::ReferenceKind::IMPORT), "");
static_assert(SODB_REFERENCE_ANNOTATION_USAGE ==
				  static_cast<int>(sourcetrail::ReferenceKind::ANNOTATION_USAGE), "");
static_assert(SODB_DEFINITION_IMPLICIT == static_cast<int>(sourcetrail::DefinitionKind::IMPLICIT), "");
static_assert(SODB_DEFINITION_EXPLICIT == static_cast<int>(sourcetrail::DefinitionKind::EXPLICIT), "");

sourcetrail::NameHierarchy makeHierarchy(const char* fullName, const char* delimiter)
{
	const std::string name = fullName ? fullName : "";
	const std::string sep = (delimiter && delimiter[0]) ? delimiter : ".";

	sourcetrail::NameHierarchy hierarchy;
	hierarchy.nameDelimiter = sep;

	size_t start = 0;
	while (true)
	{
		const size_t end = name.find(sep, start);
		const std::string part = name.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (!part.empty())
		{
			sourcetrail::NameElement element;
			element.name = part;
			hierarchy.nameElements.push_back(element);
		}
		if (end == std::string::npos)
		{
			break;
		}
		start = end + sep.size();
	}
	return hierarchy;
}

// Start of a call: the writer, with the previous shim error cleared so that
// sodb_last_error() reports the error of this call.
Writer* asWriter(sodb_writer handle)
{
	Writer* writer = static_cast<Writer*>(handle);
	if (writer)
	{
		writer->shimError.clear();
	}
	return writer;
}

int fail(Writer* writer, const char* message)
{
	writer->shimError = message;
	return 0;
}

int boolToInt(bool value)
{
	return value ? 1 : 0;
}

sourcetrail::SourceRange makeRange(int fileId, int startLine, int startColumn, int endLine, int endColumn)
{
	return sourcetrail::SourceRange{fileId, startLine, startColumn, endLine, endColumn};
}
}	 // namespace

extern "C"
{
SODB_API sodb_writer sodb_open(const char* databasePath)
{
	if (!databasePath || !databasePath[0])
	{
		return nullptr;
	}
	Writer* writer = new Writer();
	// Every database opened through this shim is a disposable per-file or
	// per-batch database that the core merges and discards. Disabling fsyncs
	// and using an in-memory journal removes the dominant indexing cost.
	writer->impl.setFastMode(true);
	// The Odin indexer emits one source location per occurrence. Two elements
	// may share a range, but sharing the row is optional; skipping the indexed
	// lookup removes ~488k SELECTs on the full core and leaves only a few
	// hundred redundant location rows (0.05%).
	writer->impl.setSourceLocationDedup(false);
	if (!writer->impl.open(databasePath))
	{
		delete writer;
		return nullptr;
	}
	return writer;
}

SODB_API void sodb_close(sodb_writer handle)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return;
	}
	writer->impl.close();
	delete writer;
}

SODB_API int sodb_begin(sodb_writer handle)
{
	Writer* writer = asWriter(handle);
	return writer ? boolToInt(writer->impl.beginTransaction()) : 0;
}

SODB_API int sodb_commit(sodb_writer handle)
{
	Writer* writer = asWriter(handle);
	return writer ? boolToInt(writer->impl.commitTransaction()) : 0;
}

SODB_API int sodb_symbol(sodb_writer handle, const char* fullName, const char* delimiter, int symbolKind)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	if (symbolKind < 0 || symbolKind > static_cast<int>(sourcetrail::SymbolKind::UNION))
	{
		return 0;
	}
	const int id = writer->impl.recordSymbol(makeHierarchy(fullName, delimiter));
	if (id == 0)
	{
		return 0;
	}
	writer->impl.recordSymbolKind(id, static_cast<sourcetrail::SymbolKind>(symbolKind));
	writer->impl.recordSymbolDefinitionKind(id, sourcetrail::DefinitionKind::EXPLICIT);
	return id;
}

SODB_API int sodb_symbol_location(sodb_writer handle, int symbolId, int fileId,
								  int startLine, int startColumn, int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordSymbolLocation(
		symbolId, makeRange(fileId, startLine, startColumn, endLine, endColumn)));
}

SODB_API int sodb_symbol_scope_location(sodb_writer handle, int symbolId, int fileId,
										int startLine, int startColumn, int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordSymbolScopeLocation(
		symbolId, makeRange(fileId, startLine, startColumn, endLine, endColumn)));
}

SODB_API int sodb_reference(sodb_writer handle, int sourceId, int targetId, int referenceKind)
{
	Writer* writer = asWriter(handle);
	if (!writer || referenceKind < 0 ||
		referenceKind > static_cast<int>(sourcetrail::ReferenceKind::ANNOTATION_USAGE))
	{
		return 0;
	}
	return writer->impl.recordReference(
		sourceId, targetId, static_cast<sourcetrail::ReferenceKind>(referenceKind));
}

SODB_API int sodb_reference_location(sodb_writer handle, int referenceId, int fileId,
									 int startLine, int startColumn, int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordReferenceLocation(
		referenceId, makeRange(fileId, startLine, startColumn, endLine, endColumn)));
}

SODB_API int sodb_unsolved_reference(sodb_writer handle, int sourceId, int referenceKind,
									 int fileId, int startLine, int startColumn,
									 int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer || referenceKind < 0 ||
		referenceKind > static_cast<int>(sourcetrail::ReferenceKind::ANNOTATION_USAGE))
	{
		return 0;
	}
	return writer->impl.recordReferenceToUnsolvedSymhol(
		sourceId, static_cast<sourcetrail::ReferenceKind>(referenceKind),
		makeRange(fileId, startLine, startColumn, endLine, endColumn));
}

SODB_API int sodb_file(sodb_writer handle, const char* path)
{
	Writer* writer = asWriter(handle);
	return writer && path ? writer->impl.recordFile(path) : 0;
}

SODB_API int sodb_file_language(sodb_writer handle, int fileId, const char* language)
{
	Writer* writer = asWriter(handle);
	if (!writer || !language)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordFileLanguage(fileId, language));
}

SODB_API int sodb_error(sodb_writer handle, const char* message, int fatal, int fileId,
						int startLine, int startColumn, int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordError(
		message ? message : "", fatal != 0,
		makeRange(fileId, startLine, startColumn, endLine, endColumn)));
}

SODB_API const char* sodb_last_error(sodb_writer handle)
{
	Writer* writer = static_cast<Writer*>(handle);
	if (!writer)
	{
		return "No writer.";
	}
	if (!writer->shimError.empty())
	{
		return writer->shimError.c_str();
	}
	return writer->impl.getLastError().c_str();
}

// ---- ABI v2 ---------------------------------------------------------------

SODB_API int sodb_abi_version(void)
{
	return SODB_ABI_VERSION;
}

SODB_API int sodb_symbol_ex(sodb_writer handle, const char* delimiter,
							const sodb_name_element* elements, int elementCount,
							int symbolKind, int definitionKind)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	if (!delimiter || !delimiter[0])
	{
		return fail(writer, "sodb_symbol_ex: the delimiter is empty.");
	}
	if (!elements || elementCount <= 0)
	{
		return fail(writer, "sodb_symbol_ex: the name has no elements.");
	}
	if (symbolKind < SODB_SYMBOL_KIND_NONE || symbolKind > SODB_SYMBOL_UNION)
	{
		return fail(writer, "sodb_symbol_ex: unknown symbol kind.");
	}
	if (definitionKind < SODB_DEFINITION_NONE || definitionKind > SODB_DEFINITION_EXPLICIT)
	{
		return fail(writer, "sodb_symbol_ex: unknown definition kind.");
	}

	sourcetrail::NameHierarchy hierarchy;
	hierarchy.nameDelimiter = delimiter;
	for (int i = 0; i < elementCount; ++i)
	{
		const sodb_name_element& in = elements[i];
		if (!in.name || !in.name[0])
		{
			return fail(writer, "sodb_symbol_ex: a name element is empty.");
		}
		sourcetrail::NameElement element;
		element.prefix = in.prefix ? in.prefix : "";
		element.name = in.name;
		element.postfix = in.postfix ? in.postfix : "";
		hierarchy.nameElements.push_back(element);
	}

	const int id = writer->impl.recordSymbol(hierarchy);
	if (id == 0)
	{
		return 0;
	}
	if (symbolKind != SODB_SYMBOL_KIND_NONE &&
		!writer->impl.recordSymbolKind(id, static_cast<sourcetrail::SymbolKind>(symbolKind)))
	{
		return 0;
	}
	if (definitionKind != SODB_DEFINITION_NONE &&
		!writer->impl.recordSymbolDefinitionKind(
			id,
			definitionKind == SODB_DEFINITION_EXPLICIT ? sourcetrail::DefinitionKind::EXPLICIT
													   : sourcetrail::DefinitionKind::IMPLICIT))
	{
		return 0;
	}
	return id;
}

SODB_API int sodb_symbol_signature_location(sodb_writer handle, int symbolId, int fileId,
											int startLine, int startColumn, int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordSymbolSignatureLocation(
		symbolId, makeRange(fileId, startLine, startColumn, endLine, endColumn)));
}

SODB_API int sodb_qualifier_location(sodb_writer handle, int symbolId, int fileId,
									 int startLine, int startColumn, int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordQualifierLocation(
		symbolId, makeRange(fileId, startLine, startColumn, endLine, endColumn)));
}

SODB_API int sodb_reference_ambiguous(sodb_writer handle, int referenceId)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordReferenceIsAmbiguous(referenceId));
}

SODB_API int sodb_local_symbol(sodb_writer handle, const char* name)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	if (!name || !name[0])
	{
		return fail(writer, "sodb_local_symbol: the name is empty.");
	}
	return writer->impl.recordLocalSymbol(name);
}

SODB_API int sodb_local_symbol_location(sodb_writer handle, int localSymbolId, int fileId,
										int startLine, int startColumn, int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordLocalSymbolLocation(
		localSymbolId, makeRange(fileId, startLine, startColumn, endLine, endColumn)));
}

SODB_API int sodb_atomic_source_range(sodb_writer handle, int fileId, int startLine,
									  int startColumn, int endLine, int endColumn)
{
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return 0;
	}
	return boolToInt(writer->impl.recordAtomicSourceRange(
		makeRange(fileId, startLine, startColumn, endLine, endColumn)));
}
}

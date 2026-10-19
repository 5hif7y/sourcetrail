#include "sourcetrail_odin_writer.h"

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
};

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

Writer* asWriter(sodb_writer handle)
{
	return static_cast<Writer*>(handle);
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
	Writer* writer = asWriter(handle);
	if (!writer)
	{
		return "No writer.";
	}
	return writer->impl.getLastError().c_str();
}
}

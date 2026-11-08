import numbat.api as numbat
from numbat.api import NodeType, EdgeType, SymbolType

# Global instance of SourcetrailDB from numbat
_db = None
_last_error = ""

# Enums - Symbol Types (maps to NodeType in numbat)
SYMBOL_TYPE = NodeType.NODE_TYPE.value
SYMBOL_BUILTIN_TYPE = NodeType.NODE_BUILTIN_TYPE.value
SYMBOL_MODULE = NodeType.NODE_MODULE.value
SYMBOL_NAMESPACE = NodeType.NODE_NAMESPACE.value
SYMBOL_PACKAGE = NodeType.NODE_PACKAGE.value
SYMBOL_STRUCT = NodeType.NODE_STRUCT.value
SYMBOL_CLASS = NodeType.NODE_CLASS.value
SYMBOL_INTERFACE = NodeType.NODE_INTERFACE.value
SYMBOL_ANNOTATION = NodeType.NODE_ANNOTATION.value
SYMBOL_GLOBAL_VARIABLE = NodeType.NODE_GLOBAL_VARIABLE.value
SYMBOL_FIELD = NodeType.NODE_FIELD.value
SYMBOL_FUNCTION = NodeType.NODE_FUNCTION.value
SYMBOL_METHOD = NodeType.NODE_METHOD.value
SYMBOL_ENUM = NodeType.NODE_ENUM.value
SYMBOL_ENUM_CONSTANT = NodeType.NODE_ENUM_CONSTANT.value
SYMBOL_TYPEDEF = NodeType.NODE_TYPEDEF.value
SYMBOL_TYPE_PARAMETER = NodeType.NODE_TYPE_PARAMETER.value
SYMBOL_FILE = NodeType.NODE_FILE.value
SYMBOL_MACRO = NodeType.NODE_MACRO.value
SYMBOL_UNION = NodeType.NODE_UNION.value

# Enums - Reference Types (Edges)
REFERENCE_TYPE_USAGE = EdgeType.TYPE_USAGE.value
REFERENCE_USAGE = EdgeType.USAGE.value
REFERENCE_CALL = EdgeType.CALL.value
REFERENCE_INHERITANCE = EdgeType.INHERITANCE.value
REFERENCE_OVERRIDE = EdgeType.OVERRIDE.value
REFERENCE_TYPE_ARGUMENT = EdgeType.TYPE_ARGUMENT.value
REFERENCE_TEMPLATE_SPECIALIZATION = EdgeType.TEMPLATE_SPECIALIZATION.value
REFERENCE_INCLUDE = EdgeType.INCLUDE.value
REFERENCE_IMPORT = EdgeType.IMPORT.value
REFERENCE_MACRO_USAGE = EdgeType.MACRO_USAGE.value
REFERENCE_ANNOTATION_USAGE = EdgeType.ANNOTATION_USAGE.value

# Enums - Definition Kinds (Explicit / Implicit)
DEFINITION_IMPLICIT = SymbolType.IMPLICIT.value
DEFINITION_EXPLICIT = SymbolType.EXPLICIT.value

# Methods
def open(path):
    global _db, _last_error
    try:
        from pathlib import Path
        p = Path(path)
        if not p.exists():
            _db = numbat.SourcetrailDB.create(path)
        else:
            _db = numbat.SourcetrailDB.open(path)
        return True
    except Exception as e:
        _last_error = str(e)
        return False

def close():
    global _db, _last_error
    if _db is None:
        return True
    try:
        _db.close()
        _db = None
        return True
    except Exception as e:
        _last_error = str(e)
        return False

def clear():
    global _db, _last_error
    if _db is None:
        return False
    try:
        _db.clear()
        return True
    except Exception as e:
        _last_error = str(e)
        return False

def isEmpty():
    return False

def beginTransaction():
    pass

def commitTransaction():
    if _db:
        _db.commit()

def getLastError():
    return getattr(_db, "_last_error", _last_error)

def getVersionString():
    return "v4.db25.p1"

def isCompatible():
    return True

def getSupportedDatabaseVersion():
    return 1

def getLoadedDatabaseVersion():
    return 1

def recordSymbol(serialized_name):
    import json
    data = json.loads(serialized_name)
    delimiter = data.get("name_delimiter", ".")
    elements = []
    for el in data.get("name_elements", []):
        elements.append(numbat.NameElement(el.get("prefix", ""), el.get("name", ""), el.get("postfix", "")))
    name_hierarchy = numbat.NameHierarchy(delimiter, elements)
    return _db._record_symbol(name_hierarchy)

def recordSymbolDefinitionKind(symbolId, kind):
    _db._record_symbol_definition_kind(symbolId, numbat.SymbolType(kind))

def recordSymbolKind(symbolId, kind):
    _db._record_symbol_kind(symbolId, numbat.NodeType(kind))

def recordSymbolLocation(symbolId, fileId, line, col, endLine, endCol):
    _db.record_symbol_location(symbolId, fileId, line, col, endLine, endCol)

def recordSymbolScopeLocation(symbolId, fileId, line, col, endLine, endCol):
    _db.record_symbol_scope_location(symbolId, fileId, line, col, endLine, endCol)

def recordSymbolSignatureLocation(symbolId, fileId, line, col, endLine, endCol):
    _db.record_symbol_signature_location(symbolId, fileId, line, col, endLine, endCol)

def recordReference(sourceId, targetId, kind):
    return _db._record_reference(sourceId, targetId, numbat.EdgeType(kind))

def recordReferenceLocation(refId, fileId, line, col, endLine, endCol):
    _db.record_reference_location(refId, fileId, line, col, endLine, endCol)

def recordReferenceIsAmbiguous(refId):
    _db.record_reference_is_ambiguous(refId)
    return True

def recordReferenceToUnsolvedSymhol(sourceId, kind, fileId, line, col, endLine, endCol):
    _db.record_reference_to_unsolved_symbol(sourceId, numbat.EdgeType(kind), fileId, line, col, endLine, endCol)
    return True

def recordQualifierLocation(symbolId, fileId, line, col, endLine, endCol):
    _db.record_qualifier_location(symbolId, fileId, line, col, endLine, endCol)
    return True

import os
class ForwardSlashPath(os.PathLike):
    def __init__(self, p):
        import pathlib
        self.p = pathlib.Path(p)
    def exists(self): return self.p.exists()
    def is_file(self): return self.p.is_file()
    def absolute(self): return self
    def __str__(self): return self.p.absolute().as_posix()
    def __fspath__(self): return self.p.absolute().as_posix()

def recordFile(path):
    return _db.record_file(ForwardSlashPath(path))

def recordFileLanguage(fileId, language):
    _db.record_file_language(fileId, language)

def recordLocalSymbol(name):
    return _db.record_local_symbol(name)

def recordLocalSymbolLocation(symbolId, fileId, line, col, endLine, endCol):
    _db.record_local_symbol_location(symbolId, fileId, line, col, endLine, endCol)

def recordAtomicSourceRange(fileId, line, col, endLine, endCol):
    _db.record_atomic_source_range(fileId, fileId, line, col, endLine, endCol)

def recordError(message, fatal, fileId, line, col, endLine, endCol):
    _db.record_error(message, fatal, fileId, line, col, endLine, endCol)


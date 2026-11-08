# Vendored SourcetrailDB writer core

Source: `sourcetrail-db` v4.db25.p1 (Apache-2.0, see `LICENSE.txt`).
Only the writer core (`core/include`, `core/src`, `version.h.in`) and the
bundled C++ SQLite wrapper (`external/cpp_sqlite`) plus its JSON headers are
included. Tests, examples and bindings are not.

## Local modifications

- `DatabaseStorage::openDatabase(path, fastMode)`: in fast mode it executes
  `PRAGMA synchronous=OFF` on the connection. This is intended for the
  disposable per-thread databases created by the Sourcetrail core, which are
  merged and removed after indexing. The journal mode is left alone: the core
  creates the temp databases in WAL mode and changing it breaks visibility for
  the core's already-open connection.
- `DatabaseStorage::applyFastModePragmas()` additionally disables foreign
  keys, stops WAL autocheckpointing, and sets a 64 MB cache with
  `temp_store=MEMORY`.
- `SourcetrailDBWriter::setFastMode(bool)` forwards the flag to the storage.
- `DatabaseStorage::setSourceLocationDedup(bool)` and
  `SourcetrailDBWriter::setSourceLocationDedup(bool)`: when disabled,
  `addSourceLocation()` skips the indexed lookup and always inserts. Ranges
  shared by different elements are valid but do not need to share a row. The
  default remains `true`; the Odin shim disables it because its indexer emits
  one location per occurrence.
- The Odin writer shim enables fast mode and disables source-location dedup
  for every database it opens.

These changes are deliberately small so the vendored copy can be refreshed
from a newer `sourcetrail-db` release with minimal conflict resolution.

To build against a standalone checkout instead:

```powershell
.\src\odin_indexer\build.ps1 -SourcetrailDB <path-to-sourcetrail-db>
# or
cmake -S src/odin_writer -B build/odin_writer -DSOURCETRAILDB_DIR=<path>
```

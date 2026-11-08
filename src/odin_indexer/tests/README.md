# Native Odin indexer tests

These tests exercise the Odin executable directly (no Qt, no Sourcetrail CLI).

## Run

```powershell
# Build the writer and the indexer first:
.\src\odin_indexer\build.ps1 -Odin "C:\Software\Odin-oct-2026\odin.exe"

.\src\odin_indexer\tests\run_tests.ps1 `
  -Indexer .\build\odin_indexer\SourcetrailOdinIndexer.exe
```

`run_tests.ps1` accepts `-WorkDir` and `-Python` (defaults to `python` or `py -3`).
The script needs Python only for the SQLite assertions in `check_db.py`; the
indexer itself does not.

## Coverage

| Test | What it checks |
|---|---|
| Sample batch vs per-file | The 3-file sample produces the same node and edge sets when indexed in one process and when each file is indexed separately into the same database. |
| Member chains | `g.x`, `f.p.y` resolve to the declared `g` / `f` symbols instead of inventing `g.x`, `f.p`, `f.p.y`. |
| Unresolved references | They are absent by default and become one `unsolved symbol` node with `--unsolved`. |
| Attributes/directives/foreign | `@private`, `#force_inline`, `foreign libc { ... --- }`, struct `where` and generics keep their declarations. |
| Proc types | Parameter names inside `proc(...)` field types and proc literals are not references (checked with `--unsolved`). |
| Determinism | Two batch runs produce identical node and edge sets. |

`check_db.py` also has a `compare` mode and a `check` mode useful for ad-hoc
audits:

```powershell
py -3 src\odin_indexer\tests\check_db.py compare db-a.srctrldb db-b.srctrldb
py -3 src\odin_indexer\tests\check_db.py check db.srctrldb `
  --require main.User,main.greet --forbid main.User.name --unsolved 0
```

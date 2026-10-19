# Parser Odin: cobertura final

**Fecha:** 2026-10-19
**Árbol canónico:** `_work\modernized`
**Base:** `ead450e` (docs 31)
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`

---

## 1. Correcciones

| Defecto | Efecto | Corrección |
|---|---|---|
| `Name :: #type proc(...)` multilínea | `skip_to_nl` sólo consumía la primera línea; los parámetros se parseaban como declaraciones top-level y los resultados con nombre (`err`) como referencias. Se creaban 62 símbolos fantasma | `skip_type_alias` consume el tipo completo y además escanea los tipos de parámetros/resultados con `scan_proc_type` |
| `using header` sobre un campo de struct | Los campos (`width`, `height`, …) quedaban como referencias no resueltas | Se infiere el tipo local desde casts/anotaciones (`(^Type)`, `cast(^Type)`, `x: Type`) y se buscan los campos en la tabla local, del paquete importado o del paquete externo (`core:image`) |
| `#config(KEY, default)` | `KEY` se registraba como referencia | Sólo se escanea la expresión por defecto |
| `@thread_local` | El nombre del atributo se escaneaba como referencia | Los atributos sin paréntesis consumen su nombre |
| `where` en un tipo `proc` anidado | `$T`/`T` se resolvían fuera del scope | La cláusula `where` se escanea con los genéricos del proc |
| `append_elems`, `clear_map`, `remove_range` | Builtins no listados | Agregados a `is_builtin_proc` |

---

## 2. Resultado

Core actual (`C:\Software\Odin-oct-2026\core`, **1.288 archivos**):

| Métrica | Antes (31 no resueltas) | Después |
|---|---:|---:|
| Ocurrencias no resueltas | 31 | **7** |
| Nodos (por defecto) | 115.772 | **115.710** |
| Aristas (por defecto) | 314.441 | **314.572** |
| Nodos (con `--unsolved`) | 115.773 | **115.711** |
| Aristas (con `--unsolved`) | 314.454 | **314.575** |
| Errores | 0 | **0** |
| Indexado | ~19 s | **~19,7 s** |

Los 62 nodos menos son símbolos fantasma que el bug de `#type` creaba. Las
aristas suben porque los tipos de parámetros/resultados del alias ahora sí se
escanean.

### Las 7 restantes son defectos del fuente, no del parser

- 4 en `simd/x86/avx.odin`: el core declara `$IIM4` y usa `IMM4`.
- 2 en `math/linalg/general.odin`: `matrix_inverse_generic` no existe en core.
- 1 en `thread/thread_pool.odin`: `pop_front` se usa sin calificar, pero el
  import de `core:container/queue` no es `using`.

Para código Odin válido, la cobertura de referencias queda completa en la
práctica.

---

## 3. Tests y gates

- `src/odin_indexer/tests/run_tests.ps1`: **9/9**.
  - Fixture `parser_edges.odin`: alias `#type proc` multilínea, `using`
    sobre un struct local, `#config`, `@thread_local` y builtins.
  - Fixture `core/test/using_external.odin`: `using` sobre un campo de
    `core:image`, para cubrir la búsqueda de campos en paquetes externos.
- Odin sample desktop / embedded: **32 / 42 / 0**.
- `tictactoe_cpp`: **383 / 819 / 0**.
- `tutorial-py`: **103 / 281 / 0**.
- Core completo: **115.710 / 314.572 / 0**, ~19,7 s.
- Graphify: **27.762 nodos / 81.597 aristas / 813 comunidades** (sin cambios
  topológicos en esta pasada).

---

## 4. Writer: control de page_size

Se probó `PRAGMA PAGE_SIZE=8192` al crear la base temporal (antes de las
tablas). El core completo quedó en 18,2–21,5 s, solapado con el baseline de
18,7–20,0 s; sin mejora medible. El cambio se revirtió. Con esto quedan
descartados índices, caché, `page_size` y tamaño de lote; el coste restante es
volumen de filas/páginas.

---

## 5. Pendientes

- Los 7 símbolos anteriores son bugs de `core` de Odin; no se corrigen desde el
  indexador.
- **Writer**: reducir volumen de filas/páginas; índices, caché, `page_size` y
  tamaño de lote ya descartados.
- **E5 (ARM64)**: **no se hará**. El usuario no dispone de ninguna máquina
  ARM64 a mano (confirmado 2026-10-19). El triplet/toolchain queda documentado
  por si cambia el hardware.
- **Historial**: purge opcional con aprobación explícita.
- **GUI**: recorrido manual del wizard.

---

## 5. Reproducir

```powershell
.\src\odin_indexer\build.ps1 -Odin "C:\Software\Odin-oct-2026\odin.exe"
.\src\odin_indexer\tests\run_tests.ps1 `
  -Indexer .\build\odin_indexer\SourcetrailOdinIndexer.exe

build\Release\app\sourcetrail_cli.exe `
  bin\app\user\projects\odin-sample\odin-sample.srctrlprj --refresh=all
```

Auditoría del core: `.srctrlprj` con `Empty Odin Source Group` y
`source_path` al core, `SOURCETRAIL_ODIN_UNSOLVED=1`, y consulta del nodo
`unsolved symbol` en el `.srctrldb`.

---

## 6. Seguridad

- Sin rutas personales en archivos rastreados; usar `<WORKSPACE>`/`<STUDY>`.
- Sin binarios rastreados.
- Commits con `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

# Builtins del compilador en el parser Odin

**Fecha:** 2026-10-19
**Árbol canónico:** `_work\modernized`
**Base:** `34108fa9` (docs 33)
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`

---

## 1. Reproducción

El doc 32 dejó anotadas **7** referencias no resueltas en el core. Al
reproducir sobre el checkout actual de `_study\Odin` (**1.366 archivos**, no
1.288) aparecieron **4**:

| Archivo | Referencia | Diagnóstico |
|---|---|---|
| `core/encoding/xml/xml_reader.odin:470` | `make_dynamic_array` | builtin del compilador |
| `core/math/linalg/general.odin:1193,1200` | `matrix_inverse_generic` | símbolo inexistente en `core` |
| `core/thread/thread_pool.odin:364` | `pop_front` | builtin del compilador |

Las cuatro `$IMM4`/`IMM4` del doc 32 **ya no aparecen**: el parser marca los
parámetros `$Nombre` como locales (`mark_generic_names` y
`process_param_group`), así que `IMM4` resuelve dentro del cuerpo del proc.

`make_dynamic_array` y `pop_front` están declarados con `@builtin` en
`base/runtime/core_builtin.odin`; no son símbolos de paquete. El parser sólo
tenía una lista parcial de builtins, por eso los trataba como no resueltos.

`matrix_inverse_generic` no está definido en ningún archivo del checkout
(`grep -rn` sobre todo `_study\Odin`): es un defecto real del fuente de `core`.
Las funciones existentes son `matrix_inverse_gauss_jordan` y
`matrix_inverse_lu_decomposition`.

---

## 2. Cambio

`is_builtin_proc` en `src/odin_indexer/main.odin` ahora cubre **todos** los
procedimientos marcados `@builtin` en `base/runtime/core_builtin*.odin`,
además de los builtins que ya tenía. Eso incluye:

- `make_dynamic_array`, `make_dynamic_array_len`,
  `make_dynamic_array_len_cap`, `make_map`, `make_map_cap`,
  `make_multi_pointer`, `make_soa*`, `make_aligned`;
- `pop_front`, `pop_front_safe`, `pop_safe`, `pop_dynamic_array`,
  `pop_soa`, y sus variantes;
- `assign_at*`, `inject_at*`, `append_*` de arrays, strings y SOA;
- `delete_*` de arrays, slices, strings, maps y SOA;
- `non_zero_*`, `reserve_*`, `resize_*`, `shrink_*`, `clear_*`,
  `ordered_remove_*`, `unordered_remove_*`, `remove_range_*`,
  `map_upsert`, `copy_from_string*`.

La lista se contrastó automáticamente contra las declaraciones `@builtin`
del checkout de Odin, así que no es una enumeración ad hoc.

---

## 3. Test

Nuevo fixture `src/odin_indexer/tests/fixtures/builtins.odin` que usa
`make_dynamic_array`, `pop_front`, `assign_at`, `make_map`, `delete_map`,
`map_upsert`, `make_multi_pointer` y `delete_dynamic_array`. El test
`compiler builtins resolve without package symbols` exige `--unsolved 0` y
prohíbe que esos nombres aparezcan como símbolos del paquete.

```
PASS compiler builtins resolve without package symbols
All Odin indexer tests passed.
```

Suite completa: **10/10**.

---

## 4. Verificación sobre el core

| Medición | Antes | Después |
|---|---:|---:|
| Archivos | 1.366 | 1.366 |
| No resueltas (`--unsolved --debug-refs`) | 4 | **2** |
| Nodos | — | 133.440 |
| Aristas | — | 356.761 |
| Errores | 0 | **0** |
| Tiempo limpio (lotes de 128) | — | **36 s** |

Las 2 restantes son las dos apariciones de `matrix_inverse_generic` en
`math/linalg/general.odin`; no se corrigen desde el indexador.

Desglose con `--timing` (11 lotes): **7,7 s de parseo, 4,9 s de referencias y
30,6 s de commit** sobre 34,5 s de suma por lote. El commit del writer es
~89 % del tiempo; la lista de builtins no mueve la aguja y el coste sigue en
el volumen de filas.

> Los números de nodos/aristas no son comparables con el corte de 1.288
> archivos del doc 32 porque cambió el conjunto de archivos del checkout.

---

## 5. Pendientes

- **Core de Odin**: `matrix_inverse_generic` inexistente en `linalg`; las 2
  no resueltas restantes son de `core`, no del parser.
- **Writer**: volumen de filas/páginas; parámetros de SQLite, índices y
  tamaños de lote ya descartados con mediciones.
- **E5 (ARM64)**: fuera de alcance; no hay hardware ARM64.
- **GUI**: recorrido manual del wizard.
- **Historial**: cerrado (doc 33).

---

## 6. Reproducir

```powershell
.\src\odin_indexer\build.ps1 -Odin "C:\Software\Odin-oct-2026\odin.exe"
.\src\odin_indexer\tests\run_tests.ps1 `
  -Indexer .\build\odin_indexer\SourcetrailOdinIndexer.exe
```

Para el core completo, en lotes de 128 archivos:

```powershell
$files = Get-ChildItem <STUDY>\Odin\core -Recurse -Filter *.odin |
  Select-Object -ExpandProperty FullName
# ejecutar el indexador con 128 --source-file-path por proceso y la misma
# --database-file-path; --unsolved --debug-refs imprime las no resueltas.
```

# F5 parcial, parser Odin y experimento de writer

**Fecha:** 2026-10-19
**Árbol canónico:** `_work\modernized`
**Base:** `71c1dec` (docs 28)
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`

---

## 1. F5 parcial: filtros de mensajes como efecto

### Cambios

- `MessageQueue::removeMessageFilter()`: inversa de `addMessageFilter`, por
  igualdad de `shared_ptr`.
- `src/lib/project/CorePlugin.*`: manifiesto `core` (`unloadable=false`) que
  registra `MessageFilterErrorCountUpdate`, `MessageFilterFocusInOut` y
  `MessageFilterSearchAutocomplete` dentro de un `ctx.effect`. El dispose los
  elimina en orden inverso.
- `Application::startMessagingAndScheduling()` ya no agrega los filtros a mano:
  llama a `st::plugin::ensureCorePluginLoaded()` antes de arrancar el loop.
- `PluginHost.h` hace `#undef ERROR` / `#undef IGNORE` después de
  `<windows.h>`; los enums `LocationType::ERROR` y `TabId::IGNORE` chocaban
  cuando un header del core se incluía después del host.

### Verificación

Los gates de indexado siguen pasando (sección 4). No se agregó un test unitario
de `removeMessageFilter`: el suite de mensajes requiere `BUILD_UNIT_TESTS_PACKAGE`
(Qt + Catch2), que no está configurado en este build. El camino de dispose se
ejercita en `plugin_host_test` con registros genéricos.

### Pendiente de F5

- Acciones de menú/toolbar, páginas de preferencias y contenido del wizard como
  efectos reversibles.
- Listeners de mensajes (`registerListener`/`unregisterListener` ya existe, pero
  los objetos Qt se registran en sus constructores).
- Un `unload` real del plugin `core` no se usa: el loop de mensajes es de
  proceso completo; el efecto queda como inversa disponible y probado por el
  modelo general.

---

## 2. Parser Odin: segunda pasada

### Defectos corregidos

| Defecto | Efecto | Corrección |
|---|---|---|
| Cadenas `""" ... """` | El lexer cerraba en la segunda `"` y parseaba el C++/texto embebido como Odin; 68 referencias falsas en `cpp-gen.odin` | Rama específica para raw strings triples, emitidas como un solo token `.Raw` |
| Alias de enum (`Blue = Green`) | `Green` se resolvía como `paquete.Green`, no como `Enum.Green` | `scan_enum_alias_expr`: variantes hermanas como locales y referencia explícita a `owner.variant` |
| `offset_of(Type, field)` | `field` se registraba como referencia al paquete | Sólo se escanea el primer argumento; `offset_of` es keyword, por lo que el chequeo va antes del guard de `.Ident` |
| Nombres de imports desnudos (`_, _, _ :: intrinsics, strconv, math`) | Referencias no resueltas | Se omiten los nombres locales de imports cuando no van seguidos de `.` |
| Locales que ensombrecen builtins (`real`, `min`, ...) | El builtin se saltaba antes de consultar `locals`, y la cadena siguiente se resolvía contra el paquete | `locals` se consulta antes de `is_builtin_proc` |

### Resultado

Core actual (`C:\Software\Odin-oct-2026\core`, **1.288 archivos**):

| Métrica | Antes | Después |
|---|---:|---:|
| Ocurrencias no resueltas (`--unsolved`) | 182 | **41** |
| Nodos (con no resueltas) | 115.773 | **115.773** |
| Aristas (con no resueltas) | 314.465 | **314.456** |
| Errores | 0 | **0** |

Sin no resueltas: **115.772 nodos, 314.435 aristas, 18,7 s**, 0 errores.

Las 41 restantes:

- 15 en `image/png/png.odin`: campos traídos por `using header`, donde el
  parser no infiere el tipo del puntero.
- 4 en `simd/x86/avx.odin`: el core declara `$IIM4` pero usa `IMM4` (typo real
  del fuente).
- 3 en `fmt/fmt.odin`: `$T` en el `where` de un proc anidado.
- 2 en `math/linalg/general.odin`: `matrix_inverse_generic` no existe en core.
- 2 en `nbio/impl_linux.odin`, y el resto casos aislados.

Tests: **7/7** (`run_tests.ps1`). Se agregó el fixture
`strings_enum.odin`, que cubre raw strings triples, alias de enum y
`offset_of`; verifica que no aparezcan símbolos fantasma y que el alias apunte
a `Color.Green`.

---

## 3. Experimento descartado: lista de archivos por source group

Se implementó y midió una variante con `--source-file-list` (un proceso y un
commit por source group), y se revirtió. El costo del commit crece de forma
superlineal con el tamaño del lote:

| Archivos por commit | Commit | Total del proceso |
|---:|---:|---:|
| 128 | 0,94 s | 1,02 s |
| 256 | 1,54 s | 1,63 s |
| 512 | 3,66 s | 3,74 s |
| 1.024 | 18,80 s | 18,87 s |
| 1.288 | **29,91 s** | **29,92 s** |

El core completo con un solo proceso tardó **37,6 s de pared**, frente a
**~19 s** con lotes de 128. También se probó un lote de **256** en el core
completo: **18,8 s** frente a **18,7 s** con 128, sin mejora medible. Conclusión:
los lotes de 128 ya están en el óptimo práctico; eliminar los commits empeora el
tiempo y duplicar el lote no aporta. El siguiente paso, si se retoma, es reducir
índices/escrituras durante el indexado, no cambiar el tamaño del lote. El código
del experimento no quedó en el árbol.

---

## 4. Verificación

| Prueba | Resultado |
|---|---|
| Odin sample desktop | **32 / 42 / 0** |
| Odin sample embedded | **32 / 42 / 0** |
| `tictactoe_cpp` | **383 / 819 / 0** |
| `tutorial-py` | 103 / 270–283 / 0 (varía entre corridas) |
| Core Odin (1.288 archivos) | **115.772 / 314.435 / 0**, 18,7 s |
| Core Odin con `--unsolved` | **115.773 / 314.456 / 0**, 41 no resueltas |
| Indexador Odin | **7/7** |
| Plugins | **3/3** |
| Registro de lenguajes | **1/1** |
| Graphify | **27.620 nodos / 81.307 aristas / 784 comunidades** |

Nota: `tutorial-py` varía unas pocas aristas entre corridas (270–283) con 0
errores; el baseline histórico también osciló (272–394). No se atribuye a estos
cambios.

---

## 5. Wizard Odin (T4)

Revisión de código, sin recorrido interactivo:

- `QtProjectWizardContentSelect` lista Odin como tipo recomendado
  (`SourceGroupType::ODIN_EMPTY`).
- `QtProjectWizard::selectedProjectType` crea `SourceGroupSettingsOdinEmpty`.
- Al derivar de `SourceGroupSettingsCustomCommand`, el wizard reutiliza el
  contenido de comando personalizado (comando, rutas, extensiones).

Falta el recorrido manual del diálogo en la GUI; el camino de datos
(settings → factory → indexado) está cubierto por el CLI y el sample.

---

## 6. Pendiente

- **F5**: acciones/páginas de GUI y listeners de mensajes.
- **E5**: cross-build ARM64.
- **Historial**: purge opcional (aprobación explícita).
- **GUI**: recorrido manual del wizard Odin.
- **Parser**: 41 no resueltas; las 15 de `png` requieren inferencia de tipos
  para `using header`.
- **Writer**: reducir índices durante la escritura o tantear lotes de 64/256;
  la lista de archivos está descartada con medición.

---

## 7. Reproducir

```powershell
.\src\odin_indexer\build.ps1 -Odin "C:\Software\Odin-oct-2026\odin.exe"
.\src\odin_indexer\tests\run_tests.ps1 `
  -Indexer .\build\odin_indexer\SourcetrailOdinIndexer.exe

cmake --build build --config Release --target sourcetrail_cli
build\Release\app\sourcetrail_cli.exe `
  bin\app\user\projects\odin-sample\odin-sample.srctrlprj --refresh=all

cmake --build build-embedded --config Release --target sourcetrail_cli
build-embedded\Release\app\sourcetrail_cli.exe `
  bin\app\user\projects\odin-sample\odin-sample.srctrlprj --refresh=all
```

Auditoría de no resueltas sobre el core: crear un `.srctrlprj` con
`<type>Empty Odin Source Group</type>` y `source_path` al core, exportar
`SOURCETRAIL_ODIN_UNSOLVED=1` y consultar el nodo `unsolved symbol` en el
`.srctrldb`.

---

## 8. Seguridad

- Sin rutas personales en archivos rastreados; usar `<WORKSPACE>`/`<STUDY>`.
- Sin binarios rastreados.
- Commits con `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

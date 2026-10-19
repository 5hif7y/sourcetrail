# Auditoría del indexador Odin, batch, F2 y F3

**Fecha:** 2026-10-19
**Árbol de trabajo:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`
**Base de la sesión:** `ef54801a` (verificado con `git ls-remote`; el clon local
`neo-sourcetrail` está en `master` y sucio por diseño).

> **Actualización final (segunda pasada):** la cobertura no resuelta del `core`
> completo bajó de 21.169 a **178 ocurrencias** (0,05 %); el writer quedó
> **vendorizado** con modo rápido y el `core` se indexa en **19,0 s** (23,6 s de
> pared) con los mismos nodos/aristas. Los tests del indexador son **6/6**. Las
> secciones 2–3 conservan las cifras del corte anterior; la segunda pasada está
> en la sección 11.

---

## 1. Auditoría del estado

- El remoto canónico `origin/modernized` estaba en `ef54801a`; el árbol local
  `neo-sourcetrail` no tenía remoto ni rama `modernized`, pero su contenido
  coincidía con el fork (`git diff fork/modernized` vacío para los archivos
  tocados). Se clonó una copia limpia en `_work\modernized` para commitear.
- Escaneo de archivos rastreados (incluye UTF-16): **0 identificadores
  personales**. Las rutas de usuario que quedan son ejemplos genéricos de
  upstream (`DOCUMENTATION.md`, `NetworkProtocolHelperTestSuite.cpp`).
- **0 binarios rastreados** (`.exe/.dll/.lib/.pdb/.obj`); los binarios del
  indexador están en `.gitignore`.
- Los blobs viejos con `error.log`/`build_log.txt` siguen en el historial
  (base `5f735e9e`); el purge con `git filter-repo` sigue siendo opcional y
  requiere force-push.
- Graphify regenerado: **27.578 nodos, 81.243 aristas, 763 comunidades**
  (incluye el writer vendorizado y la C ABI).

---

## 2. Indexador Odin: corrección

### 2.1 Defectos encontrados y corregidos

| Defecto | Efecto | Corrección |
|---|---|---|
| `resolve()` creaba `paquete.variable.campo.campo` para cadenas de miembros | Símbolos fantasma (`globals.f.p`, `globals.f.p.y`) | Resolución por **prefijo declarado más largo**; el destino es `globals.f` |
| Referencias no resueltas se inventaban como símbolos | Navegación a símbolos que no existen | Se omiten por defecto; `--unsolved` las registra como `unsolved symbol` |
| `skip_attribute` no consumía `@private`, `@export`, etc. | La declaración real se perdía y quedaba un símbolo `private` fantasma | Consume el nombre del atributo (sin paréntesis) |
| `name :: #force_inline proc ...` | La declaración se descartaba | Los directives con `proc`/tipos se parsean; el resto se registra como global |
| `---` se lexa como tres `-` | El resultado de un `proc` foreign consumía el resto del archivo | `collect_result_tokens` corta en el primer `-` |
| Resultados con nombre `(n: int, err: Error)` | `n`/`err` se escaneaban como tipos (no resueltos) y el cuerpo no se parseaba si había `#optional_ok` | `scan_result_tokens`, corte en `#`, salto de directivas sin consumir la línea |
| `struct($T: type) where ... {` | El `where` impedía entrar al cuerpo; los campos se perdían | `skip_until_body` y parámetros genéricos en el parser |
| `for i, v in ...` | `for` es keyword y el marcado de locales se saltaba | El chequeo de `for` va antes del filtro de identificadores; soporta `&x` |
| Parámetros con default sin tipo (`allocator := context.allocator`) | Se marcaban como referencias | `split_params` separa parámetros con `:=`/`=` |
| `dynamic`, `cstring16`, `u32le`, `intrinsics`, `builtin`, `assert`, `panic`, `ensure_contextless`, `resize`, `unreachable` | Referencias no resueltas | Listas de builtins/tipos |
| Imports externos usados en tipos | `bits.add_u64`, `fmt.println`, `CF.StringRef` no resolvían | `resolve_ref_target` reconoce prefijos de imports externos |
| `X := ...` a nivel de paquete | La variable no se declaraba | `parse_declaration` maneja `:=`; los `proc` literales se parsean |

### 2.2 Resolución

- Cadena `a.b.c`: si el nombre completo está declarado, se usa; si no, se prueba
  el prefijo declarado más largo (sin bajar al paquete). Las cabeceras de imports
  externos (`core:`, `base:`, `vendor:`, `intrinsics`, `builtin`) se conservan
  como símbolos opacos.
- `context.allocator`, `$T` y `_` se ignoran.
- Las referencias no resueltas se omiten por defecto. `--unsolved` las escribe
  como un único nodo `unsolved symbol` con sus ocurrencias, para auditorías.

### 2.3 Cobertura medida

Batch de 128 archivos de `core` (el que contiene `bufio`, `bytes`, `crypto`,
`container`, etc.):

| Métrica | Antes | Después |
|---|---:|---:|
| Ocurrencias no resueltas | 1.840 | **177** |
| Nodos | 3.008 | **3.181** |
| Aristas | 7.542 | **7.917** |

Quedan sobre todo nombres de parámetros en **tipos `proc` dentro de structs**
(`on_remove: proc(value: Value, ...)`), tipos Objective-C/foreign sin declaración
visible y accesos vía `using import`. No afectan a los tipos ni a las llamadas
resueltas.

---

## 3. Indexador Odin: eficiencia

El coste por archivo estaba dominado por la base de datos, no por el parser:

| Fase (archivo vacío) | Tiempo |
|---|---:|
| `sodb_open` (setup de esquema/meta) | ~260 ms |
| parse + símbolos | ~0,4 ms |
| `sodb_commit` | ~100 ms |
| Proceso (arranque + DLL) | ~69 ms |

### 3.1 Batch nativo

El indexador acepta varios `--source-file-path=` en un proceso:

- una apertura y un commit por lote;
- caché de tablas de símbolos por directorio durante el lote;
- un `Writer` compartido con localizaciones por archivo.

El core agrupa los archivos en lotes de 128 (`%{SOURCE_FILE_PATH_ARGS}`),
`SourceGroupCustomCommand` los encola como un solo comando y
`MemoryIndexerCommandProvider` consume el lote completo. El progreso cuenta
archivos, no comandos. El fallback Python sigue en modo un archivo por proceso.

### 3.2 Resultados

| Proyecto | Antes (1 proceso/archivo) | Batch | Mejora |
|---|---:|---:|---:|
| Sample (3 archivos) | 1,107 ms | **538 ms** | 2,1× |
| `core/strings` (6) | 1.882 ms | **1.070 ms** | 1,8× |
| `core/bufio` + `core/fmt` (11) | 3.169 ms | **566 ms** | 5,6× |
| `core` completo (1.366) | ~505 s (extrapolado) | **26,3 s** | ~19× |

`core` completo, por defecto (sin no resueltas): **132.371 nodos, 350.566
aristas, 1.366 archivos, 0 errores**, 114 MB de DB, 30,8 s de pared con 2 hilos.
Con `SOURCETRAIL_ODIN_UNSOLVED=1`: 132.372 nodos, 368.725 aristas, 18.159
aristas no resueltas (31.136 ocurrencias), mismas 0 errores.

Los 11 lotes tardaron 0,6–10,4 s cada uno; la apertura fue 74–424 ms y el commit
0,5–10,3 s. El commit crece con el tamaño del lote/DB: sigue siendo el coste
dominante. Pendiente de optimizar: `PRAGMA synchronous=OFF`/`journal_mode=MEMORY`
para DB temporales (requiere exponer la conexión del writer o vendorizar
`sourcetrail-db`) y/o un único proceso por source group con lista de archivos.

---

## 4. F1: wiring de tests

- Opción `BUILD_PLUGIN_CONTEXT_TESTS=ON` en el CMake raíz; targets
  `plugin_context_test` y `plugin_host_test` con `ctest`.
- El CMake standalone de `src/lib/plugin/tests` también registra ambos tests.
- Workflow `.github/workflows/cmake-multi-platform.yml`: jobs
  `plugin-context` y `language-registry` en Linux/Windows/macOS.
- Resultado local: **2/2 tests pasan**.

---

## 5. F2: registro de lenguajes por ID

- `src/lib/settings/LanguageRegistry.*`: `LanguageDescriptor` (id estable,
  `LanguageType`, nombre visible, extensiones) y registro de mapeos
  source-group → lenguaje.
- IDs estables: `c`, `cxx`, `java`, `python`, `odin`, `custom`.
- `LanguageType.cpp` delega en el registro; `getLanguageTypeForSourceGroupType`
  ya no es un `switch` central.
- IDs de source group: `c_empty`, `cxx_empty`, `cxx_cdb`, `cxx_codeblocks`,
  `cxx_vs`, `java_empty`, `java_maven`, `java_gradle`, `python_empty`,
  `odin_empty`, `custom_command`.
- `ProjectSettings` guarda el ID estable; `stringToSourceGroupType` acepta el ID
  nuevo y los strings visibles antiguos, así que los `.srctrlprj` viejos abren.
- `SourceGroupSettingsOdinEmpty` toma `.odin` del descriptor.
- Test `src/lib/settings/tests/LanguageRegistryTest.cpp`: round-trips, mapeos,
  compatibilidad legacy y registro dinámico. **Pasa**.
- Gates: sample Odin 32/42/0; `tictactoe_cpp` 383/819/0; `tutorial-py`
  103/281/0 (baseline previo 103/272; 9 aristas de diferencia, 0 errores);
  CLI embedded 32/42/0.

---

## 6. F3: host de plugins (corte inicial)

- `src/lib/plugin/PluginHost.h`: `PluginManifest` (id, versión, nombre, inject,
  enabled, unloadable, apply) y `PluginHost` con carga/descarga, chequeo de
  dependencias, error y lista de plugins.
- Los registros usan `ctx.effect(...)`, por lo que la descarga revierte todo.
- `PluginHostTest.cpp`: dependencia faltante, disabled, unloadable=false,
  duplicado, descarga y limpieza de efectos. **Pasa**.
- **Odin ya se registra como plugin**: `src/lib/project/OdinPlugin.*` envuelve
  `SourceGroupFactoryModuleOdin` en un efecto; `main.cpp`, `cli/main.cpp` y
  `libsourcetrail.cpp` llaman a `st::plugin::ensureOdinPluginLoaded()`.
  `SourceGroupFactory::removeModule` permite la descarga.
- **C ABI v1 y carga dinámica**: `PluginABI.h` define el descriptor, el host API
  y el punto de entrada `sourcetrail_plugin_descriptor()`. `PluginHost::loadDynamic`
  usa `LoadLibrary`/`dlopen`, valida la ABI, ejecuta `apply` y descarga en orden
  (efectos primero, `FreeLibrary`/`dlclose` después).
- `PluginABITest.cpp` + `TestDynamicPlugin.cpp`: plugin compartido real,
  `effect-applied`/`effect-disposed` y biblioteca inexistente. **Pasa**.
- **Pendiente de F3**: migrar CXX/Java al host, efectos de mensajes/UI y
  reemplazar las llamadas directas restantes. No se afirma que estén hechas.

---

## 7. Tests del indexador nativo

`src/odin_indexer/tests/`:

- `run_tests.ps1` + `check_db.py` + fixtures.
- Batch vs un archivo por proceso: mismos nodos/aristas.
- Sin símbolos fantasma en cadenas de miembros.
- No resueltas off por defecto / on con `--unsolved`.
- Atributos, `#force_inline`, `foreign` con `---`, `where`, genéricos.
- Determinismo entre dos corridas.

Resultado: **5/5 pasan** (sample 32/42; globals 11/15; syntax 10/10;
unsolved 3/1 y 4/2).

---

## 8. Builds y verificaciones

- Desktop: `sourcetrail_cli`, `Sourcetrail` (GUI), `sourcetrail_api` compilan.
- Embedded: `sourcetrail_cli` compila y usa el indexador nativo.
- `plugin_context_test`, `plugin_host_test`, `language_registry_test`: pasan.
- CLI desktop/embedded sobre el sample: 32 nodos, 42 aristas, 0 errores.
- `core` completo: 1.366 archivos, 0 errores.
- Graphify actualizado.

---

## 9. Pendientes

1. F3: migración de CXX/Java al `PluginHost` y efectos de mensajes/UI (C ABI v1
   y carga dinámica ya están).
2. Parser Odin: nombres de parámetros en tipos `proc` de structs, `using import`
   y tipos Objective-C; tests de coverage masiva.
3. Coste de commit del writer: `synchronous=OFF` para DB temporales o vendorizar
   `sourcetrail-db`.
4. E5: cross-build ARM64 en Linux/QEMU/placa.
5. Historial: purge opcional de los dos logs (requiere aprobación).
6. Wizard GUI: recorrer la creación de un source group Odin con los IDs nuevos.

---

## 10. Reproducir

```powershell
# Indexador + tests
.\src\odin_indexer\build.ps1 -Odin "C:\Software\Odin-oct-2026\odin.exe"
.\src\odin_indexer\tests\run_tests.ps1 `
  -Indexer .\build\odin_indexer\SourcetrailOdinIndexer.exe

# Sample por CLI
build\Release\app\sourcetrail_cli.exe `
  bin\app\user\projects\odin-sample\odin-sample.srctrlprj --refresh=all

# Auditoría de no resueltas
$env:SOURCETRAIL_ODIN_UNSOLVED = "1"
build\Release\app\sourcetrail_cli.exe `
  <proyecto>.srctrlprj --refresh=all

# Tests de plugins y registro
ctest --test-dir _study\plugin-context-test -C Release --output-on-failure
ctest --test-dir _study\language-registry-test -C Release --output-on-failure
```

---

## 11. Segunda pasada (2026-10-19): parser y writer

### 11.1 Parser

Correcciones adicionales, todas con test o medición:

- Tipos `proc(...)` en campos/parámetros y literales: los nombres de sus
  parámetros ya no son referencias.
- `foreign libc { ... }` dentro de cuerpos y `@(link_name=...)` dentro de
  bloques foreign.
- Estructuras anónimas inline (`struct #raw_union { ... }`) y `where` en
  líneas siguientes.
- `switch v in x`, `#partial switch &v in x`, `using params` (campos del struct
  en scope) y `for key, &value in`.
- Imports relativos (`import x86 "../"`, `import "decimal"`): se resuelve la
  ruta y se usa el **nombre de paquete declarado** (`rexcode_x86`,
  `strconv_decimal`) para la tabla, manteniendo el nombre local para el código.
- Builtins: `append_elem`, `ordered_remove`, `pop`, `shrink`, `quaternion`,
  `jmag`, `kmag`, `Maybe`, `string16`, `Objc_Block`, `not_in`, etc.
- `_ :: runtime` y embeddings anónimos (`using _`) ya no generan referencias.

Resultado en `core` completo con `SOURCETRAIL_ODIN_UNSOLVED=1`:
**21.169 → 178 ocurrencias no resueltas** (0,05 % de las aristas). Quedan
algunos `_` en Foundation, constantes de enum y locales de `image/png`, sin
impacto en las referencias resueltas.

### 11.2 Writer vendorizado y modo rápido

- `src/odin_writer/vendor/sourcetrail-db/` (6,8 MB, Apache-2.0) incluye el core
  del writer y SQLite. Ya no hace falta el repo hermano; `build.ps1` lo usa por
  defecto y `-SourcetrailDB` permite un checkout externo.
- Modo rápido para DB temporales: `PRAGMA synchronous=OFF`,
  `foreign_keys=OFF`, `wal_autocheckpoint=0`, `cache_size=-65536`,
  `temp_store=MEMORY`. **No se cambia el journal mode**: el core crea las DB en
  WAL y cambiarlo rompe la visibilidad de su conexión abierta (probado: 0
  nodos).
- Efecto: `sodb_open` baja de 74–424 ms a 2–100 ms; el commit sigue costando
  0,3–5,7 s por lote (inserción/índices de SQLite, no fsync).

| Medición | Antes | Después |
|---|---:|---:|
| `core` indexado (1.366 archivos) | 24,5 s | **19,0 s** |
| Pared del CLI | 28,7 s | **23,6 s** |
| Nodos / aristas | 133.502 / 356.574 | **133.502 / 356.574** |
| `core/strings` | 1,07 s | **0,556 s** |
| `core/bufio` + `fmt` (412/1.227) | 0,57 s | **0,572 s** |

El siguiente límite es el coste de inserción/índices por lote: un solo proceso
por source group con lista de archivos, o menos índices durante la escritura,
sería el paso siguiente.

### 11.3 Tests

`src/odin_indexer/tests/run_tests.ps1`: **6/6** (se agregó el caso de tipos
`proc`). Gates: sample 32/42/0, desktop y embedded.

### 11.4 F3: C ABI v1 y carga dinámica

- `src/lib/plugin/PluginABI.h`: versión 1, `st_plugin_descriptor`,
  `st_plugin_host_api` (`add_effect`, `log`) y el entry point
  `sourcetrail_plugin_descriptor()`.
- `PluginHost::loadDynamic` carga `.dll`/`.so`/`.dylib`, valida la ABI y la
  dependencia `inject`, ejecuta `apply` y guarda el handle. Al descargar,
  primero dispone los efectos (el plugin todavía está cargado) y después
  cierra la biblioteca.
- Test real: `TestDynamicPlugin.cpp` compila una biblioteca compartida que
  registra un efecto; `PluginABITest.cpp` verifica `effect-applied`,
  `effect-disposed`, lista de plugins y biblioteca inexistente. **3/3 tests de
  plugin pasan**.
- Queda migrar CXX/Java y los efectos de mensajes/UI al host.

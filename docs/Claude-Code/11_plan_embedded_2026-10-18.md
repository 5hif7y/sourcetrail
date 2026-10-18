# Plan de implementación — Sourcetrail embedded como estándar

**Fecha:** 2026-10-18
**Autor:** Claude Code
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Objetivo:** que el perfil por defecto sea **embebido/headless de bajos recursos**:
CLI sin GUI, sin LLVM, Java opcional, hilos y heap acotados, indexado Python con daemon
y refresco incremental. El escritorio (GUI, C++, Java, tests) pasa a ser opt-in.

---

## 0. Estado de partida

| Elemento | Estado actual |
|---|---|
| Build limpio completo (4 núcleos) | **274,7 s**, 0 errores (PCH + unity en `lib`/`lib_gui`) |
| `lib` / `lib_gui` / `lib_cxx` | 111,5 s / 74,6 s / 57,9 s (target aislado) |
| Dependencias vcpkg | LLVM y tests ya son features; **Qt sigue siendo obligatorio** |
| Defaults CMake | CXX OFF, **Java ON**, GUI siempre, `SOURCETRAIL_SHARED_CORE` OFF |
| CLI | `sourcetrail_cli` enlaza sólo `Sourcetrail_lib` + plugins + boost, **sin Qt** |
| Hilos | `getIdealThreadCount()` = todos los núcleos (GUI resta 1 en Windows) |
| JVM | `-Xms64m -Xmx2048m` **fijo** |
| Python | un proceso por archivo; daemon medido: **2,44× en serie**, ~6 % en corpus grandes |
| I6 Java | `createASTs` 12,9× en parseo, ~14 % end‑to‑end; **no apto para poca RAM** (retiene N ASTs) |
| Comparación con la copia original | **0 archivos faltantes** en todas las ramas |

---

## 1. Principios

1. **CLI primero**: el GUI Qt es el mayor consumidor de RAM/disco; el perfil estándar no
   lo compila ni despliega.
2. **Nada de LLVM por defecto**: el plugin C++ (49 MB + LLVM) no es viable en embebido.
3. **Java opcional**: si se habilita, heap automático y acotado.
4. **Memoria acotada, no sólo CPU**: evitar lotes que retengan N ASTs; preferir procesos
   persistentes con una base abierta y cachés calientes.
5. **Todo reproducible**: los artefactos pesados (builds, vcpkg, PDBs) se regeneran; el
   repositorio y los docs son la fuente de verdad.

---

## 2. Paquetes de trabajo

### E1 — Perfil de build embedded (estándar)

**Cambios:**

1. `CMakeLists.txt`
   - `option(BUILD_GUI "Build the Qt GUI" OFF)`.
   - `option(BUILD_JAVA_LANGUAGE_PACKAGE ... OFF)` (hoy ON).
   - `BUILD_CXX_LANGUAGE_PACKAGE` sigue OFF; `BUILD_UNIT_TESTS_PACKAGE` OFF.
   - `SOURCETRAIL_SHARED_CORE` OFF por defecto; **forzar ON** si CXX o Java están ON
     (los plugins lo requieren).
   - `VCPKG_MANIFEST_FEATURES`: añadir `gui` sólo si `BUILD_GUI`.
   - `add_subdirectory(src/lib_gui)`, `src/app` y `src/resources` sólo si `BUILD_GUI`.
   - `sourcetrail_cli` es el target por defecto del perfil (ALL sin GUI).
2. `vcpkg.json`: mover `qtsvg`, `qtbase`, `qtimageformats` a la feature `gui`.
   - Base embedded: boost, icu, sqlite3, tinyxml, nlohmann‑json.
3. `copyAppData` selectivo (hoy copia todo `bin/app`):
   - Siempre: `data/python` (indexer) y `data/license`.
   - Si `BUILD_JAVA_LANGUAGE_PACKAGE`: `data/java`.
   - Si `BUILD_CXX_LANGUAGE_PACKAGE`: `data/cxx`.
   - Si `BUILD_GUI`: `color_schemes`, `fonts`, `syntax_highlighting_rules`, `fallback`,
     recursos Qt.
   - `user/projects` (tutoriales) sólo con `COPY_SAMPLE_PROJECTS=ON` (default OFF).
4. `CMakePresets.json`: `embedded-release` como preset documentado/estándar;
   `desktop-release` habilita GUI + CXX + Java + tests y `/O2`.

**Aceptación:** configure < 60 s con caché de vcpkg; build < 5 min en 4 núcleos;
artefactos sin Qt/LLVM; CLI arranca e indexa Python.

### E2 — Límites de recursos

1. **Hilos** (`src/lib/utility/utilityApp.cpp`):
   - `SOURCETRAIL_INDEXER_THREADS` (env) tiene prioridad.
   - Default embedded: `max(1, min(hardware_concurrency, 2))`; el preset desktop sube
     el valor.
   - Propagar a `Project::buildIndex` y a los usos de `PersistentStorage`.
2. **Heap JVM** (`src/lib_java/data/parser/java/JavaEnvironmentFactory.cpp`):
   - `SOURCETRAIL_JAVA_HEAP` (p. ej. `512m`) tiene prioridad.
   - Si no: `min(2048 MB, max(256 MB, 25 % de RAM física))`; `-Xms` = min(64 MB, heap/4).
   - RAM física: `GlobalMemoryStatusEx` (Windows), `sysconf` (Linux), `sysctl` (macOS).
3. **Python shallow** (`SourceGroupSettingsPythonEmpty`):
   - `SOURCETRAIL_PYTHON_SHALLOW=1` o setting del proyecto añade `--shallow`.
   - Parso sin jedi: mucho menos RAM/tiempo, menor precisión. Opt‑in.
4. **Refresco incremental del CLI** (`src/cli/main.cpp`):
   - Hoy fuerza `RefreshMode::ALL_FILES`.
   - Añadir `--refresh=all|updated` (default **updated** en embedded) y `--threads=N`.
   - Si no hay base previa, `updated` indexa todo. `--refresh=all` para CI.
5. **Full‑text search opcional** (`PersistentStorage`):
   - Setting `indexing/full_text_search_enabled` (default OFF en embedded, ON en GUI).
   - Evita el `SuffixArray`/índice en RAM cuando el CLI no necesita autocompletado.
6. **Caché SQLite**: `PRAGMA cache_size=-2000` (2 MB) en el perfil embedded.

**Aceptación:** con 1 hilo y heap 512 MB, indexar tutorial-py y javaparser sin OOM;
RSS del CLI por debajo del objetivo (§5); segunda indexación incremental mucho más rápida.

### E3 — Daemon Python transparente (sin cambios en C++)

**Diseño:**

- `run.py` se divide en **cliente liviano** (sin importar jedi) y **daemon**.
- Un daemon **por base de datos** (cada thread de C++ usa una base `_threadN`), escuchando
  en un named pipe (Windows `AF_PIPE`) o Unix socket (`AF_UNIX`); fallback TCP localhost.
- Handshake de versión + token aleatorio; directorio temporal por usuario.
- El daemon mantiene `srctrl.open` y los imports de jedi/parso/numbat calientes; procesa
  una solicitud por vez (el thread de C++ ya es secuencial) y devuelve stdout/stderr/código.
- Timeout de inactividad (60 s) y limpieza por PID/lock file; si el daemon no arranca o
  falla, el cliente cae al indexado local actual (fallback seguro).
- `SOURCETRAIL_PYTHON_DAEMON=0` lo desactiva.

**Medición esperada:** 2,44× en serie en archivos chicos; ~6 % en matplotlib (3,5 min con
4 workers, ~14 min con 1 núcleo); counts idénticos (104/395/21 en tutorial-py).

**Aceptación:** counts idénticos en tutorial-py y una muestra de matplotlib; sin procesos
huérfanos tras cerrar; fallback probado matando el daemon a mitad de indexado.

### E4 — Tamaño y memoria

1. **Flags de tamaño** (`cmake/Sourcetrail.cmake`):
   - `SOURCETRAIL_EMBEDDED` (default ON): MSVC `/O1 /Gy /Gw` + `/OPT:REF,ICF`;
     GCC/Clang `-Os -ffunction-sections -fdata-sections` + `--gc-sections`.
   - Desktop: `/O2` u `-O3` como hoy.
2. **Sin PDBs en el deploy**: no copiar `*.pdb` de Qt/plugins; strip en Linux.
3. **Linkage**: triplet embedded con librerías estáticas y CRT estático
   (`x64-windows-static` / `arm64-linux` con `VCPKG_CRT_LINKAGE static`), sujeto a prueba.
4. **Datos**: no copiar tutoriales, fuentes ni reglas de sintaxis en CLI-only.
5. **FTS/caché**: ver E2.5/E2.6.
6. **Plugins bajo demanda**: el CLI carga sólo los plugins presentes; sin CXX/Java no hay
   DLLs que cargar. Verificar que `PluginManager` no falle si `plugins/` está vacío.

**Aceptación:** medir tamaño del deploy CLI-only (sin Python runtime) y RSS con 1 hilo.

### E5 — Cross‑compilación y empaquetado

1. Triplets vcpkg para `arm64-linux` (y `x64-linux`) embedded; probar en Raspberry Pi o
   QEMU.
2. Documentar el flujo: compilar en host, copiar `sourcetrail_cli` + `Sourcetrail_lib` +
   datos + runtime Python/jedi/numbat; o **pre‑indexar en desktop y copiar el `.srctrldb`**.
3. `BUILD_EMBEDDED.md` con requisitos, tamaño, límites y variables de entorno.

---

## 3. Ideas adicionales (mayor eficiencia)

| Idea | Por qué |
|---|---|
| **Refresco incremental por defecto en CLI** | Reindexar sólo archivos cambiados; en embedded es la mayor ganancia repetida. |
| **Pre‑indexar en desktop y copiar la base** | El dispositivo no indexa; sólo consulta/refresca. |
| **FTS off en CLI** | El índice de sufijos puede dominar la RAM en proyectos grandes. |
| **Shallow Python** | Parso sin jedi para RAM muy baja; degradación controlada. |
| **Daemon por base** | Elimina arranque/imports/DB open por archivo sin aumentar el pico de RAM. |
| **Heap JVM automático** | Evita `-Xmx2048m` en máquinas de 2 GB. |
| **Hilos = 1–2** | Menos procesos Python/Java simultáneos; menos pico de RAM y thrashing. |
| **Sin PDBs ni tutoriales en el deploy** | Decenas de MB de disco. |
| **Flags de tamaño + strip** | Binarios y DLLs más chicos; menos page cache. |
| **No cargar plugins ausentes** | Arranque más rápido y menos errores en el dispositivo. |
| **I6 NO para embedded** | `createASTs` retiene N ASTs; en poca RAM es contraproducente. Revisar sólo con lote 4–8 y heap bajo. |

---

## 4. Archivos a tocar

| Archivo | Cambio |
|---|---|
| `CMakeLists.txt` | `BUILD_GUI`, defaults Java/shared/core, features vcpkg, subdirs condicionales, `copyAppData` selectivo. |
| `vcpkg.json` | Feature `gui` con Qt; base sin Qt. |
| `CMakePresets.json` | `embedded-*` (estándar) y `desktop-*` (opt‑in). |
| `cmake/Sourcetrail.cmake` | Flags `/O1`/`-Os`, strip, `SOURCETRAIL_EMBEDDED`. |
| `src/lib/utility/utilityApp.cpp/.h` | Override de hilos y RAM física. |
| `src/lib_java/.../JavaEnvironmentFactory.cpp` | Heap automático/env. |
| `src/lib/data/storage/PersistentStorage.cpp` | FTS opcional; caché SQLite. |
| `src/cli/main.cpp` | `--refresh`, `--threads`, `--java-heap`, `--python-shallow`. |
| `src/lib/settings/source_group/type/SourceGroupSettingsPythonEmpty.cpp` | Modo shallow. |
| `bin/app/data/python/SourcetrailPythonIndexer/run.py` | Cliente/daemon. |
| `BUILD_WINDOWS.md` / nuevo `BUILD_EMBEDDED.md` | Documentación del perfil. |

---

## 5. Métricas de aceptación

| Métrica | Objetivo embedded |
|---|---|---|
| Deploy CLI-only (sin runtime Python) | < 60 MB, sin Qt/LLVM/PDB |
| Build CLI-only en 4 núcleos | < 5 min |
| RSS indexando tutorial-py con 1 hilo | < 300 MB |
| RSS indexando javaparser con 1 hilo / heap 512 MB | sin OOM |
| Segunda indexación (sin cambios) | < 10 % del tiempo de la primera |
| Daemon Python | 2,44× en archivos chicos; ~6 % en corpus grandes; counts idénticos |
| Procesos huérfanos | 0 tras cerrar (timeout incluido) |
| GUI desktop | sigue compilando y funcionando con `desktop-*` |

---

## 6. Fases y esfuerzo

| Fase | Contenido | Estimación |
|---|---|---|
| 0 | Baseline: build CLI-only actual, RSS, tamaño de deploy | 0,5 día |
| 1 | E1 perfil de build (GUI off, Qt feature, Java off, datos selectivos) | 1–2 días |
| 2 | E2 límites (hilos, heap, refresh incremental, shallow, FTS/caché) | 1–2 días |
| 3 | E3 daemon Python + pruebas/fallback | 2–3 días |
| 4 | E4 tamaño/RAM (flags, strip, static, PDBs) | 1–2 días |
| 5 | E5 cross‑compile/empaquetado + docs | 2–3 días |
| — | Validación en dispositivo/QEMU | 1–2 días |

**Orden recomendado:** 1 → 2 → 3 → 4 → 5. El daemon (3) puede ir en paralelo con 4.

---

## 7. Riesgos y mitigación

| Riesgo | Mitigación |
|---|---|
| Cambiar defaults rompe flujos de escritorio | Presets `desktop-*` y docs; CI desktop opcional. |
| vcpkg poda Qt/LLVM del árbol instalado | Caché binaria (7,2 GB) restaura en minutos; documentar. |
| Daemon con procesos huérfanos o bloqueos | Timeout, lock/PID, fallback local, `SOURCETRAIL_PYTHON_DAEMON=0`. |
| Refresco incremental con timestamps poco fiables | `--refresh=all` explícito; validar mtime/hash. |
| Static linking/CRT en Windows | Probar primero `x64-windows-static-md`; mantener dinámico como fallback. |
| Cross‑compile de boost/icu | Empezar por Python-only; añadir Java/CXX después. |
| Regresiones de indexado | Correr tutorial-py/javaparser/matplotlib y comparar counts. |

---

## 8. Estado y próximos pasos

- [x] Comparación con la copia original: **0 archivos faltantes**.
- [x] Spikes I6/I1‑I2 medidos y documentados (`10_...`, `spikes/`).
- [ ] E1 perfil de build.
- [ ] E2 límites de recursos.
- [ ] E3 daemon Python.
- [ ] E4 tamaño/RAM.
- [ ] E5 cross‑compilación y docs.

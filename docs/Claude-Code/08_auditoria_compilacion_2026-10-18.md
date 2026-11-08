# Auditoría de código y de los mecanismos de compilación

**Fecha:** 2026-10-18
**Autor:** Claude Code
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Pregunta:** ¿alivianamos la compilación? ¿Queda algo pendiente?

> **Actualización (ver `09_pendientes_2026-10-18.md`):** B3 se aplicó con
> `UNITY_BUILD` en `lib` y `lib_gui`. El build limpio completo baja de 535,1 s a
> **274,7 s (4,6 min)**, 0 errores. `lib_cxx` queda sin unity por colisiones con
> símbolos de Clang.

---

## 0. Resumen

| Pregunta | Respuesta |
|---|---|
| ¿La compilación del **proyecto** se alivió? | **Sí.** Build limpio completo: **906,7 s (fallido) → 535,1 s (exitoso)**. PCH nuevo en `lib` (−51 %) y `lib_cxx` (−74 %), más el PCH previo de `lib_gui` (−60 %). |
| ¿Se alivió la causa de las **4 h**? | **Sí, estructuralmente.** Esa causa era vcpkg compilando **LLVM**, no el proyecto. Ahora LLVM sólo se instala si `BUILD_CXX_LANGUAGE_PACKAGE=ON`. |
| ¿Había bugs? | **Sí, tres de build** que impedían un build limpio correcto (uno rompía el link de `lib_cxx`, otro la copia de Qt), **dos de código** en el parser de Code::Blocks y **uno de repositorio** (`.gitignore`). Todos corregidos. |
| ¿Queda pendiente? | Detalle en §8. Nada bloqueante para compilar/indexar; sí hay deuda de empaquetado y optimizaciones de indexado de bajo ROI. |

---

## 1. Dónde se van las horas

Hay que separar tres etapas que se confunden bajo "compilar 4 horas":

| Etapa | Qué es | Esta máquina (i5‑7400, 4 núcleos, 16 GB) | Máquina sin caché |
|---|---|---|---|
| **1. vcpkg** | Compilar/instalar dependencias; **LLVM domina** | **0 s** (ya instalado) | **horas** (LLVM solo: varias horas en 4 núcleos) |
| **2. Configurar** | CMake + verificación de vcpkg | **31,9 s** | ~1 min + instalación |
| **3. Compilar el proyecto** | ~468 TU + enlaces | **ver §2** | igual |

El árbol `build/vcpkg_installed` ocupa **33 GB** y la caché binaria de vcpkg **7,2 GB**;
`llvm:x64-windows@21.1.2` se instaló el **2026‑08‑12**. Esa instalación es la que costó
las horas, y sólo hace falta **una vez** por máquina (o se restaura de la caché binaria).

---

## 2. Mediciones

### 2.1 Baseline (antes de esta auditoría)

Build limpio completo en `build-clean/`, reutilizando `build/vcpkg_installed`:

```
BUILD CLEAN (ALL targets): 906,7 s, exit=1, 917 errores
```

**No era un baseline válido**: falló al enlazar `lib_cxx` por un bug de rutas (§4.1) y
los 916 mensajes `LNK2038`/`LNK1319` inflaron el tiempo. Los spans de objetos (primera a
última compilación de cada target, 4 núcleos) sí son comparables:

| Target | TU | Span de compilación |
|---|---|---|
| `lib` | 237 | **6,7 min** |
| `lib_cxx` | 55 | **3,4 min** (~14,8 s/TU) |
| `lib_gui` | 161 | ~3 min (ya tenía PCH) |

### 2.2 Después de las correcciones y los PCH nuevos

| Target | Antes | Después | Ganancia |
|---|---|---|---|
| `lib` (237 TU) | span 6,7 min | **span 3,3 min** (target completo aislado: 223,3 s) | **−51 %** |
| `lib_cxx` (55 TU) | span 3,4 min | **span 0,9 min** (target completo aislado: 57,9 s) | **−74 %** |
| `lib_gui` (161 TU) | 343,1 s | **136,8 s** (medido antes, PCH ya existente) | −60 % |
| **Build limpio completo** | **906,7 s, exit 1** (fallaba al link) | **535,1 s, exit 0, 0 errores** | **−41 %** (y termina) |

En el build limpio completo los spans son algo mayores por contención entre targets
(`lib` 3,9 min, `lib_cxx` 1,3 min, `lib_gui` 3,4 min); la comparación justa es la
medición aislada de cada target, arriba. El baseline habría tardado más si hubiera
terminado: no llegó a enlazar `Sourcetrail.exe` ni `sourcetrail_cli.exe`.

PCH generados: `lib` 140,8 MB; `lib_cxx` 523,4 MB; `lib_gui` 268,1 MB.

Otros números ya medidos en `04_plan_performance.md`: Maven incremental **12,6 s → 0,8 s**,
build no-op **6,7 s**, incremental de 1 TU + link **4,4 s**.

---

## 3. La causa real de las 4 horas: LLVM incondicional

### 3.1 Diagnóstico

`vcpkg.json` declaraba `llvm`, `catch2` y `gtest` como dependencias **incondicionales**.
`find_package(Clang)` sólo corre bajo `BUILD_CXX_LANGUAGE_PACKAGE`, que por defecto está
en **OFF**, pero vcpkg instalaba LLVM igual. Además, todos los presets
(`CMakePresets.json`, `sourcetrail-presets`) fuerzan `BUILD_CXX_LANGUAGE_PACKAGE=ON` y
`BUILD_UNIT_TESTS_PACKAGE=ON`, así que el camino documentado siempre compila LLVM.

### 3.2 Solución aplicada

`vcpkg.json` ahora usa **features**:

```json
"features": {
  "cxx":   { "dependencies": ["llvm"] },
  "tests": { "dependencies": ["catch2", "gtest"] }
}
```

`CMakeLists.txt` declara las opciones **antes de `project()`** (vcpkg corre dentro de
`project()`) y construye `VCPKG_MANIFEST_FEATURES`:

```cmake
option(BUILD_CXX_LANGUAGE_PACKAGE "..." OFF)
option(BUILD_UNIT_TESTS_PACKAGE "..." OFF)
set(VCPKG_MANIFEST_FEATURES "")
if (BUILD_CXX_LANGUAGE_PACKAGE)
  list(APPEND VCPKG_MANIFEST_FEATURES "cxx")
endif()
if (BUILD_UNIT_TESTS_PACKAGE)
  list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()
project(...)
```

`BUILD_UNIT_TESTS_PACKAGE` además **ahora existe** como opción (antes se usaba sin
declararse y sólo funcionaba si un preset la definía).

### 3.3 Verificación (sin instalar nada)

`vcpkg install --dry-run` contra el manifiesto nuevo:

| Features | Resultado |
|---|---|
| ninguna | `llvm`, `catch2`, `gtest` → **will be removed** (no se instalan) |
| `cxx` | sólo `catch2`/`gtest` se quitan; **LLVM se queda** |
| `cxx,tests` | nada se quita (estado actual completo) |

Consecuencia práctica: `cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=...` tarda ~1 min y
no toca LLVM; para el indexador C++ hay que pasar `-DBUILD_CXX_LANGUAGE_PACKAGE=ON`.

Al alternar la opción, vcpkg **poda** del árbol instalado lo que ya no se usa (p. ej.
LLVM al pasar CXX a OFF). El caché binario no se toca: volver a ON restaura LLVM desde
caché en minutos, sin recompilarlo.

---

## 4. Bugs de compilación encontrados y corregidos

### 4.1 `Clang_DIR` hardcodeado rompía el link al compartir `VCPKG_INSTALLED_DIR`

`CMakeLists.txt` forzaba:

```cmake
set(Clang_DIR "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/cmake/clang" ...)
```

Si `VCPKG_INSTALLED_DIR` apunta a otro lado (justo la optimización de reutilizar el árbol
instalado entre builds), esa ruta no existe y CMake cae en `debug/lib/cmake/clang`.
Resultado: **916 errores `LNK2038`** (`_ITERATOR_DEBUG_LEVEL` y `RuntimeLibrary`) y
`LNK1319: 916 mismatches`.

**Corregido** a `${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/cmake/clang`.
Verificado: `Clang_DIR` en el cache apunta a `lib/cmake/clang`, `lib_cxx` enlaza y el
plugin de 49,4 MB se genera.

### 4.2 Ruta de plugins Qt hardcodeada

`src/app/CMakeLists.txt` copiaba los plugins de plataforma desde:

```cmake
"${CMAKE_CURRENT_BINARY_DIR}/../../vcpkg_installed/x64-windows/Qt6/plugins/platforms"
```

Mismo problema: falla con `VCPKG_INSTALLED_DIR` compartido y **también en builds sin
vcpkg** en Windows. **Corregido** a
`"${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins/platforms"`
y el bloque ahora exige `isVcpkgBuild`.

### 4.3 `DebugStubs.cpp` no puede compilar con el PCH de `lib_cxx`

El PCH fuerza `<cstdlib>` antes del archivo, y sus stubs de CRT (`_calloc_dbg`) chocan
con las macros/declaraciones de la CRT (`error C2065: '_calloc_dbg': undeclared
identifier`). **Corregido** con
`set_source_files_properties(DebugStubs.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)`.

### 4.4 El port al fork omitía tres archivos

Al configurar el fork por primera vez aparecieron tres archivos que el port de la tarea 3
no había incluido: `src/lib/SourcetrailLibExport.h`, `src/lib_cxx/SourcetrailCxxExport.h`
y `src/lib_cxx/DebugStubs.cpp`. Los dos primeros los incluyen headers del core y del
plugin C++; sin ellos no compila el core compartido. El configure fallaba con
`Cannot find source file: DebugStubs.cpp`. Se agregaron y el fork configura con exit 0.

### 4.5 `.gitignore` con `build/` en UTF‑16

El archivo terminaba en `62 00 75 00 69 00 6c 00 64 00 2f 00 0d 00 0a 00` ("build/" en
UTF‑16LE sin BOM), por lo que **`build/` nunca estuvo ignorado**. Corregido a `build/` en
UTF‑8. (En el clon del fork ya estaba corregido; faltaba en el árbol de trabajo.)

---

## 5. Optimizaciones aplicadas

### 5.1 PCH en `lib`

Headers estándar compartidos (`<algorithm>`, `<string>`, `<vector>`, `<map>`, `<memory>`,
`<thread>`, …). Se limita a la biblioteca estándar a propósito: los headers propios
cambian seguido y harían recompilar el PCH en casi cada edición.

### 5.2 PCH en `lib_cxx`

Headers de Clang/LLVM que dominan el costo por archivo (`ASTContext.h`, `Decl.h`,
`DeclCXX.h`, `DeclTemplate.h`, `SourceManager.h`, `CompilerInstance.h`,
`Preprocessor.h`, `Tooling.h`, …). Se excluyeron headers template usados por pocos
archivos (`RecursiveASTVisitor.h`) para no encarecer las 55 TU.

### 5.3 Otros mecanismos revisados (sin cambios)

- `/MP`, `/W4`, `/permissive-`, `/Zc:*`, `/utf-8` ya están en `setMsvcTargetOptions`.
- El generador VS con `--parallel` reparte TU entre procesos; `/MP` es redundante por
  archivo pero inofensivo.
- `deploySourcetrailCore` ya copia la DLL del core a `app` (CLI incluido).
- Maven incremental por stamp ya estaba aplicado.

---

## 6. Bugs de código

Ambos en `src/lib_cxx/utility/codeblocks/CodeblocksProject.cpp`, heredados de upstream:

| Línea | Antes | Después | Efecto |
|---|---|---|---|
| 79 | `if (codeBlocksProjectFileElement == nullptr)` tras desreferenciarlo y antes de usar `projectElement` | `if (projectElement == nullptr)` | **null deref** si el XML no tiene `<Project>` |
| 93 | `project->m_title = stringToCompilerVarType(value);` (`m_title` es `std::string`) | `project->m_title = value;` | asignaba un **enum** (1 byte) al título |

**CLI con ruta relativa** (`src/cli/main.cpp`): el ejecutable usaba `argv[1]` tal cual.
Al pasar un proyecto relativo, la base temporal derivada de esa ruta quedaba relativa y
el proceso indexador (que corre con el directorio del proyecto como CWD) la resolvía a
una ruta inexistente: `ERROR: unable to open database file` y **0 nodos**. Ahora la ruta
se hace absoluta antes de cargar el proyecto. Verificado indexando `tutorial-py` con una
ruta relativa: **103 nodos, 177 edges distintos, 20 archivos**.

---

## 7. Warnings y deuda técnica

- **~680 warnings** en el build limpio completo: 306 C4251 + 57 C4275 (miembros STL/Qt en
  interfaz DLL), 274 C4702 (casi todos en `llvm/ADT/bit.h`, header de terceros repetido
  por cada TU), 25 C4244, 19 C4996 (`getenv`/`strncpy`). No son errores; C4251/C4275 son
  esperables con el core compartido. Opcional: `/external:W0` para silenciar los de
  headers de LLVM sin ocultar los del código propio.
- **58 `TODO`/`FIXME`/`HACK`** en `src` (heredados). Los más relevantes:
  `PersistentStorage` (consultas, nodos de namespace), `TaskBuildIndex` (archivos no
  indexados), `Application` (weak_ptr redundante).
- **Documentación desactualizada**: `BUILD_WINDOWS.md` pide VS 2022 y CMake 3.15;
  `CMakeLists.txt` exige CMake **3.27** y C++23, y el toolchain usado es VS 18 2026.
- **Presets**: ✅ corregidos. El triplet y el overlay de LLVM sí existen en el repo
  (`vcpkg-triplets/`, `vcpkg-overlay-ports/llvm`), pero `CMakePresets.json` no los
  declaraba. Ahora `vcpkg-presets` pasa `VCPKG_OVERLAY_TRIPLETS` y
  `VCPKG_OVERLAY_PORTS`. El submódulo `vcpkg/` sigue necesitando
  `git submodule update --init --recursive` (documentado en `BUILD_WINDOWS.md`).
- **Indexador Python**: ✅ empaquetado en `bin/app/data/python/SourcetrailPythonIndexer`
  (16 archivos, 0,4 MB) y el resolver lo busca primero ahí. `copyAppData` lo despliega en
  cada configuración. El entorno Python debe tener `jedi`, `parso` y `numbat`
  (documentado en `BUILD_WINDOWS.md`).

---

## 8. Pendientes

| ID | Pendiente | ROI / riesgo | Estado |
|---|---|---|---|
| B1 | Triplet/overlay de vcpkg en los presets | Medio / bajo | **✅ Hecho** (submódulo: `git submodule update --init`) |
| B2 | Empaquetar `SourcetrailPythonIndexer` y ajustar el resolver | Alto para distribución / bajo | **✅ Hecho** |
| B3 | Unity build (`C4`) en `lib`/`lib_gui` | −20 a −40 % adicional, riesgo de colisiones | **✅ Hecho** (`lib` y `lib_gui`; `lib_cxx` descartado, ver `09_...`) |
| B4 | Portar estas correcciones al fork `5hif7y/sourcetrail` (rama `modernized`) | Necesario para que el fork compile igual | **✅ Hecho** (commits en `modernized`) |
| I1/I2 | Batch/daemon del indexador Python (elimina ~0,94 s de arranque por archivo) | **~6 %** con paralelismo activo | No hecho, bajo retorno |
| I6 | `ASTParser.createASTs` batch para Java | Medio | Requiere cambiar el contrato JNI |
| I7 | Inject paralelo | Bajo | Descartado por prioridad |
| — | Diferencia de 0,2 % entre indexado serie y paralelo (matplotlib) | Desconocido | Investigar sólo si se exige bit‑identidad |

---

## 9. Verificación realizada

1. Configure limpio reutilizando `VCPKG_INSTALLED_DIR`: **31,9 s, exit 0**.
2. Dry‑run de vcpkg con 0/1/2 features: comportamiento correcto (§3.3).
3. Build completo con las correcciones: **exit 0, 0 errores**; `Sourcetrail.exe`,
   `sourcetrail_cli.exe`, `sourcetrail_cxx_plugin.dll` (49,4 MB) y
   `sourcetrail_java_plugin.dll` generados; ambos plugins **cargan** al ejecutar el CLI.
4. PCH: `lib` 223,3 s y `lib_cxx` 57,9 s, ambos con 0 errores.
5. Build limpio completo final (`--clean-first`, todos los targets): **535,1 s
   (8,9 min), exit 0, 0 errores**; `Sourcetrail.exe` (2,8 MB), `sourcetrail_cli.exe`,
   plugin C++ (49,4 MB), plugin Java y `java-indexer.jar` presentes.
6. GUI: `Sourcetrail.exe` arranca, muestra la ventana **"Sourcetrail"** y cierra con
   `CloseMainWindow` → **exit code 0** (valida el despliegue de `platforms/qwindows.dll`).
   `platforms/` contiene `qwindows.dll`, `qminimal.dll`, `qoffscreen.dll` y
   `qdirect2d.dll`; los dos plugins de lenguaje están en `app/plugins/`.
7. Indexado Python con el indexer **empaquetado** (`app/data/python/...`) y una ruta de
   proyecto **relativa**: `tutorial-py` → **103 nodos, 177 edges distintos, 20 archivos**,
   exit 0. El conteo de filas de `edge` varía (216–223) por duplicados del merge paralelo,
   pero los edges distintos son estables (177).
8. Fork `modernized`: configure **21,6 s, exit 0**; build completo **694,4 s (11,6 min),
   exit 0, 0 errores**; `Sourcetrail.exe`, CLI, plugin C++ (49,4 MB), plugin Java y el
   indexer Python desplegado. El build del fork es más lento que el local (535,1 s)
   porque incluye la compilación inicial de Maven para el jar de Java.

**No verificado**: instalación de vcpkg desde cero en otra máquina (requeriría horas de
LLVM), y reindexado de los corpus (no se tocó ningún camino de indexado en esta pasada).

---

## 10. Conclusión

- **Sí, la compilación del proyecto se alivianó de forma medible**: el build limpio
  completo pasó de **906,7 s (fallido) a 535,1 s (exitoso)**; PCH en `lib` (−51 %),
  `lib_cxx` (−74 %) y `lib_gui` (−60 %); incremental de segundos.
- **La causa de las 4 h era vcpkg/LLVM**, no el código. El split por features evita
  compilar LLVM cuando no se usa el indexador C++: el build por defecto (GUI + Java +
  Python) ya no lo toca. El build con C++ sigue necesitando LLVM una vez por máquina,
  idealmente desde caché binaria o un LLVM precompilado.
- **Los dos bugs de build reproducibles** al compartir el árbol de vcpkg (link de
  `lib_cxx` y copia de Qt) quedaron corregidos y verificados.
- **Pendientes B1/B2 cerrados**: los presets declaran el triplet y el overlay de LLVM, y
  el indexador Python viaja dentro de la app (`app/data/python`). Además se corrigió el
  CLI para aceptar rutas relativas de proyecto (antes indexaba 0 archivos).
- Todo quedó aplicado también al fork `5hif7y/sourcetrail`, rama `modernized`.

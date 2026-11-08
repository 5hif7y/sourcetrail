# Auditoría post-regeneración y backlog consolidado

**Fecha:** 2026-10-18
**Autor:** Claude Code
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Contexto:** build desde cero tras borrar ~144 GB de builds (informe `12_...`).

---

## 1. Resultado de la auditoría

**No se rompió nada.** El build regenerado funciona en los tres lenguajes y en la GUI:

| Prueba | Resultado |
|---|---|
| C++ `tutorial` | exit 0, 6,5 s, **99 nodos / 153 edges / 21 archivos** |
| Java `javaparser` | exit 0, 19,8 s, **4.126 nodos / 25.601 edges / 138 archivos** |
| Python `tutorial-py` (ruta relativa) | exit 0, **103 nodos / 177 edges distintos / 20 archivos** |
| Plugins C++ y Java | cargan |
| GUI | arranca y cierra con exit 0 |
| Build | **0 errores** (`error C/LNK/fatal/MSB3073` = 0) |
| Warnings | 181 C4251, 38 C4275, 274 C4702 (headers de LLVM), 11 C4996, 25 C4244 |
| Artefactos | `.exe`, CLI, plugin C++ 49,43 MB, plugin Java 0,26 MB, `sourcetrail_indexer.exe`, indexer Python |
| Fork | limpio, `d764bfd8`, igual a `origin/modernized` |

## 2. Hallazgos (ninguno es una rotura)

1. **`bin/app/data/fallback/ApplicationSettings.xml`** se modificó al abrir la GUI
   (agregó un proyecto reciente). Revertido; el archivo vuelve a estar limpio.
2. **El repo local `master` sigue rastreando `build/`** y quedó con ~1.500 entradas
   (deletions/modificaciones). Es higiene, no funcionalidad: el fork `modernized` es la
   fuente de verdad y está limpio. El `.gitignore` ya ignora `build/`; faltaría
   `git rm -r --cached build` + commit si se quiere limpiar el repo local.
3. **`src/indexer/CMakeLists.txt` enlaza `Sourcetrail_lib_gui` incondicionalmente.**
   Con `BUILD_GUI=OFF` y CXX ON, el ejecutable `sourcetrail_indexer` no compilaría.
   Hay que quitar ese link (el indexer es headless) para soportar CLI+CXX sin GUI.
4. **`src/test` incluye suites Qt** (`QtActionsTestSuite`, `QtResourcesTestSuite`):
   los tests requieren GUI. Gatear `BUILD_UNIT_TESTS_PACKAGE` con `BUILD_GUI`.
5. **El árbol `build/` ocupa 34,5 GB** (reproducible). Se puede borrar cuando no se
   necesite; la caché binaria de vcpkg (7,3 GB) queda para reconstruir.
6. **El submódulo `vcpkg` sigue vacío**; los presets `vcpkg-*` requieren
   `git submodule update --init --recursive`.
7. **El perfil embedded no está implementado todavía**: los defaults siguen siendo
   Java ON, GUI siempre, shared core OFF.

## 3. Backlog consolidado

| ID | Tema | Estado / prioridad |
|---|---|---|
| **E1** | Perfil embedded estándar: `BUILD_GUI=OFF`, Java OFF, Qt como feature, subdirs condicionales, datos selectivos, `src/indexer` sin lib_gui, tests con GUI | **En curso — siguiente** |
| **E2** | Hilos 1–2 (`SOURCETRAIL_INDEXER_THREADS`), heap JVM automático, `--refresh=updated`, shallow Python, FTS off, caché SQLite | Pendiente |
| **E3** | Daemon Python transparente (2,44× en serie; ~6 % en corpus grandes) | Pendiente |
| **E4** | Tamaño: `/O1`/`-Os`, strip, sin PDBs/tutoriales, triplets estáticos | Pendiente |
| **E5** | Cross-compile ARM64 + empaquetado + `BUILD_EMBEDDED.md` | Pendiente |
| B1 | Inicializar submódulo vcpkg y presets | Pendiente (documentado) |
| I1/I2 | Daemon/batch Python | Evaluado; se implementa en E3 |
| I6 | `createASTs` batch Java | Spike cerrado; no apto para poca RAM; revisar sólo con lote 4–8 |
| I7 | Inject paralelo | Cerrado por bajo ROI |
| — | Diferencia 0,2 % serie/paralelo | Aceptada; sin requisito de bit‑identidad |
| — | `/external:W0` para warnings de LLVM | Opcional |

## 4. Validación de plugins para E1

La preocupación es que al desactivar CXX/Java por defecto los plugins **sigan
compilando al reactivarlos**. Matriz a ejecutar:

| Configuración | GUI | CXX | Java | Qué valida |
|---|---|---|---|---|
| **Embedded** | OFF | OFF | OFF | CLI-only, sin Qt/LLVM/JVM |
| **CLI+CXX** | OFF | ON | OFF | El plugin C++ y `sourcetrail_indexer` compilan sin GUI |
| **CLI+Java** | OFF | OFF | ON | El plugin Java compila sin GUI |
| **Desktop** | ON | ON | ON | Ambos plugins + GUI (regresión) |

Para CLI+CXX se reutiliza `build/vcpkg_installed` pasando `gui` como feature extra
(para no podar Qt del árbol compartido) y se compilan `lib_cxx` + `sourcetrail_indexer`
+ CLI. Para CLI+Java no hace falta Qt ni LLVM.

## 5. E1 implementado

### 5.1 Cambios

- `CMakeLists.txt`: `BUILD_GUI` (default **OFF**), Java **OFF** por defecto,
  `COPY_SAMPLE_PROJECTS` OFF, `SOURCETRAIL_SHARED_CORE` forzado ON si CXX o Java;
  `VCPKG_MANIFEST_FEATURES` respeta features del usuario y agrega `gui`/`cxx`/`tests`.
- `vcpkg.json`: Qt (`qtsvg`, `qtbase`, `qtimageformats`) movido a la feature `gui`.
- Subdirectorios condicionales: `resources`, `lib_gui`, `app`, `src/indexer` (CXX) y
  `src/test` (tests + GUI). `configureLicenseFile` sólo con GUI.
- `src/indexer` ya no enlaza `Sourcetrail_lib_gui` (es headless) y se marca
  `WIN32_EXECUTABLE OFF`: define `main()`, no `WinMain` (antes lo aportaba Qt al
  enlazar `lib_gui`).
- `deploySourcetrailCore` copia también `$<TARGET_RUNTIME_DLLS:...>`: en shared-core
  sin GUI nadie desplegaba las dependencias del core (ICU/SQLite/Boost) y el CLI moría
  con `0xC0000135` (DLL not found).
- `ApplicationSettingsPrefiller`: los detectores Java/CXX quedan bajo
  `#if BUILD_JAVA_LANGUAGE_PACKAGE` / `#if BUILD_CXX_LANGUAGE_PACKAGE`.
- `SourcetrailLibExport.h`: en core **static** no aplica `dllimport` (error C2491);
  sólo decora cuando `SOURCETRAIL_SHARED` está definido.
- `copyAppData` selectivo: python/fallback/license siempre; java si Java; fuentes,
  colores, reglas y plantillas sólo con GUI; tutoriales sólo con `COPY_SAMPLE_PROJECTS`.
- `CMakePresets.json`: presets estándar = embedded; `desktop-presets` +
  `system-release-desktop`, `vcpkg-release-desktop`, `vcpkg-debug-desktop`.
- `BUILD_WINDOWS.md`: perfil embedded por defecto y comandos desktop opt-in.

### 5.2 Resultado embedded (sin Qt, sin LLVM, sin Java)

| Etapa | Tiempo |
|---|---|
| Configure (incluye vcpkg base) | **241,2 s** |
| Build | **70,6 s** |
| **Total** | **311,9 s (5,2 min)**, 0 errores |

- Árbol `build-embedded`: **0,6 GB** (vs 34,5 GB desktop).
- CLI: **2,0 MB** (core estático, sin `Sourcetrail_lib.dll`), más boost/ICU/SQLite.
- Datos desplegados: sólo `fallback`, `license`, `python`; **sin Qt, sin Java, sin
  plugins**.
- Smoke test: `tutorial-py` → **103 nodos, 177 edges distintos, 20 archivos**, exit 0.

### 5.3 Matriz de plugins (GUI off)

| Perfil | Configure | Build | Resultado |
|---|---|---|---|
| **Embedded** (sin plugins) | 241,2 s | 70,6 s | **0 errores**; CLI 2 MB; Python 103/177/20 |
| **CLI+Java** | 34,4 s | 144,4 s | **0 errores**; plugin Java 0,26 MB; indexado Java 4.126/25.601/138 |
| **CLI+CXX** | 29,1 s | 196,6 s (primer intento) + 19 s (fix) | **0 errores** tras corregir el entry point; plugin C++ 49,43 MB; indexado C++ 99/153/21 |
| **Desktop** (GUI+CXX+Java) | 27,3 s | 328,4 s | **0 errores**; ambos plugins (49,43 / 0,26 MB); GUI exit 0; CLI C++ 99/153/21 |

Los dos fallos encontrados por la matriz eran reales y están corregidos: el
`sourcetrail_indexer` sin `WinMain` y el deploy incompleto de DLLs del core en
shared-core sin GUI. **Los plugins compilan y funcionan con la GUI desactivada.**

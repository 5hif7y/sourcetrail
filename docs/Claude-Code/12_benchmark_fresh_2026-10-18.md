# Auditoría de estado y benchmark de compilación desde cero

**Fecha:** 2026-10-18
**Autor:** Claude Code
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`

---

## 1. Auditoría del estado actual

| Elemento | Estado |
|---|---|
| Builds presentes | **Ninguno** (`build`, `build-clean`, `build_test_*` borrados) |
| Fuentes | 528 `.cpp`, 642 `.h` |
| Docs `Claude-Code` | 11 archivos (01–11) + `spikes/` con los dos spikes |
| Fork `modernized` | limpio, `e57ff8c2`, igual a `origin/modernized` |
| Repo local (`master`) | 1.995 entradas modificadas, casi todas **deletions de `build/`** (el repo local rastreaba artefactos); el fork es la fuente de verdad |
| Defaults CMake | CXX `OFF`, Java `ON`, Python `OFF`, tests `OFF`, shared core `OFF` |
| vcpkg | features `cxx`/`tests`; Qt todavía en dependencias base (el plan embedded lo mueve a `gui`) |
| Caché binaria vcpkg | **7,2 GB / 268 archives** |
| Espacio libre antes del build | D: 180,3 GB · C: 136,5 GB |

**Limpieza previa:** se liberaron ~144 GB (73,7 GB en D: y 70,7 GB en C:) borrando
builds locales, builds de la copia original y clones de verificación. Se conservaron
fuentes, docs, fork, `_tools`, `ccv-out`, la sesión de Antigravity y la caché de vcpkg.

**Comparación con la copia original:** 0 archivos faltantes en todos los árboles
(`neo-sourcetrail`, `SourcetrailPythonIndexer`, `numbat`, `sourcetrail-db`,
`haskell-indexer`, `sourcetrail-python-indexer`, `original-sourcetrail`,
`modernized-sourcetrail`).

---

## 2. Benchmark: configure + build en un solo paso

**Comando:**

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="<vcpkg>/scripts/buildsystems/vcpkg.cmake" `
  -DSOURCETRAIL_SHARED_CORE=ON `
  -DBUILD_CXX_LANGUAGE_PACKAGE=ON `
  -DBUILD_JAVA_LANGUAGE_PACKAGE=ON
cmake --build build --config Release --parallel
```

Se elige el build completo (C++ + Java + GUI + shared core) para reproducir el techo de
la compilación. La caché binaria de vcpkg está caliente; el árbol instalado se recrea
desde los archives (no se compila LLVM).

### Resultados

| Etapa | Tiempo | Resultado |
|---|---|---|
| Configure (incluye restore de vcpkg) | **1.179,4 s (19,7 min)** | exit 0; restauró ~33 GB desde la caché de 7,2 GB; **LLVM no se compiló** |
| Build | **420,1 s (7,0 min)** | exit 0, 0 errores |
| **Total** | **1.599,6 s (26,7 min)** | |

**Artefactos:** `Sourcetrail.exe` 2,79 MB, `sourcetrail_cli.exe` 0,05 MB,
`sourcetrail_cxx_plugin.dll` 49,43 MB, `sourcetrail_java_plugin.dll` 0,26 MB, indexer
Python desplegado.

**Tamaño del árbol `build/`:** **34,5 GB, 23.151 archivos** (incluye
`vcpkg_installed`; el staging temporal de vcpkg en C: se borró después).

### Smoke tests sobre el build regenerado

| Prueba | Resultado |
|---|---|
| Python `tutorial-py` con ruta relativa | exit 0; **103 nodos, 177 edges distintos, 20 archivos** |
| Java `javaparser` | exit 0; **19,8 s**, 4.126 nodos, 25.601 edges, 138 archivos |
| Plugins C++ y Java | ambos cargan |
| GUI | arranca y cierra con **exit 0** |

### Comparación histórica

| Escenario | Build limpio |
|---|---|
| Sin PCH, fallaba al link (2026-10-18 temprano) | 906,7 s |
| PCH en `lib`/`lib_gui`/`lib_cxx` | 535,1 s |
| PCH + unity en `lib`/`lib_gui`, vcpkg ya instalado | **274,7 s** |
| **Este benchmark: desde cero, con restore de vcpkg** | **420,1 s de build** (+ 1.179,4 s de configure/restore) |

---

## 3. Observaciones

1. **El costo de un build desde cero está dominado por vcpkg**, no por el proyecto:
   19,7 min de restore (33 GB desde 7,2 GB de caché) contra 7,0 min de compilación.
   Sin caché binaria, LLVM/Qt se compilarían desde fuente y el arranque sería de horas.
2. **El build del proyecto es consistente**: 420,1 s desde cero vs 274,7 s con vcpkg ya
   instalado y cachés calientes. La diferencia es caché de SO fría + Maven construyendo
   el jar (en el benchmark anterior el stamp ya existía).
3. **El perfil embedded (plan 11) evitaría Qt y LLVM**: el configure restauraría sólo
   dependencias base y el build sería CLI-only. Este benchmark es el **techo desktop**
   (GUI + C++ + Java + shared core); E1 debe medir el piso embedded.
4. **Espacio**: el build regenerado ocupa 34,5 GB en D:. Además, vcpkg dejó 34 GB de
   staging en C: (`vcpkg/packages`) y 2,8 GB en `buildtrees`; se borraron tras el build.
   Libre al cierre: **D: 145,8 GB · C: 140,3 GB**. El árbol `build/` es reproducible y
   se puede volver a borrar cuando no se necesite.
5. **Sin regresiones**: Python, Java, ambos plugins y la GUI funcionan sobre el build
   regenerado; los conteos de tutorial-py y javaparser coinciden con los históricos.

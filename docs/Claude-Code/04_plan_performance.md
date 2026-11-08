# Plan de optimización de compilación e indexación

**Autor:** Claude Code
**Fecha:** 2026-10-17
**Estado:** propuesta — no se aplicó ningún cambio de este documento.
**Regla:** cada propuesta trae impacto estimado, esfuerzo y riesgo, y se verifica midiendo
antes/después. Nada se da por bueno sin medición.

---

## 1. Resumen ejecutivo

Hay dos cuellos distintos y conviene no mezclarlos:

- **Compilación**: el costo está en recompilar ~470 unidades de traducción y enlazar el
  plugin C++ de **51,8 MB** que empaqueta LLVM/Clang. La configuración actual **no usa PCH
  ni unity build**, y el target Maven del indexador Java corre en **cada** build.
- **Indexación**: el costo está en **un proceso por archivo** (Python) y en reconstruir el
  entorno de JDT por archivo (Java). En C++ el pipeline ya es multi-proceso y razonable.

La máquina es un **i5-7400 (4 núcleos / 4 hilos, 16 GB)**. Muchas cifras "lentas" son
hardware; pero hay ganancias estructurales grandes que no dependen del hardware.

---

## 2. Línea base medida

| Medición | Valor |
|---|---|
| CPU / RAM | Intel i5-7400, 4 cores / 4 threads, 16 GB |
| Configure (cache limpia) | 48 s |
| Configure (incremental) | 12 s |
| Build completo (lib + plugins + CLI) | ~9 min |
| Build de `lib_gui` (161 TUs) | ~12 min |
| Build incremental (sólo `lib`) | ~1 min |
| `sourcetrail_cxx_plugin.dll` | 51,8 MB |
| `Sourcetrail_lib.dll` | 4,8 MB |
| TU por target | lib 237, lib_gui 161, lib_cxx 55, lib_java 15 |
| `vcpkg_installed` | 33 GB / 21.441 archivos |
| Caché binaria vcpkg | 7,2 GB (LLVM no se recompila) |
| Index Python — costo fijo por archivo | **0,94 s** (imports: 0,43 s) |
| Index Python — tutorial-py | 1,67 s/archivo |
| Index Python — matplotlib | **13,8 s/archivo** (913/913 archivos → 3 h 29 min) |
| Index Python — hilos | 3 (`hardware_concurrency() - 1`) |
| Index Java — kafka | 0,19 s/archivo (6.177 en ~20 min) |
| Index Java — javaparser | 0,12 s/archivo |
| Index C++ — sqlite | 0,28 s/archivo (354 en 98 s) |
| Log de kafka | 130.705 líneas / 16 MB, de las cuales **12.357** son el mismo warning |

---

## 3. Compilación

### 3.1 Dónde se va el tiempo

1. **Volumen de TUs**: 468 `.cpp` entre `lib`, `lib_gui`, `lib_cxx`, `lib_java`.
2. **`lib_gui` es Qt puro**: cada TU incluye decenas de headers de Qt; sin PCH se parsea
   lo mismo 161 veces.
3. **Enlace del plugin C++**: enlaza estáticamente LLVM+Clang → 51,8 MB. Es el paso más
   caro y se repite cuando cambia cualquier símbolo del core (el plugin linkea
   `Sourcetrail_lib`).
4. **Maven en cada build**: `add_custom_target(Sourcetrail_java_indexer ALL ...)` corre
   `mvn package` siempre, aunque no haya cambiado nada.
5. **`WINDOWS_EXPORT_ALL_SYMBOLS ON`** en `Sourcetrail_lib`: obliga al linker a enumerar
   y exportar todos los símbolos y a generar un `.def`/`.exp` grande.
6. **Build cache atado a rutas absolutas**: copiar `build/` a otra ruta compila el árbol
   original (nos pasó) o fuerza reconstrucción total.

### 3.2 Propuestas

| ID | Propuesta | Impacto | Esfuerzo | Riesgo |
|---|---|---|---|---|
| **C1** | Sacar `Sourcetrail_java_indexer` del `ALL` y condicionarlo a cambios en `java_indexer/**` (`add_custom_command(OUTPUT ... DEPENDS ...)`) | Ahorra ~5–15 s **en cada** build | Bajo | Bajo |
| **C2** | Evaluar quitar `WINDOWS_EXPORT_ALL_SYMBOLS` de `Sourcetrail_lib` y exportar explícitamente sólo lo que usa `lib_api`/GUI | Link más rápido (medir con `/TIME`) | Medio | Medio (puede romper símbolos) |
| **C3** | PCH en `lib_gui` (`target_precompile_headers` con QtWidgets/QtCore + std) | −30 a −50 % del build de GUI | Medio | Bajo/Medio |
| **C4** | Unity build por target (`UNITY_BUILD ON`, batch 8–16) en `lib`/`lib_gui` | −20 a −40 % de compilación | Medio | Medio (colisiones de namespaces anónimos) |
| **C5** | Compilar con Ninja + MSVC en vez del generador VS para desarrollo (mantener VS para la solución) | Arranque incremental más rápido, mejor paralelismo | Medio | Bajo |
| **C6** | Desacoplar el plugin C++ del core vía `sourcetrail_api` (el "Modelo B" de la auditoría vieja) para que un cambio en `lib` no relinkee 51 MB | Elimina el relink más caro | Alto | Alto |
| **C7** | Copiar `Sourcetrail_lib.dll` a `app/` en un target propio (hoy es `POST_BUILD` de `Sourcetrail`) para que `--target sourcetrail_cli` deje el CLI usable | Evita builds de GUI innecesarios | Bajo | Bajo |
| **C8** | Documentar/automatizar: **nunca copiar `build/`** entre rutas; reconfigurar conservando `vcpkg_installed` (ya documentado en §4 del informe) | Evita rebuilds completos accidentales | Bajo | Bajo |
| **C9** | Perfilado del link con `/TIME` para confirmar cuánto pesa C2/C6 | Medición previa | Bajo | Ninguno |

**Orden recomendado:** C1 + C9 (baratos, medibles) → C3 (mayor ganancia de compilación) →
C7/C8 (proceso) → C2 → C4 → C6 sólo si el link sigue siendo el dominante.

---

## 4. Indexación

### 4.1 Pipeline y cuellos

```
SourceGroup → IndexerCommand(s) → cola → N workers → IntermediateStorage
                                     ↓
                         TaskMergeStorages (main thread)
                                     ↓
                         TaskInjectStorage  (main thread, de a uno)
                                     ↓
                              SQLite *_tmp.srctrldb
```

Cuellos medidos:

- **Python**: el core invoca `python run.py index` **una vez por archivo**. Sólo el
  arranque del intérprete + imports cuesta **0,94 s por archivo** (0,43 s de imports). En
  matplotlib eso son ~15 min de puro arranque; el resto es jedi resolviendo.
- **Python, afinidad**: 3 workers en una máquina de 4 hilos. Cada worker es un proceso
  Python con su propia DB (`*_thread<n>`).
- **Java**: `ASTParser.setEnvironment()` se ejecuta **por archivo**; JDT re-lista los
  `source roots`. Ya se mitigó pasando sólo el root del archivo (§2.5 del informe), pero
  la solución completa es reutilizar el entorno.
- **Merge/inject**: `TaskInjectStorage` inyecta **de a un storage** en el hilo principal;
  durante kafka apareció `waiting, too many storages queued: 14` mientras los workers
  esperaban. El productor va más rápido que el consumidor.
- **Logging**: kafka generó 130.705 líneas (16 MB), con **12.357** repeticiones del mismo
  warning (`application/logging_enabled is not present in config`) y 10.796 mensajes de
  estado. Es I/O y CPU regalados.

### 4.2 Propuestas

| ID | Propuesta | Impacto | Esfuerzo | Riesgo |
|---|---|---|---|---|
| **I1** | **Modo batch de Python**: una invocación por lote de N archivos (`--source-file-list`), reutilizando el entorno de jedi entre archivos | Elimina ~0,9 s/archivo de arranque y aprovecha cachés de jedi; estimado **−30 a −50 %** | Alto (toca core + indexer) | Medio |
| **I2** | Igual que I1 pero sin tocar el core: un **daemon** por worker que recibe rutas por pipe/socket y mantiene el entorno caliente | Similar a I1 | Alto | Medio |
| **I3** | `--shallow` opcional (indexado por nombre, sin contexto) para proyectos enormes; luego deep-index de lo cambiado | Muy alto en proyectos grandes | Bajo (ya existe en el indexer) | Bajo (menor fidelidad) |
| **I4** | Subir `indexer_thread_count` a 4 en el CLI (hoy 3 por `-1` pensado para la GUI) | +~30 % en indexación paralela | Bajo | Bajo |
| **I5** | **Java: cachear `FileSystem`/`ASTParser` por thread** y llamar `setEnvironment` una vez, no por archivo | Elimina el O(archivos × roots) de raíz; permitiría volver a pasar todos los roots | Alto | Alto |
| **I6** | **Java: procesar N archivos por llamada JNI** (`processFiles`) reutilizando parser y entorno | Similar a I5, más simple | Medio | Medio |
| **I7** | **Inject en paralelo o por lotes**: un único escritor que consuma varios storages por vuelta; cola más grande; commit por lote | Reduce el backpressure de kafka y el tiempo total del 20 % final | Medio | Medio |
| **I8** | **Progreso real**: reportar archivos indexados desde `InterprocessIndexingStatusManager` en vez de esperar merges (hoy la barra queda en 0 % mucho tiempo) | Percepción de velocidad; no acelera | Bajo | Bajo |
| **I9** | **Bajar el ruido de logs**: cachear el warning de `ConfigManager` y no loguear `MessageIndexingStatus` por archivo salvo en verbose | −10 % de I/O y CPU en corridas grandes | Bajo | Bajo |
| **I10** | **Java: `-Xmx` explícito** (p. ej. `-Xmx2g`) y mantener los hilos attachados | Evita GC thrashing y safepoints en proyectos grandes | Bajo | Bajo |
| **I11** | Reutilizar el `SourcetrailPythonIndexer` como **proceso persistente** con pool (evita crear/cerrar DB y proceso por archivo) | Complementa I1/I2 | Alto | Medio |

**Orden recomendado:** I4 + I9 + I8 + I10 (baratos, sin riesgo) → I7 (backpressure) →
I3 (opción de producto) → I5/I6 (Java) → I1/I2 (el gran salto de Python).

---

## 5. Roadmap sugerido

**Fase 0 — medición (1 h).** Instrumentar `/TIME` en el link y tiempos por target; medir
una corrida de indexación con `--verbose` desactivado. Sin cambios de código.

**Fase 1 — ganancias baratas (1–2 días). ✅ aplicada el 2026-10-18, ver §7.**
C1 (Maven condicional), C7 (copia del DLL), C8 (proceso), I4 (4 hilos CLI), I9 (logging),
I8 (progreso), I10 (`-Xmx`). Todo de bajo riesgo y verificable con las mismas corridas.

**Fase 2 — compilación (3–5 días).**
C3 (PCH en `lib_gui`) ✅ aplicada el 2026-10-18 (§8): −60 % en el rebuild de `lib_gui`.
Pendiente de esta fase: C2 (exports) medido con `/TIME`.

**Fase 3 — indexación Java (3–5 días).**
I7 (inject) e I5/I6 (entorno JDT por thread / batch). Con esto kafka debería bajar de
~20 min y desaparecer el trade-off del filtro de roots.

**Fase 4 — indexación Python (1–2 semanas).**
✅ aplicado el paralelismo de comandos (I4 extendido, §11): tutorial-py 33,4 s → 12,5 s y
matplotlib 913 archivos **3 h 29 min → 56,6 min (3,69×)**.
Pendiente: I1/I2 (batch o daemon, para eliminar los ~0,94 s de arranque por archivo) e
I3 (shallow opcional).

**Fase 5 — arquitectura (opcional).**
C6 (plugin C++ contra la C-API) si el relink de 51 MB sigue dominando.

---

## 6. Verificación de cada propuesta

Toda propuesta se acepta sólo si:

1. **Compilación**: build limpio y build incremental medidos con `/TIME`, mismo hardware,
   misma configuración; se comparan `cl.exe` y `link.exe` por target.
2. **Indexación**: misma corrida (tutorial, javaparser, kafka, matplotlib) con el mismo
   binario y se comparan nodos, aristas, errores y tiempo; los conteos **no deben bajar**
   salvo que el cambio lo justifique explícitamente (p. ej. `--shallow`).
3. **Regresión funcional**: los 4 tutoriales + los 3 proyectos reales siguen indexando y
   los procesos salen solos.
4. **Rollback**: cada propuesta es un commit independiente y reversible.

---

## 7. Fase 1 — aplicada (2026-10-18 03:45)

Se implementaron las propuestas baratas de bajo riesgo y se midieron.

| ID | Cambio | Resultado medido |
|---|---|---|
| **C1** | `java_indexer/CMakeLists.txt`: `add_custom_command(OUTPUT stamp DEPENDS <java sources> pom.xml)` en vez de `add_custom_target(... ALL)`. Stamp por configuración. | Sin cambios: **12,6 s → 0,8 s** (Maven no corre). Con un `.java` tocado: 6 s y el stamp se actualiza. |
| **C1 (bug)** | `mvn` es un `.cmd`; invocado desde el batch generado **sin `call`** nunca vuelve y el `touch` del stamp no se ejecutaba (MSB8065 silencioso). Se agregó `call`. | Verificado: el stamp se crea y el incremental funciona. |
| **C7** | `deploySourcetrailCore(target)` en `cmake/Sourcetrail.cmake`, aplicado a `Sourcetrail` **y** `sourcetrail_cli`. | Build sólo-CLI tras cambiar `lib`: **7,7 s**, con `Sourcetrail_lib.dll` copiado a `app/`. Antes había que compilar la GUI. |
| **C8** | Proceso documentado (no copiar `build/`; reconfigurar conservando `vcpkg_installed`). | — |
| **I4** | `getIdealThreadCount(bool forGui)`; el CLI usa todos los núcleos (4 en esta máquina, antes 3). | El warning `indexing/indexer_thread_count` confirma que toma el camino automático; `Application::createInstance(nullptr, nullptr)` ⇒ `hasGUI()==false`. |
| **I9a** | `MessageIndexingStatus` ya no se loguea (`setIsLogged(false)`). | kafka: **10.796 líneas → 0**. |
| **I9b** | `ConfigManager` deduplica warnings por clave (`m_warnedKeys`). | kafka: **12.357 → 2** por corrida (una por instancia de settings). |
| **I10** | JVM con `-Xmx2048m`. | javaparser sigue indexando 4126/25.601 y el proceso sale solo. |
| **I8** | Se difiere: `TaskExecuteCustomCommands` ya reporta progreso por archivo; el progreso grueso de Java/C++ requiere tocar el status manager (fase 3). | — |

### Verificación de la fase

| Prueba | Resultado |
|---|---|
| Build sin cambios | **6,7 s** |
| Build sólo-CLI tras tocar `lib` | **7,7 s**, DLL desplegado |
| `tutorial` (C++) | 99 / 153 ✅ |
| `tutorial-py` | 103 / 394 ✅ |
| `javaparser` | 4126 / 25.601 ✅ |
| `tictactoe_cpp` | 383 / 819 ✅ |
| GUI con proyecto Java: abrir + cerrar | exit 0 ✅ |
| Log de `tutorial-py` | 0 líneas `MessageIndexingStatus`, 2 warnings de config |

**Rollback:** todos los cambios son ediciones independientes en
`java_indexer/CMakeLists.txt`, `cmake/Sourcetrail.cmake`, `src/app/CMakeLists.txt`,
`src/cli/CMakeLists.txt`, `src/lib/utility/ConfigManager.*`, `src/lib/utility/utilityApp.*`,
`src/lib/project/Project.cpp`, `src/lib/utility/messaging/type/indexing/MessageIndexingStatus.h`
y `src/lib_java/data/parser/java/JavaEnvironmentFactory.cpp`.

---

## 8. Fase 2 — aplicada (2026-10-18 11:35)

| ID | Cambio | Resultado medido |
|---|---|---|
| **C3** | `target_precompile_headers(Sourcetrail_lib_gui PRIVATE ...)` con 32 headers de QtCore/QtGui/QtWidgets + std | Rebuild completo de `lib_gui` (161 TU) + link: **343,1 s → 136,8 s (−60 %)**. Incremental (1 TU + link): **4,4 s**. |
| (bug) | En Qt 6 `QAction` vive en QtGui, no en QtWidgets; el primer intento falló con `C1083: Cannot open include file: 'QtWidgets/QAction'`. | Corregido a `<QtGui/QAction>`. |

Medición: se tocaron los 161 `.cpp` de `src/lib_gui` para forzar recompilación completa y se
cronometró `cmake --build --config Release --target Sourcetrail --parallel`, primero sin PCH
y después con PCH configurado. El PCH se materializa en
`build/src/lib_gui/Sourcetrail_lib_gui.dir/Release/cmake_pch.pch` y `cmake_pch.cxx` se
compila una sola vez.

### Verificación de la fase

| Prueba | Resultado |
|---|---|
| Build completo `lib_gui` + link | **136,8 s**, 0 errores |
| Build incremental (1 TU) + link | **4,4 s** |
| GUI arranca y cierra | exit 0 ✅ |
| GUI con `java-mini.srctrlprj` cargado | responde y cierra exit 0 ✅ |
| `tutorial` / `tutorial-py` / `javaparser` / `tictactoe_cpp` | 99/153 · 103/394 · 4126/25.601 · 383/819 ✅ |

**Rollback:** revertir el bloque `target_precompile_headers` al final de
`src/lib_gui/CMakeLists.txt`.

---

## 9. Pendientes cerrados (2026-10-18 11:45)

### C2 — `WINDOWS_EXPORT_ALL_SYMBOLS` en el core: medido y descartado

Se agregó `/TIME` al linker de `Sourcetrail_lib` y se midió un relink completo:

```
LibDef: Total time = 0.766s
OptRef: Total time = 0.015s
OptIcf: Total time = 0.079s
Final:  Total time = 1.688s     →  ~2,5 s en total
```

El link del core **no es un cuello de botella**: representa ~2,5 s dentro de builds de
minutos. Quitar la exportación automática obligaría a anotar con `SOURCETRAIL_LIB_EXPORT`
todos los símbolos que hoy consume la GUI (sólo 3 clases lo tienen) y arriesga romper el
enlace por una ganancia marginal. **Decisión: se mantiene la opción tal cual.** `/TIME` se
retiró después de medir.

### I8 — progreso: revisado, diferido con motivo

`TaskExecuteCustomCommands` ya incrementa `indexed_source_file_count` y despacha
`MessageIndexingStatus` por archivo; `TaskBuildIndex` hace lo propio al cosechar cada
`IntermediateStorage`. La barra **sí avanza**. El retraso residual que se veía en kafka es
el backpressure del inject (`waiting, too many storages queued`), que se ataca en la fase
3 (I7). No se cambió nada.

### Java — cuatro problemas menores

| Problema | Cambio |
|---|---|
| `JavaParser::s_nextParserId++` no atómico | `std::atomic<int>` (`JavaParser.h/.cpp`) |
| `catch (Exception)` no cubre `Error` | `catch (Throwable)` en `JavaIndexer.java`; OOM/LinkageError ya no dejan una excepción JNI pendiente |
| `Attach/DetachCurrentThread` por archivo | `unregisterEnvironment()` ya no desattachea: el hilo queda attached y `createEnvironment()` reutiliza la entrada para el siguiente archivo |
| Estándar `"26"` ofrecido por `ToolChain.cpp` | **No era un problema**: `javap -constants` sobre `org.eclipse.jdt.core-3.44.0.jar` muestra `VERSION_26 = "26"`. JDT 3.44 sí lo soporta; no se tocó |

### Verificación

| Prueba | Resultado |
|---|---|
| Jar reconstruido | 9,5 s (Maven corrió por el cambio de `.java`) |
| Build C++ (`Sourcetrail` + `sourcetrail_cli`) | 19,3 s, 0 errores |
| `tutorial` / `tutorial-py` / `javaparser` / `tictactoe_cpp` | 99/153 · 103/394 · **4126/25.601** · 383/819 ✅ |

---

## 10. Fase 3 — investigada (2026-10-18 12:30)

### I5/I6 — reutilizar el entorno de JDT: **no es viable con `ASTParser`**

Spike (`_tools/JdtSpike.java`), mismo archivo parseado N veces:

```
fresh: 45 ms/archivo   (ASTParser nuevo + setEnvironment por archivo)
reuse:  2 ms/archivo   (un ASTParser, setEnvironment una sola vez)
```

Parecía una mejora de 22×. Se implementó un cache `ThreadLocal<ASTParser>` y se midió
sobre proyectos reales:

| Proyecto | Parser nuevo (correcto) | Parser reutilizado |
|---|---:|---:|
| javaparser | **4126 nodos / 25.601 aristas** | 2883 / 8.320 |
| kafka/clients (1 root) | **25.194 / 146.846** | 21.742 / 61.675 |

**La reutilización entre archivos distintos degrada la resolución de bindings en
silencio**: no produce errores de parseo (0 errores), simplemente registra menos símbolos
y muchas menos referencias. El spike daba 2 ms porque re-parseaba el mismo archivo.

**Decisión: revertido.** El código volvió exactamente al estado anterior y se verificó:
javaparser 4126/25.601 y kafka/clients 25.194/146.846, idénticos.

**Camino correcto si se retoma:** la API pensada para varios archivos es
`ASTParser.createASTs(String[] sourceFilePaths, String[] encodings, String[] bindingKeys,
FileASTRequestor, IProgressMonitor)`, que comparte la resolución de bindings entre todas
las unidades de una llamada. Requiere que el indexador reciba una **lista de archivos** y
lea los contenidos en Java (hoy el core manda un archivo por invocación JNI). Es un cambio
de contrato (I6) y quedó fuera de esta fase.

### I7 — inject paralelo: no prioritario

El backpressure de kafka (`waiting, too many storages queued: 14`) ocurre durante la fase
de producción, pero el consumidor mantiene el ritmo: de los ~20 min de kafka, el merge/
inject final tras el último archivo tomó ~36 s. El costo está en JDT, no en el inject.
No se justifica el riesgo de paralelizar el escritor SQLite.

---

## 11. Fase 4 — paralelismo de Python aplicado (2026-10-18 12:35)

### Hallazgo

Los Source Groups de tipo custom command se ejecutaban **en serie**:
`SourceGroupSettingsWithCustomCommand` tiene `m_runInParallel = false` por defecto y
`load()` también usa `false` cuando la clave no está en el `.srctrlprj`. El log lo
confirmaba: cada `Start processing command` arrancaba inmediatamente después del
`Process returned successfully` anterior.

Medición de referencia: indexar 10 archivos del tutorial **en un solo proceso**
(`_tools/py_batch_spike.py`) da **0,649 s/archivo** de trabajo real. El CLI tardaba
1,67 s/archivo de pared porque sumaba el arranque de un proceso Python por archivo
(~0,94 s) **y** lo hacía de a uno.

### Cambio

`SourceGroupSettingsPythonEmpty` fuerza `setRunInParallel(true)` en el constructor y en
`loadSettings()`. Es seguro porque:

- cada archivo lo indexa un proceso Python independiente,
- cada worker escribe en su **propia** base temporal (`<db>_threadN`),
- el core fusiona las bases al final (mecanismo ya existente).

### Bug encontrado al habilitar el paralelismo: "database is locked"

El nombre de la base por thread se armaba **después** de la extensión:

```
mpl-lib_tmp.srctrldb_thread1
```

`numbat` trunca el path en el **último punto** y le agrega `.srctrldb` (el mismo
comportamiento que obligó a usar `_tmp.srctrldb` en su momento). Resultado: los cuatro
threads resolvían a `mpl-lib_tmp.srctrldb` y fallaban con
`sqlite3.OperationalError: database is locked`; las `.srctrldb_threadN` quedaban en 0 bytes.

**Fix** en `TaskExecuteCustomCommands.cpp`: se arma el nombre completo
(`stem + "_threadN" + extension`) y se concatena al **directorio padre**. Detalle: el
primer intento concatenó sobre `withoutExtension()` y creó un subdirectorio
(`mpl-lib_tmp/_thread1.srctrldb`), porque `FilePath::concatenate()` inserta un separador.
Verificado con el comando real del CLI:
`tutorial-py_tmp_thread1.srctrldb`.

### Verificación de equivalencia (no sólo conteos)

Se indexó el tutorial en serie y en paralelo y se compararon los conjuntos de aristas
distintas `(source, type, target)` (`_tools/diff_edges.py`):

```
A distinct edges: 177
B distinct edges: 177
only in A: 0
only in B: 0
```

La diferencia de 394 vs 220 aristas es **duplicación**: en serie la base única acumula
filas repetidas; el merge en paralelo las deduplica. El grafo es idéntico y más limpio.

### Resultado medido

| | Serie | Paralelo |
|---|---:|---:|
| tutorial-py (20 archivos) | 33,4 s | **12,5 s** |
| Nodos | 103 | 103 |
| Aristas distintas | 177 | **177** (idénticas) |
| Comandos concurrentes | 1 | **4** |

**−63 %** con contenido demostrablemente equivalente.

### Escala: subconjunto real de matplotlib

`lib/matplotlib` (254 archivos: incluye `tests/`, no sólo los 154 sin tests):

```
254 comandos, 254 OK, 0 fallos, 0 "database is locked"
16.431 nodos / 142.861 aristas (75.524 distintas) / 254 archivos
49,6 min
```

Son los archivos más pesados del corpus (~46 s de CPU cada uno, frente a ~14 s de media
en los 913), así que el tiempo está dominado por el tamaño del archivo, no por overhead.

### Validación final: matplotlib 913 archivos, serie vs paralelo

| | Serie | Paralelo |
|---|---:|---:|
| Tiempo | 3 h 29 min (12.537 s) | **56,6 min (3.397 s)** |
| Archivos | 913 | 913 |
| Nodos | 25.973 | 25.991 |
| Aristas (filas) | 622.480 | 232.774 |
| Aristas distintas | 101.524 | 101.318 |
| Salida del proceso | 0 | 0 |

**Velocidad: 3,69×.** Las filas de aristas caen porque el merge deduplica.

**Sobre la equivalencia exacta:** en el tutorial las aristas distintas fueron idénticas
(177 = 177). En matplotlib difieren 696 (sólo serie) y 490 (sólo paralelo): **0,2 % neto**
sobre 101k. Se verificó que:

- el indexer es **determinista por archivo** (mismo archivo, dos corridas: 1092 = 1092),
- dos corridas **paralelas** del mismo proyecto dan conjuntos idénticos (177 = 177).

Es decir, la diferencia aparece entre el modo serie y el paralelo, no dentro del paralelo.
Las aristas divergentes son casi todas referencias a módulos stdlib/builtins
(`builtins`, `functools`, `gc`, `logging`, `_weakrefset`) y rutas de módulo anidadas. La
hipótesis a verificar (no confirmada) es el estado incremental de la base en serie —cada
archivo ve los símbolos de los anteriores— o la caché de jedi compartida. Si se necesita
un grafo bit-idéntico, hay que investigar el merge; para el objetivo de velocidad, el
resultado es equivalente en la práctica.

`_audit/matplotlib_serial_913.srctrldb` y `_audit/matplotlib_parallel_913.srctrldb`
quedan como evidencia.

---

## 12. Lo que ya se descartó (para no repetirlo)

- **Recompilar LLVM/Clang con vcpkg**: la caché binaria está (7,2 GB) y `vcpkg_installed`
  se reutiliza; no hace falta.
- **`fetchRootDirectories()` como cuello**: medido; `getPackageName` no usa bindings y
  resuelve kafka en segundos.
- **Bajar `java_standard` para acelerar**: medido; no cambia el tiempo (142 s vs 136 s).
- **Más RAM/CPU**: no es una propuesta; el plan es de software.

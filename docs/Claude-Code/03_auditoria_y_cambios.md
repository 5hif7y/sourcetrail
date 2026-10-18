# Auditoría de estado y cambios aplicados

**Autor:** Claude Code
**Fecha:** 2026-10-16
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Commit base:** `20ffe0b WIP: Backup antes de reestructuracion hibrida`
**Alcance acordado:** que C++, Python y Java indexen correctamente los proyectos de ejemplo
(tutorial, tutorial-py, javaparser, tictactoe_cpp). No incluye los problemas de escala de
proyectos reales (kafka, matplotlib).

---

## 1. Estado de partida (medido, no supuesto)

| Prueba | Resultado inicial |
|---|---|
| `sourcetrail_cli.exe` arranca | ✅ carga `sourcetrail_cxx_plugin.dll` y `sourcetrail_java_plugin.dll` |
| `Sourcetrail.exe` arranca | ✅ ventana principal, proceso responde |
| Tutorial C++ (`tutorial.srctrlprj`) | ✅ **99 nodos, 153 aristas, 20 archivos, 807 LOC, 0 errores** |
| Tutorial Python (`tutorial-py.srctrlprj`) | ❌ **0 nodos** — `run.py` no encontrado |
| javaparser (Java) | ❌ **0 nodos** — los comandos se descartan en la IPC |

El árbol traía `build/vcpkg_installed` (33 GB) y un `build/Release/app` completo, así que
**no hizo falta recompilar LLVM**. El build cache apuntaba a `<ORIGINAL_WORKSPACE>\...`;
fue necesario reconfigurar para esta copia (ver §5).

---

## 2. Bugs confirmados, con evidencia

### 2.1 Python: el indexer no se encuentra

```
ERROR: Python indexer not found at:
  .../neo-sourcetrail/build/SourcetrailPythonIndexer/run.py
command "python ".../build/SourcetrailPythonIndexer/run.py" index ..." returned code "2".
Graph: 0 Nodes / 0 Edges
```

`SourceGroupSettingsPythonEmpty.cpp` hacía `getParentDirectory()` dos veces desde el
directorio del ejecutable. Con el layout `build/Release/app` eso da
`neo-sourcetrail/build/`, y con el layout `bin/app` da `neo-sourcetrail/`. En ninguno de
los dos existe `SourcetrailPythonIndexer/` (es hermano de `neo-sourcetrail`, un nivel más
arriba). La variable de entorno `SourcetrailPythonIndexer` no estaba definida.

### 2.2 Python: el tutorial era C++ con extensión `.py`

No era un bug del indexador. `bin/app/user/projects/tutorial-py/src/` había sido generado
por una conversión mecánica que sólo borraba los `#include`:

```python
# my_next_step.py (antes)
def my_next_step(int foo):
	the_central_hub();
	int bar = 2 * foo;
	return bar;
```

y `the_central_hub.py` tenía `SearchTutorial* the_challenge_of_search;`,
`start_exploring.py` tenía `StartExploring()` sin `def`, `utility.py` tenía
`typedef unsigned int uint;`, etc. Además `my_first_step.py` **no importaba**
`my_next_step`.

Verificación con el indexer real sobre `my_first_step.py`:

```
nodes:  file, module my_first_step, function my_first_step, "unsolved symbol"
edges:  my_first_step → "unsolved symbol"   (REFERENCE)
```

Ese nodo `unsolved symbol` es el que la GUI intercepta en
`ActivationController::handleMessage()` y muestra como tooltip en vez de navegar: el
síntoma exacto reportado ("my_next_step no es seleccionable").

### 2.3 Java: los comandos nunca llegan al indexer

Con un proyecto Java de 2 archivos:

```
SharedIndexerCommand::fromLocal() | ERROR: Trying to push unhandled type of IndexerCommand
  for file: .../Hello.java. Type string is: indexer_command_java. It will be ignored.
SharedIndexerCommand::fromShared() | ERROR: Cannot convert shared IndexerCommand ...
  The type is unknown or no factory is registered.
Finished indexing: 0/2 source files indexed      →  0 Nodes / 0 Edges
```

Causa: `IndexerCommand` declara `populateSharedCommand()` con implementación vacía por
defecto. `IndexerCommandCxx` la sobrescribe y registra una factory
(`SharedIndexerCommand::CXX`); **`IndexerCommandJava` no hacía ninguna de las dos cosas**.
Todo comando Java quedaba con `type == UNKNOWN` y se descartaba al entrar a la memoria
compartida.

Esto ocurre también en modo in-process: aun con `multiProcess == false`, la cola de
comandos pasa por `InterprocessIndexerCommandManager`, que exige serialización.

> Esto reemplaza la hipótesis de los informes previos (heap del JVM, JDT 3.44 vs
> `"26"`, concurrencia JNI). Esos problemas existen, pero no eran la causa de los 0 nodos.

### 2.4 Java: el proceso no termina

El indexado **termina** (log: `Finished indexing ... 0 errors`), imprime
`Sourcetrail CLI finished.`, y acto seguido el proceso queda vivo indefinidamente:

```
SharedMemoryGarbageCollector::stop() | INFO: delete garbage collector memory: grbg_cllctr
  ← último log; el proceso nunca sale (1 thread, CPU plana)
```

Ocurría sólo con proyectos Java. El cierre llama `Application::destroyInstance()`, y
después, en la destrucción de estáticos, `~JavaEnvironmentFactory()` hacía
`m_jvm->DestroyJavaVM()`, que espera a que terminen todos los hilos no-daemon del JVM.
Se diagnosticaron dos procesos zombi (no se podían matar ni con `taskkill /F`; hubo que
usar `Win32_Process.Terminate`).

### 2.5 Java a escala: JDT re-lista todos los source roots por archivo

Con el bug §2.3 arreglado, kafka (6177 archivos) por fin encolaba comandos, pero el
indexado se arrastraba: a los 5 minutos la base temporal se quedó en 104 MB y el proceso
seguía vivo sin avanzar. Un `jstack` contra el JVM embebido lo ubicó:

```
"Thread-1073" RUNNABLE
  at java.io.WinNTFileSystem.list0(Native Method)
  at org.eclipse.jdt.internal.compiler.batch.ClasspathDirectory.computeDirectoryList(...)
  at ...FileSystem.getModulesDeclaringPackage(...)
  at ...CompilationUnitScope.faultInImports(...)
  at com.sourcetrail.JavaIndexer.processFile(JavaIndexer.java:107)
```

`utility::fetchRootDirectories()` descubre **134 roots** en kafka (uno por módulo/carpeta
`src/main/java`). Ese array se pasa entero a `ASTParser.setEnvironment()` **para cada
archivo**, y JDT recorre cada root recursivamente en cada resolución de import. El costo
total es O(archivos × archivos del proyecto).

Medición que lo confirma: indexar **sólo** `kafka/clients` (1345 archivos, 1 root) tarda
136 s; bajar el `java_standard` a 8 no cambia nada (142 s), así que no era la resolución
de módulos.

**Fix:** `JavaIndexer.filterSourcesForFile()` deja en `sources` únicamente los roots que
contienen el archivo que se está indexando (con fallback al listado completo si ninguno
coincide). El costo por archivo pasa a depender del tamaño de su módulo, no del proyecto.

### 2.6 Python a escala: stack overflow dentro de jedi

Indexando matplotlib, un archivo (`lib/matplotlib/backend_bases.py`, 3820 líneas) tumbaba
el proceso Python del indexador:

```
command "python .../run.py index --source-file-path=.../backend_bases.py ..." returned code "-1073741571"
```

`-1073741571` = `0xC00000FD` = **STATUS_STACK_OVERFLOW**. Reproducido a mano con
`python -X faulthandler`, el stack muestra cientos de frames idénticos:

```
File ".../jedi/api/helpers.py", line 188 in filter_follow_imports
File ".../jedi/api/helpers.py", line 188 in filter_follow_imports
...
```

`filter_follow_imports()` de jedi 0.20 se llama a sí misma siguiendo imports **sin
guarda de profundidad**; con un ciclo de imports revienta el stack. No es una
`RecursionError` de Python (que sería capturable), es un overflow del stack nativo.

**Fix:** `AstVisitor.gotoDefinitions()` baja `sys.setrecursionlimit` a 300 alrededor de
`script.goto(...)`, de modo que jedi levante `RecursionError` antes del overflow, y en ese
caso reintenta con `follow_imports=False`. El archivo se indexa igual (con definiciones a
nivel import), en vez de perderse:

```
backend_bases.py  →  631 nodos / 7.286 aristas  (antes: crash, 0)
```

Además se corrigió un `AttributeError: 'NoneType' object has no attribute 'split'` en
`getNameHierarchyFromFullNameOfDefinition` / la rama `builtins` de
`getNameHierarchyOfClassOrFunctionDefinition`, donde `definition.full_name` o
`definition.name` pueden ser `None` (p. ej. al resolver `__name__`).

### 2.7 Python a escala: numbat abre los archivos con la codificación ANSI

Además del stack overflow, otros 8 archivos de matplotlib fallaban con:

```
UnicodeDecodeError: 'charmap' codec can't decode byte 0x90 in position 1490
  at numbat/api.py:1095 in record_file
  at sourcetraildb.py:165 in recordFile
```

`numbat` hace `open(path, 'r').readlines()` **sin especificar encoding**, así que en Windows
usa la codepage ANSI (cp1252) y revienta con cualquier byte UTF-8 que cp1252 no defina.
Los 9 archivos afectados (entre ellos `axes/_base.py`, `axes/_axes.py`,
`backend_bases.py`) contienen caracteres no ASCII.

**Fix:** el comando del Source Group Python pasa `-X utf8`:

```
python -X utf8 "<...>/run.py" index --source-file-path=... --database-file-path=...
```

Eso hace que todo `open()` por defecto use UTF-8. Verificado con los 8 archivos que
quedaban: **todos indexan** (1.245 nodos / 24.757 aristas en conjunto) y `tutorial-py`
sigue dando 103/394.

### 2.8 Wizard: falta el `case` de Python

`QtProjectWizard::selectedProjectType()` recorría
`C_EMPTY`, `CXX_*`, `JAVA_*`, `CUSTOM_COMMAND`, `UNKNOWN`. **No había
`case SourceGroupType::PYTHON_EMPTY`**, así que `settings` quedaba `nullptr`, no se llamaba
`createSourceGroup()` y nunca se ejecutaba `setNextEnabled(true)`: el botón
"Next"/"Create" quedaba gris para siempre al elegir "Empty Python Source Group".

Confirmado en el código, no clickeado (ver §6).

---

### 2.9 Python: indexado en serie y nombres de base por thread (2026-10-18)

Dos problemas encadenados, encontrados al buscar la lentitud de matplotlib:

1. **Los Source Groups de tipo custom command corren en serie**:
   `SourceGroupSettingsWithCustomCommand` tiene `m_runInParallel = false` por defecto y
   `load()` también usa `false` si la clave no está. El log lo mostraba: cada
   `Start processing command` arrancaba justo después del `Process returned successfully`
   anterior. En un i5 de 4 núcleos se indexaba **un archivo a la vez**.
   **Fix:** `SourceGroupSettingsPythonEmpty` fuerza `setRunInParallel(true)`.

2. **La base temporal por thread se nombraba después de la extensión**:
   `foo_tmp.srctrldb_thread1`. `numbat` trunca en el último punto y agrega `.srctrldb`, así
   que los cuatro threads escribían en `foo_tmp.srctrldb` y fallaban con
   `sqlite3.OperationalError: database is locked`.
   **Fix:** `TaskExecuteCustomCommands.cpp` inserta el sufijo **antes** de la extensión
   (`foo_tmp_thread1.srctrldb`) usando `FilePath::withoutExtension()`.

**Verificación de equivalencia:** se indexó el tutorial en serie y en paralelo y se
compararon las aristas distintas `(source, type, target)`: **177 = 177, cero diferencias**.
La caída de 394 a 220 aristas es deduplicación del merge, no pérdida de datos.

**Resultado:** tutorial-py 33,4 s → **12,5 s** (−63 %) con 4 comandos concurrentes.

---

## 3. Cambios aplicados

| Archivo | Cambio | Motivo |
|---|---|---|
| `src/lib_gui/qt/project_wizard/QtProjectWizard.cpp` | `#include "SourceGroupSettingsPythonEmpty.h"` + `case SourceGroupType::PYTHON_EMPTY:` que crea los settings | §2.8 |
| `src/lib/settings/source_group/type/SourceGroupSettingsPythonEmpty.cpp` | (a) helper `findPythonIndexerFilePath()`: respeta la env var y, si no, **sube directorios** desde el ejecutable hasta encontrar `SourcetrailPythonIndexer/run.py` (máx. 6 niveles). (b) helper `makePythonIndexerCommand()` con `python -X utf8` | §2.1, §2.7 |
| `src/lib_java/data/indexer/IndexerCommandJava.h/.cpp` | `populateSharedCommand()` + factory registrada para `SharedIndexerCommand::JAVA`, espejando `IndexerCommandCxx` | §2.3 |
| `src/lib_java/data/parser/java/JavaEnvironmentFactory.cpp` | `~JavaEnvironmentFactory()` ya no llama `DestroyJavaVM()` | §2.4 |
| `java_indexer/src/main/java/com/sourcetrail/JavaIndexer.java` | `filterSourcesForFile()`: sólo el root del archivo indexado | §2.5 |
| `SourcetrailPythonIndexer/indexer.py` | (a) `gotoDefinitions()` con `setrecursionlimit(300)` + fallback sin `follow_imports`; (b) `full_name`/`name` nulos en la resolución de nombres | §2.6 |
| `bin/app/user/projects/tutorial-py/src/*.py` (21 archivos) | Reescritura completa como traducción fiel del tutorial C++ | §2.2 |

### Sobre la reescritura de `tutorial-py`

Se mantuvieron los 21 módulos, los comentarios del tutorial y las mismas relaciones
(calls, imports, herencia, miembros). Cambios inevitables por el lenguaje:

| C++ | Python |
|---|---|
| `#include "x.h"` | `from x import ...` |
| `void f()` | `def f():` |
| `int value;` | `value: uint = 0` |
| `static void f()` | `@staticmethod` |
| `namespace a { class NameConflict }` | `class a: class NameConflict:` |
| `ClassWithHiddenMembers::this_last_member` | `ClassWithHiddenMembers.this_last_member` |
| `void in()` | `def in_()` — `in` es palabra reservada |
| `int (*f)() = [](){...}` | `f = lambda: ...` |

Verificado con `py_compile` y con la ejecución real de `start_tour()` → `0`.

---

## 4. Verificación

Recompilado `Sourcetrail` + `sourcetrail_cli` en Release desde esta copia y ejecutado el
CLI sobre los cuatro proyectos de ejemplo. Todos terminaron por sí solos (sin matar el
proceso) y todos reportaron `0 Errors / 0 Fatal Errors`:

| Proyecto | Salió solo | Tiempo | Nodos | Aristas | Archivos | `unsolved symbol` |
|---|:---:|---:|---:|---:|---:|---:|
| `tutorial` (C++) | ✅ | 3,2 s | 99 | 153 | 20 | 0 |
| `tutorial-py` (Python) | ✅ | 33,4 s | 103 | 394 | 20 | **0** |
| `javaparser` (Java) | ✅ | 16,9 s | 4126 | 25601 | 138 | 0 |
| `tictactoe_cpp` (C++) | ✅ | 6,2 s | 383 | 819 | 14 | 0 |

Comprobación puntual del síntoma original, sobre `tutorial-py.srctrldb`:

```
my_first_step  --EDGE_CALL(8)-->  my_next_step.my_next_step
```

es decir, `my_next_step` ya resuelve y es navegable. `unsolved symbol` no aparece en
ninguna de las cuatro bases.

> Los `ERROR` que aparecen en el log (`error_all_data_index`,
> `MessageErrorCountClear`) son nombres de índices SQLite, no fallos.

### Antes / después

| | Antes | Después |
|---|---|---|
| tutorial-py | 0 nodos, `run.py` no encontrado | 103 nodos, 394 aristas |
| javaparser | 0 nodos, comandos descartados | 4126 nodos, 25601 aristas |
| cierre del CLI con Java | colgaba (zombi, `taskkill /F` fallaba) | sale solo |

### Proyectos reales

| Proyecto | Antes | Después |
|---|---|---|
| sqlite (C) | ya funcionaba | 22.808 nodos / 103.944 aristas / 354 archivos, 98 s |
| kafka (Java, 6177 archivos) | **nunca terminaba** (1 h en 0%) | **151.977 nodos / 1.056.163 aristas / 6.177 archivos, ~20 min** |
| matplotlib (Python, 913 archivos) | no se podía crear el proyecto (bug del wizard) | **25.973 nodos / 622.480 aristas / 913 archivos, 3 h 29 min** |

**Re-run completo con todos los fixes (2026-10-17):**

```
commands started:      913
finished successfully: 913
non-zero return codes:   0
archivos en disco:     913
archivos indexados:    913
faltantes:               0
```

Comparación con la corrida anterior (antes de los fixes de §2.6/§2.7):

| | Antes | Después |
|---|---:|---:|
| Archivos | 904 / 913 | **913 / 913** |
| Nodos | 25.378 | **25.973** |
| Aristas | 590.804 | **622.480** |
| LOC | 260.786 | **281.546** |
| Fallos del indexer | 9 | **0** |
| Tiempo | 3 h 28 min | 3 h 29 min |

Los 2 nodos `unsolved symbol` que quedan son referencias realmente irresolubles del
código, no un fallo del indexador.

kafka deja **16.644 errores** en la base (el contador del CLI muestra 214.890 porque es
acumulativo de *updates*, no de errores distintos). Los más frecuentes:

- `assertThrows`/`mock`/`assertEquals` sin resolver → JUnit y Mockito no están en el
  classpath (el proyecto no trae las dependencias de Gradle).
- `import org.apache.kafka.common.message.*` sin resolver → consecuencia conocida del
  filtro por archivo: los tipos de **otros módulos** no siempre se resuelven.

**Trade-off explícito:** el filtro cambia exhaustividad por viabilidad. Sin él, kafka no
termina; con él, se pierden algunas referencias entre módulos y aparecen más errores de
resolución. La solución completa sería reutilizar el *environment* de JDT entre archivos
(un `FileSystem` por thread en vez de uno por archivo), que permitiría pasar los 134 roots
sin el costo O(n²). Queda como mejora futura.

### Reconstruir esta copia

El `CMakeCache.txt` original apunta a `<ORIGINAL_WORKSPACE>/...`; compilar sin
reconfigurar **recompila el árbol original en C:**, no esta copia. Para esta copia:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=<USERPROFILE>/DEV/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_CXX_LANGUAGE_PACKAGE=ON -DBUILD_JAVA_LANGUAGE_PACKAGE=ON `
  -DSOURCETRAIL_SHARED_CORE=ON

cmake --build build --config Release --target Sourcetrail sourcetrail_cli --parallel
```

> `SOURCETRAIL_SHARED_CORE=ON` es obligatorio: sin él, `src/lib` se compila con
> `__declspec(dllimport)` y falla con `C2491` en los miembros estáticos.

> Si hay un proceso Sourcetrail vivo, el `POST_BUILD` que copia los plugins a `plugins/`
> falla con `Permission denied` (MSB3073). Cerrar todo antes de compilar.

---

## 5. Notas de entorno

- La copia en D: y el original en C: están en el mismo commit (`20ffe0b`).
- El `build/` copiado tiene rutas absolutas a C:; se reconfiguró para D:.
- `vcpkg_installed` (33 GB) se reutilizó tal cual; la caché binaria de vcpkg (7,2 GB)
  también está presente, así que una reinstalación no obliga a recompilar LLVM.
- Los 776 archivos faltantes de la copia son intermedios de `build/src/`; se regeneran.

---

## 6. Pendientes / fuera de alcance

1. **Wizard GUI**: ✅ verificado con UI Automation y clicks reales. Seleccionar "Empty
   Python Source Group" habilita "Next"; "Next" cierra el modal y habilita "Create".
2. **Cierre de la GUI**: ✅ verificado con `java-mini.srctrlprj` cargado (crea la JVM):
   `CloseMainWindow` → exit code 0.
3. **Escala Java** — resuelta en dos pasos. El bloqueante (0 nodos y la explosión de
   source roots) está resuelto, y los problemas menores se cerraron el 2026-10-18:
   - ✅ JVM con `-Xmx2048m` (`JavaEnvironmentFactory.cpp`).
   - ✅ `JavaParser::s_nextParserId` es `std::atomic<int>`.
   - ✅ `unregisterEnvironment()` ya no desattachea el hilo por archivo.
   - ✅ `catch (Throwable)` en `JavaIndexer.java` (cubre `OutOfMemoryError`/`LinkageError`).
   - ✅ El estándar `"26"` **no era un problema**: JDT 3.44 define `VERSION_26`
     (verificado con `javap -constants` sobre el jar). No se tocó `ToolChain.cpp`.
4. **Rendimiento Python**: ~3,5 h para 913 archivos (un proceso `python` + jedi por
   archivo). No es un bug, pero es el próximo cuello de botella si importa la velocidad.
5. **Re-run completo de matplotlib**: ✅ hecho, 913/913 archivos y 0 fallos.
6. **Tarea 3** (portar a `git@github.com:5hif7y/sourcetrail.git`): pendiente por pedido
   explícito del usuario.

### Evidencia dejada en disco

- Logs de build: `_build_cli.log`, `_build_cli2.log`, `_build_gui.log` (raíz del repo).
- Bases de prueba y proyecto Java mínimo: `Sourcetrail-profundo/_audit/`.
- Prompts y salidas del chat de Antigravity: `Sourcetrail/_tools/`.

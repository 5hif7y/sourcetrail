# Arquitectura y flujo de indexado de neo-sourcetrail

**Autor:** Claude Code
**Fecha:** 2026-10-16
**Alcance:** cómo está partido el código, cómo se representa el conocimiento en la base, y el camino
completo desde "abrir un proyecto" hasta "nodos en el grafo". Verificado contra el árbol en
`Sourcetrail-profundo/neo-sourcetrail` (commit `20ffe0b`), no contra documentación previa.

---

## 1. Componentes

| Componente | Tipo | Fuente | Rol |
|---|---|---|---|
| `Sourcetrail` | ejecutable GUI (Qt6) | `src/app` | Interfaz: wizard, grafo, code view, búsqueda |
| `sourcetrail_cli` | ejecutable consola | `src/cli` | Carga un `.srctrlprj`, indexa y sale. Es el banco de pruebas |
| `sourcetrail_indexer` | ejecutable auxiliar | `src/indexer` | Indexado en **subproceso**: recibe comandos por memoria compartida |
| `Sourcetrail_lib` | DLL (`SOURCETRAIL_SHARED_CORE=ON`) | `src/lib` | Núcleo **sin Qt**: settings, project, storage SQLite, parser framework, messaging |
| `Sourcetrail_lib_gui` | librería estática | `src/lib_gui` | Todo Qt: ventanas, widgets, wizard, `QtApplication` |
| `sourcetrail_api` (`sourcetrail.dll`) | DLL C-API | `src/lib_api` | API `extern "C"` con tipos opacos y prefijo `srctrl_*` |
| `sourcetrail_cxx_plugin` | DLL plugin (~51,8 MB) | `src/lib_cxx` | Parser C/C++ sobre **Clang/LLVM 21.1.2** |
| `sourcetrail_java_plugin` | DLL plugin | `src/lib_java` | Puente JNI + `data/java/lib/java-indexer.jar` (Eclipse JDT) |
| `SourcetrailPythonIndexer` | proceso Python externo | `../SourcetrailPythonIndexer` (hermano del repo) | Indexador Python (jedi/parso + numbat) |

Regla de oro de la separación: **`src/lib` no puede incluir Qt**. Los metatipos de Qt
(`Q_DECLARE_METATYPE`) viven en `src/lib_gui/qt/utility/QtMetaTypes.h`, y el core sólo usa
`std::string`, `std::vector` y `boost::locale` (por ejemplo en `TextCodec`).

---

## 2. Modelo de datos

El índice es **una base SQLite** (`.srctrldb`). El esquema se crea en
`src/lib/data/storage/sqlite/SqliteIndexStorage.cpp`.

### 2.1 Tablas

```
element(id)                     -- raíz de todo lo que es "cosa" en el grafo
├── node(id, type, serialized_name)
│   ├── symbol(id, definition_kind)          -- nodos que son símbolos
│   ├── file(id, path, language, modification_time, indexed, complete, line_count)
│   └── local_symbol(id, name)               -- variables locales
├── edge(id, type, source_node_id, target_node_id)
└── error(id, message, fatal, indexed, translation_unit)

source_location(id, file_node_id, start_line, start_column, end_line, end_column, type)
occurrence(element_id, source_location_id)   -- dónde aparece cada nodo/arista
element_component(id, element_id, type, data)
component_access(node_id, type)              -- public/private/protected
filecontent(id, content)
meta(id, key, value)
```

`node`, `edge`, `file`, `error` y `local_symbol` son **subtipos de `element`** (FK con
`ON DELETE CASCADE`). Por eso `edge.id` y `node.id` comparten espacio de ids con `element.id`.

### 2.2 Nombres serializados

`node.serialized_name` no es un string plano: es una secuencia con separador `\t`:

```
<prefijo>\t m<elemento> \t s<separador>\t p<paréntesis> ...
```

Ejemplo real de una DB:

```
'::\tmCodeTutorial\ts\tp\tnmeaning_of_life_the_universe_and_everything\tsstatic int\tp'
```

Es decir: nombre cualificado → nombre del nodo → tipo textual → sufijo. El orden importa
para que el core reconstruya la jerarquía (`NameHierarchy`).

### 2.3 Tipos de nodo (`src/lib/data/NodeKind.h`)

Máscara de bits, no enum secuencial:

| Valor | Constante | Uso |
|---:|---|---|
| 1 | `NODE_SYMBOL` | símbolo genérico |
| 2 | `NODE_TYPE` | tipo genérico |
| 4 | `NODE_BUILTIN_TYPE` | `int`, `bool`, `void`… |
| 8 | `NODE_MODULE` | módulo / archivo importable (Python) |
| 16 | `NODE_NAMESPACE` | namespace C++ |
| 32 | `NODE_PACKAGE` | package Java |
| 64 | `NODE_STRUCT` | struct |
| 128 | `NODE_CLASS` | clase |
| 256 | `NODE_INTERFACE` | interfaz |
| 512 | `NODE_ANNOTATION` | anotación |
| 1024 | `NODE_GLOBAL_VARIABLE` | global |
| 2048 | `NODE_FIELD` | campo / atributo |
| 4096 | `NODE_FUNCTION` | función |
| 8192 | `NODE_METHOD` | método |
| 16384 | `NODE_ENUM` | enum |
| 32768 | `NODE_ENUM_CONSTANT` | valor de enum |
| 65536 | `NODE_TYPEDEF` | typedef / alias |
| 131072 | `NODE_TYPE_PARAMETER` | genérico / type var |
| 262144 | `NODE_FILE` | archivo |
| 524288 | `NODE_MACRO` | macro / preprocesador |
| 1048576 | `NODE_UNION` | union |
| 2097152 | `NODE_RECORD` | record |
| 4194304 | `NODE_CONCEPT` | concept C++20 |
| 8388608 | `NODE_C_CAST` | cast C |

### 2.4 Tipos de arista (`src/lib/data/graph/Edge.h`)

| Valor | Constante |
|---:|---|
| 1 | `EDGE_MEMBER` |
| 2 | `EDGE_TYPE_USAGE` |
| 4 | `EDGE_USAGE` |
| 8 | `EDGE_CALL` |
| 16 | `EDGE_INHERITANCE` |
| 32 | `EDGE_OVERRIDE` |
| 64 | `EDGE_TYPE_ARGUMENT` |
| 128 | `EDGE_TEMPLATE_SPECIALIZATION` |
| 256 | `EDGE_INCLUDE` |
| 512 | `EDGE_IMPORT` |
| 1024 | `EDGE_BUNDLED_EDGES` |
| 2048 | `EDGE_MACRO_USAGE` |
| 4096 | `EDGE_ANNOTATION_USAGE` |

---

## 3. Flujo de indexado

Punto de entrada: `Project::buildIndex()` en `src/lib/project/Project.cpp`.

```
Project::buildIndex()
│
├── TaskCleanStorage          (si el refresh mode pide limpiar)
├── preIndexTasks             (hooks de cada SourceGroup)
│
└── TaskParseWrapper          (crea el diálogo de progreso: 0%)
    └── TaskGroupParallel
        ├── TaskFillIndexerCommandsQueue   → llena la cola de comandos
        ├── TaskBuildIndex                 → ejecuta los indexers
        └── (en cadena) TaskMergeStorages → TaskInjectStorage
│
├── TaskFinishParsing
└── swapToTempStorage()       → renombra *_tmp.srctrldb a *.srctrldb
```

### 3.1 De archivos a comandos

Cada `SourceGroup` concreto sabe listar sus archivos y producir `IndexerCommand`:

- `SourceGroupJava::getIndexerCommands()` → un `IndexerCommandJava` **por archivo**
- `SourceGroupCustomCommand` → un comando de shell **por archivo**, sustituyendo
  `%{SOURCE_FILE_PATH}` y `%{DATABASE_FILE_PATH}` (ver nota más abajo)
- `SourceGroupCxx*` → comandos que se empaquetan en un `CompilationDatabase`

### 3.2 Ejecución

`TaskBuildIndex` decide entre dos modos (`Project.cpp`):

```cpp
bool multiProcess = ApplicationSettings::getMultiProcessIndexingEnabled() &&
                    hasCxxSourceGroup();
```

- **Multi-proceso** (sólo si hay un SourceGroup C/C++): lanza `sourcetrail_indexer.exe`
  por proceso y habla por memoria compartida (`InterprocessIndexerCommandManager`,
  `InterprocessIntermediateStorageManager`, `InterprocessIndexingStatusManager`).
- **In-process**: `runIndexerThread` corre los indexers dentro del propio proceso, un
  thread pool. **Los proyectos Java puros y Python siempre van por acá**, porque
  `hasCxxSourceGroup()` es `false`.

El progreso se publica en `TaskBuildIndex::updateIndexingDialog()`:

```cpp
int progress = 0;
if (sourceFileCount != 0) progress = indexedSourceFileCount * 100 / sourceFileCount;
MessageIndexingStatus(true, progress).dispatch();
```

`indexedSourceFileCount` sólo sube cuando se **cosecha un `IntermediateStorage`** de un
worker. Si ningún worker termina, la barra queda en 0% para siempre aunque haya CPU.

### 3.3 Persistencia

Los indexers **no escriben el `.srctrldb` final**. Escriben `IntermediateStorage` que
`TaskMergeStorages` fusiona y `TaskInjectStorage` vuelca en
`<proyecto>_tmp.srctrldb`. Al final, `Project::swapToTempStorage()`:

1. fuerza checkpoint WAL (fix histórico, ver `Project.cpp`),
2. borra el `.srctrldb` viejo,
3. renombra `_tmp.srctrldb` → `.srctrldb`.

> **Trampa histórica documentada:** la extensión temporal es `_tmp.srctrldb`, no
> `.srctrldb_tmp`. `numbat` (el wrapper Rust de SourcetrailDB) trunca el path en el último
> punto y le agrega `.srctrldb`; con `.srctrldb_tmp` escribía silenciosamente en la base
> principal y se perdía el trabajo. Ver `ProjectSettings::TEMP_INDEX_DB_FILE_EXTENSION`.

---

## 4. Plugins de lenguaje

`PluginManager` (`src/lib/plugin/PluginManager.cpp`) usa `boost::dll`:

1. escanea `<dir del ejecutable>/plugins/`,
2. abre cada `.dll`,
3. busca el símbolo `extern "C" void sourcetrail_plugin_init()`,
4. lo ejecuta.

`src/lib_cxx/PluginInit.cpp` y `src/lib_java/PluginInit.cpp` registran
`LanguagePackageCxx` + `SourceGroupFactoryModuleCxx` (y los equivalentes Java) en los
singletons globales.

### Orden de inicialización (crítico)

`walkthrough_phase8.md` documenta la regresión: si se registra
`SourceGroupFactoryModuleProxy` (el fallback que delega a `sourcetrail_cli`) **antes** de
`PluginManager::loadPlugins()`, los proyectos C++ caen en el Proxy y producen un comando
incompatible con la IPC → base vacía. La secuencia correcta en `src/app/main.cpp`,
`src/cli/main.cpp`, `src/indexer/main.cpp` y `src/lib_api/libsourcetrail.cpp` es:

```
loadPlugins()  →  registrar Proxy / Custom / Python  →  cargar proyecto
```

### Dónde viven las DLLs

El loader PE de Windows **no busca en subcarpetas**. `Sourcetrail.exe` enlaza
directamente contra los plugins, así que las DLL deben estar en el mismo directorio que el
`.exe`; además `POST_BUILD` las copia a `plugins/` para que `PluginManager` también las
descubra. Si sólo están en `plugins/`, el proceso no arranca.

---

## 5. El caso Python

Python **no es un plugin**. Se implementa como un `SourceGroup` de tipo
"custom command":

```
SourceGroupType::PYTHON_EMPTY
  └── SourceGroupSettingsPythonEmpty   (deriva de SourceGroupSettingsCustomCommand)
        └── SourceGroupFactoryModulePython
              └── SourceGroupCustomCommand
```

`SourceGroupSettingsPythonEmpty` arma este comando:

```
python "<...>/SourcetrailPythonIndexer/run.py" index \
    --source-file-path="%{SOURCE_FILE_PATH}" \
    --database-file-path="%{DATABASE_FILE_PATH}"
```

Es decir: **el core invoca a Python una vez por archivo**, y cada invocación abre la misma
base SQLite y escribe directo con `numbat`. No hay `IntermediateStorage` para Python.

Consecuencias:

- Python hereda el modo in-process (no hay SourceGroup C++).
- Si `run.py` no aparece, el comando devuelve código 2 y el resultado es **0 nodos**.
- El path de `run.py` se resuelve en `SourceGroupSettingsPythonEmpty.cpp`; hoy busca
  hacia arriba desde el directorio del ejecutable (ver `02_como_escribir_un_indexer.md`).

---

## 6. Superficie de la C-API

`src/lib_api/libsourcetrail.cpp` expone un subconjunto estable para plugins y para
consumidores externos. `sourcetrail.db`/`numbat` replican el lado escritura. El contrato
mínimo que un indexador necesita es:

```
open / create(path)
recordSymbol(hierarchy, definitionKind)          → id
recordSymbolWithLocation(...)
recordReference(sourceId, targetId, kind)
recordLocalSymbol(name)
recordOccurrence(elementId, location)
recordError(message, fatal, indexed, translationUnit)
commit()
```

---

## 7. Puntos frágiles verificados

| Punto | Por qué duele | Evidencia |
|---|---|---|
| `SOURCETRAIL_SHARED_CORE` | Si se configura OFF, `src/lib` se compila como DLL import y falla con `C2491` en `s_dllimport` | build del 16/10 en esta copia |
| Extensión temporal `_tmp.srctrldb` | numbat trunca en el último punto | `ProjectSettings.cpp`, `audit_phase4.md` |
| Separadores de path | el core normaliza a `/`; numbat recibía `\` | `sourcetraildb.py` (`ForwardSlashPath`) |
| `fetchRootDirectories()` Java | calcula un root por cada `package`; kafka produce **134 roots**. No es lento en sí (parsea sin bindings), pero infla el `sources` de JDT | `src/lib_java/utility/utilityJava.cpp` |
| JDT con muchos `source paths` | re-lista recursivamente cada root por archivo: O(archivos × roots) | `JavaIndexer.java`, ver §4 |
| `hasCxxSourceGroup()` | Java/Python nunca usan subproceso | `Project.cpp:601` |
| Loader PE vs `plugins/` | DLLs deben estar junto al `.exe` | `cxx_java_reconnection_log.md` §5 |
| `Python indexer not found` | fallback con `getParentDirectory()` fijo | ver auditoría |

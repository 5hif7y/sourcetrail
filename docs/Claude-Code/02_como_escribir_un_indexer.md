# Cómo escribir un indexador y agregarlo a neo-sourcetrail

**Autor:** Claude Code
**Fecha:** 2026-10-16
**Alcance:** el contrato que debe cumplir un indexador, las dos formas de integrarlo, y el
checklist completo para agregar un lenguaje nuevo. Basado en el código actual y en el
indexador Python que ya funciona.

---

## 1. Qué es un indexador

Un indexador convierte **un archivo fuente** en un conjunto de:

- **nodos** (símbolos: clases, funciones, variables, módulos, archivos),
- **aristas** (relaciones: llamada, herencia, uso, import, inclusión),
- **ocurrencias** (dónde aparece cada cosa: archivo, línea, columna),
- **símbolos locales** (variables dentro de una función),
- **errores** de parseo.

El resultado se escribe en una base SQLite compatible con Sourcetrail. **No hay que
inventar el esquema**: lo crea el core (`SqliteIndexStorage`) o la API de escritura.

---

## 2. Las dos integraciones posibles

### 2.1 Indexador externo por archivo (modelo Python)

El core ejecuta un comando de shell **una vez por archivo**, pasándole dos rutas:

```
<tu-indexer> index --source-file-path="<archivo>" --database-file-path="<db>"
```

Tu programa abre la DB, escribe, hace commit y sale con código 0. Es el modelo más
simple y no requiere recompilar Sourcetrail.

**Ventajas:** cualquier lenguaje de implementación, aislamiento total, fácil de debuggear
a mano.
**Desventajas:** un proceso nuevo por archivo; sin `IntermediateStorage`; el progreso
depende de que cada invocación termine.

Se integra con `SourceGroupSettingsCustomCommand` (o una subclase). Ver
`SourceGroupSettingsPythonEmpty` como ejemplo real.

### 2.2 Indexador nativo (modelo C++/Java)

Implementás `Indexer` (`src/lib/data/indexer/Indexer.h`) con `doIndex(IndexerCommand*)`,
y un `IndexerCommand` que sepa serializarse para la IPC. El resultado se devuelve como
`IntermediateStorage`, que el core fusiona e inyecta.

**Ventajas:** un solo proceso, paralelismo real, integración con el diálogo de progreso.
**Desventajas:** hay que tocar el core, CMake, y entender la IPC de memoria compartida.

`src/lib_java/data/parser/java/JavaParser.cpp` es el ejemplo canónico.

---

## 3. Escribir en la base

### 3.1 Opción A — API de escritura (recomendada)

Usá `SourcetrailDB` desde C++ (`src/lib/data/storage/SourcetrailDBWriter`) o `numbat`
desde Python/Rust. La API se encarga del schema, los ids y las ocurrencias.

Contrato mínimo (nombres de la C-API `srctrl_*`, equivalentes en numbat):

| Operación | Para qué |
|---|---|
| `open(path)` / `create(path)` | abrir/crear la DB |
| `recordSymbol(nameHierarchy, definitionKind)` | declarar un símbolo |
| `recordSymbolWithLocation(...)` | símbolo con su ubicación |
| `recordReference(sourceId, targetId, kind)` | crear una arista |
| `recordLocalSymbol(name)` | variable local |
| `recordOccurrence(elementId, location)` | ubicar un nodo/arista |
| `recordError(message, fatal, indexed, unit)` | reportar un problema |
| `commit()` | cerrar transacción |

En Python, `SourcetrailPythonIndexer/sourcetraildb.py` envuelve `numbat` y expone
constantes con nombre:

```python
SYMBOL_CLASS, SYMBOL_FUNCTION, SYMBOL_METHOD, SYMBOL_FIELD, SYMBOL_MODULE, ...
REFERENCE_CALL, REFERENCE_INHERITANCE, REFERENCE_IMPORT, REFERENCE_USAGE, ...
DEFINITION_EXPLICIT, DEFINITION_IMPLICIT
```

### 3.2 Opción B — SQL directo

El esquema está en `SqliteIndexStorage.cpp`. Es válido escribir a mano, pero hay que
respetar:

- `element` como base (todo nodo/arista inserta primero su fila en `element`),
- el formato de `serialized_name` (`\t` y prefijos `m`/`s`/`p`),
- `symbol(id, definition_kind)` para cada nodo símbolo,
- `file(id, path, ...)` con `path` en **barras normales** (`C:/...`).

Si te equivocás en el formato de `serialized_name`, la base abre igual pero el árbol de
nombres sale mal.

---

## 4. Checklist para agregar un lenguaje

Supongamos `MYLANG`. Hay que tocar, en este orden:

### 4.1 Tipo de SourceGroup

1. `src/lib/settings/source_group/SourceGroupType.h`
   → agregar `MYLANG_EMPTY` al enum.
2. `SourceGroupType.cpp`
   → mapear a los strings legibles (`"Empty MyLang Source Group"`).
3. `src/lib/language/LanguageType.h*`
   → agregar `MYLANG` si el lenguaje no existe.

### 4.2 Settings

4. Crear `src/lib/settings/source_group/type/SourceGroupSettingsMyLangEmpty.h/.cpp`,
   derivando de `SourceGroupSettingsCustomCommand`.
5. Registrar la carga en `ProjectSettings.cpp` (`loadSettings` / `saveSettings`).

### 4.3 Fábrica

6. Crear `src/lib/project/SourceGroupFactoryModuleMyLang.h/.cpp`:
   `supports(type)` + `createSourceGroup(settings)`.
7. Registrarlo **después** de `PluginManager::loadPlugins()` en:
   - `src/app/main.cpp`
   - `src/cli/main.cpp`
   - `src/indexer/main.cpp`
   - `src/lib_api/libsourcetrail.cpp`
   
   (si se registra antes, los plugins legítimos nunca se cargan — ver §5)

### 4.4 Wizard (GUI)

8. `QtProjectWizardContentSelect.cpp`
   → `sourceGroupInfos[LanguageType::MYLANG].push_back(...)`, ícono y descripción.
9. **`QtProjectWizard::selectedProjectType()`** en `QtProjectWizard.cpp`
   → agregar el `case SourceGroupType::MYLANG_EMPTY:` que crea el settings.
   
   > **Este `case` es obligatorio y se olvida fácil.** Si falta, el diálogo de tipo de
   > Source Group no cierra y el botón "Next"/"Create" queda gris para siempre. Le pasó
   > a Python y es exactamente el bug que arreglamos.

10. Si el lenguaje necesita páginas propias, crear
    `QtProjectWizardContentMyLang*` y agregarlas al `switch` de contenido.

### 4.5 Build

11. `CMakeLists.txt`
    → `option(BUILD_MYLANG_LANGUAGE_PACKAGE ...)` y `add_subdirectory(src/lib_mylang)`.
12. Si es plugin: `src/lib_mylang/PluginInit.cpp` con
    `extern "C" __declspec(dllexport) void sourcetrail_plugin_init()`.
13. `POST_BUILD` para copiar la DLL junto al `.exe` **y** a `plugins/`.

### 4.6 Proyecto de ejemplo

14. Crear `bin/app/user/projects/mylang-sample/mylang-sample.srctrlprj` con
    `<type>Empty MyLang Source Group</type>` y `source_paths`.
15. Agregarlo a `bin/app/data/fallback/ApplicationSettings.xml` si querés que aparezca en
    "Recent Projects".

---

## 5. Orden de inicialización

En los cuatro `main`:

```cpp
PluginManager::loadPlugins();                       // 1º
SourceGroupFactory::getInstance()->addModule(...);   // 2º (Proxy, Custom, Python…)
```

Invertir esto hace que el Proxy capture los proyectos nativos y los mande por un camino
que no entiende la IPC → 0 nodos. Ver `walkthrough_phase8.md`.

---

## 6. Trampas conocidas (todas costaron tiempo real)

| Trampa | Síntoma | Solución |
|---|---|---|
| Extensión temporal `.srctrldb_tmp` | 0 nodos tras indexar | usar `_tmp.srctrldb` (`ProjectSettings`) |
| WAL sin checkpoint antes del rename | 0 nodos | `PRAGMA wal_checkpoint(TRUNCATE)` en `swapToTempStorageFile` |
| Paths con `\` en la DB | nodos sin ocurrencias en Windows | normalizar a `/` |
| `WINDOWS_EXPORT_ALL_SYMBOLS` en un plugin que linkea Clang | `LNK1106` (archivo `.exp` de 1,7 GB) | exportar sólo la entrada del plugin |
| `RelWithDebInfo` sin `IMPORTED_LOCATION` | error de CMake al generar | `CMAKE_MAP_IMPORTED_CONFIG_*` o limitar `CMAKE_CONFIGURATION_TYPES` |
| Debug `MDd` en build Release | `LNK2038 RuntimeLibrary mismatch` | forzar `Clang_DIR` a la carpeta release |
| Plugin sólo en `plugins/` | "no se encontró sourcetrail_cxx_plugin.dll" | copiar también junto al `.exe` |
| `SOURCETRAIL_SHARED_CORE=OFF` | `C2491: definition of dllimport static data member` | configurar ON |
| Falta `case` en `selectedProjectType()` | "Next" gris | agregar el case |
| JDT recibe TODOS los source roots en cada archivo | el indexado se degrada a O(archivos × roots): kafka (6177 archivos, 134 roots) no termina; el CPU se va en `ClasspathDirectory.computeDirectoryList` | pasar sólo el root que contiene el archivo (`JavaIndexer.filterSourcesForFile`) |
| JVM sin `-Xmx` | GC thrashing / `OutOfMemoryError` no capturado | fijar `-Xmx` y `catch (Throwable)` |

---

## 7. Anatomía del indexador Python (referencia)

```
SourcetrailPythonIndexer/
├── run.py              CLI: subcomandos `index` y `check-environment`
├── indexer.py          recorrido del AST con jedi/parso
├── shallow_indexer.py  modo rápido por nombre, sin resolución
├── sourcetraildb.py    wrapper de numbat + constantes de tipos
└── numbat/             (repo hermano) binding Rust a SourcetrailDB
```

Flujo por archivo:

```
run.py index --source-file-path X --database-file-path Y
  └── indexer.indexFile(X)
        ├── jedi.Script(...)          resolución de nombres
        ├── recorre AST               emite recordSymbol / recordLocalSymbol
        ├── script.goto(...)          resuelve cada referencia
        ├── recordReference(...)      arista si resolvió
        └── recordReferenceToUnsolvedSymhol(...)   si no resolvió
```

Cuando una referencia no se resuelve, **no se pierde**: se crea un nodo literal
`"unsolved symbol"` y la GUI lo intercepta en
`ActivationController::handleMessage()` para mostrar un tooltip en vez de navegar. Si ves
muchos de esos, el problema suele ser del *código indexado* (imports faltantes, sintaxis
inválida), no del indexador.

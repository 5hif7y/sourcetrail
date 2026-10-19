# Arquitectura de plugins al estilo Cordis para Sourcetrail

**Fecha:** 2026-10-18
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Fuentes estudiadas:**
- Paper: *A Programming Paradigm for Spatiotemporal Composability* (Shi, Zhang, Cui;
  Peking University / DeepSeek-AI, 2026), `2608.25512v1.pdf`.
- DeepSeek Harness (`dsh`), commit `687f282e`:
  <https://github.com/deepseek-ai/deepseek-harness>.
- Cordis 4.0.5, vendorizado en `dsh` (`vendor/cordis`), y su *primer*.
- Plan histórico del repo: `rehacer/01…05` (fases 1–4 de la reestructuración).

---

## 0. Aclaración necesaria

Claude Code, el arnés con el que estoy trabajando, **no está construido sobre Cordis
ni sobre DeepSeek Harness**. `dsh` es otro arnés de agentes; el paper describe su
modelo. Lo que sí es cierto es que el paper es explícitamente agnóstico al lenguaje y
a la implementación (§6.4), y sus dos dimensiones —efectos revertibles y coeffects
reactivos— se pueden aplicar a Sourcetrail.

Este documento estudia **adoptar ese modelo en Sourcetrail**, no reescribir el arnés de
Claude Code. El objetivo práctico es que los lenguajes (C++, Java, Python, Odin y los
que vengan) sean plugins independientes, cargables y descargables sin tocar el núcleo.

---

## 1. Lo esencial del paper, en una página

El paper identifica dos dimensiones ortogonales de la composición dinámica:

| Dimensión | Pregunta | Mecanismo formal | Implementación en Cordis |
|---|---|---|---|
| **Temporal** | ¿Se pueden deshacer *todos* los efectos de un componente al descargarlo? | Efecto revertible: `Γ → Γ × (Γ → Γ)`; el runtime guarda la inversa y las compone en LIFO | `ctx.effect(callback)` devuelve un `dispose` |
| **Espacial** | ¿Puede un componente declarar y resolver sus dependencias en runtime? | Coeffect reactivo: una especificación `d ⊆ K` se clasifica como activante/desactivante/neutral ante cada cambio de contexto | `inject`, `ctx.get/set`, `notify`, `refresh` |

La pieza que las une es el **contexto** `Γ`: todo efecto y todo coeffect pasa por él.
Un componente se instancia como **fiber** con un estado (`LOADING`, `ACTIVE`,
`UNLOADING`, `INACTIVE`, `FAILED`), una vista comprometida de sus dependencias y un
acumulador de inversas. Al descargarlo, las inversas se ejecutan en orden inverso; los
dependientes se desactivan antes de que el proveedor desaparezca.

La implementación de referencia (paper §5.1) tiene cuatro primitivas:

```text
ctx.effect(cb)        // registra un efecto y devuelve su inversa
ctx.get(key)          // lee un servicio
ctx.set(key, value)   // publica un servicio (es un efecto)
ctx.use(component)    // instancia un componente como fiber
```

y un *loader* declarativo (§5.2) que reconcilia una configuración persistente con el
árbol de fibers y permite hot module replacement (HMR).

**Lo que hay que copiar:** el contexto como único mediador, las dependencias
declaradas (`inject`), las registraciones como efectos con `dispose`, el ciclo de vida
del fiber y el orden de descarga (dependientes primero).
**Lo que no hace falta copiar:** el `Proxy` de JavaScript, el modelo async/await, HMR
en producción y la composición de *cada* clase de la GUI.

---

## 2. Estado actual de Sourcetrail

Sourcetrail ya tiene una modularización *gruesa*; el plan `rehacer/` la describió y
buena parte se implementó:

| Capa | Estado |
|---|---|
| `src/lib` | núcleo C++/Boost sin Qt (fase 2 completada) |
| `src/lib_gui` + `src/app` | GUI Qt enlazada contra el núcleo |
| `src/lib_api` | C API (`sourcetrail.dll`) como barrera |
| `src/lib_cxx`, `src/lib_java` | DLLs `sourcetrail_cxx_plugin.dll` / `sourcetrail_java_plugin.dll` |
| `src/cli` | CLI headless sin Qt (perfil embedded) |
| Python | indexador externo por archivo, vía `SourceGroupSettingsCustomCommand` |

Pero el mecanismo de plugins es todavía **init-only**:

- `src/lib/utility/PluginManager.cpp:14` recorre `plugins/`, carga DLLs con
  `boost::dll::shared_library`, busca `sourcetrail_plugin_init()` y la llama.
  Guarda las librerías en `m_loadedPlugins` y **nunca las descarga**; no hay manifiesto,
  versión, dependencias ni `dispose`.
- `src/lib/app/LanguagePackageManager.cpp:20` y
  `src/lib/project/SourceGroupFactory.cpp:16` son singletons con `addPackage`/`addModule`
  append-only. No existe `remove`.
- `src/lib_cxx/PluginInit.cpp:13` y `src/lib_java/PluginInit.cpp` llaman directamente a
  esos singletons desde el DLL. Funciona porque el core se compila como shared
  (`SOURCETRAIL_SHARED_CORE`) cuando hay lenguajes, pero es exactamente el problema de
  ABI/globales duplicadas que el plan `rehacer/04` quería evitar.
- `src/app/main.cpp:142` registra C++/Java estáticamente si `!SOURCETRAIL_SHARED`, y
  después carga los plugins; el orden importa (ver `docs/Claude-Code/02…`).
- Los lenguajes están fijados en enums: `LanguageType` (`CXX`, `C`, `JAVA`, `PYTHON`,
  `CUSTOM`) y `SourceGroupType`. Agregar Odin obliga a tocar el enum, el string mapping,
  settings, factory, wizard y cuatro `main` (checklist de 15 pasos en el doc 02).

### 2.1 Qué falta respecto del paper

| Concepto Cordis | Sourcetrail hoy | Consecuencia |
|---|---|---|
| Efecto revertible | `sourcetrail_plugin_init()` sin inversa | no se puede descargar un plugin; reinicio |
| Coeffect / `inject` | orden de registro manual | dependencias implícitas y frágiles |
| Fiber / ciclo de vida | no existe | no hay activar/desactivar por dependencia |
| Loader declarativo | carpeta `plugins/` | no hay configuración, versiones ni reconciliación |
| HMR | no existe | cada cambio exige reiniciar |
| IDs estables | enums compilados | un lenguaje nuevo requiere recompilar el core |

---

## 3. Diseño propuesto: “Cordis en las costuras”

La propuesta no es convertir cada widget en un plugin. Es aplicar el modelo a las
**costuras de servicio** del producto:

1. **Lenguajes e indexadores** (C++, Java, Python, Odin).
2. **Registraciones de la GUI**: acciones de menú, paneles, páginas de preferencias,
   páginas del wizard.
3. **Comandos del CLI**.
4. **Backends de storage/messaging** y detectores de toolchains.
5. **Perfiles de composición**: `embedded`, `desktop`, `cli-cxx`, `cli-java`, etc.

El núcleo (`Project`, `Storage`, `Indexer` base, `Message` bus) sigue siendo código
normal; sólo sus *puntos de extensión* se registran como efectos.

### 3.1 Núcleo C++ mínimo

Un header-only en `src/lib/plugin/` (sin Qt, sin LLVM) con estas piezas:

```cpp
namespace st::plugin {

using Dispose = std::function<void()>;

class Effect
{
public:
    Effect() = default;
    explicit Effect(Dispose dispose);
    Effect(Effect&&) noexcept;
    Effect& operator=(Effect&&) noexcept;
    ~Effect();                       // ejecuta la inversa una sola vez
    void reset();
private:
    Dispose m_dispose;
};

template <class T>
struct ServiceKey
{
    static constexpr std::string_view name = typeid(T).name(); // o un ID explícito
};

class Context
{
public:
    template <class T> T& require();
    template <class T> Effect provide(std::shared_ptr<T> service);
    template <class F> Effect effect(F&& body);   // body devuelve Dispose
    Fiber& use(const Component& component, const Config& config);
    void pump();                                   // transiciones pendientes
};

struct Component
{
    std::string id;
    std::vector<std::string> inject;              // claves de servicio
    std::function<void(Context&)> apply;          // registra efectos
};

} // namespace st::plugin
```

Propiedades:

- `Effect` es RAII y single-shot, como el `dispose` de Cordis.
- `Context::effect` ejecuta el cuerpo, guarda la inversa y la compone en LIFO con las
  anteriores del fiber.
- `require<T>()` falla con un error claro si el servicio no está; `inject` evita que
  el componente se active si falta una dependencia.
- No hay `std::any` en rutas calientes: el registro se resuelve una vez al activar el
  componente, no por símbolo indexado.
- El ciclo de vida es **síncrono**: Sourcetrail ya tiene un hilo principal y un
  `Message` pump. No hace falta async/await; `pump()` procesa transiciones pendientes.

### 3.2 Ciclo de vida del fiber (versión síncrona)

```text
refresh(fiber):
  target = resolve(fiber.inject)          # conjunto de proveedores activos
  if target == fiber.committed: return    # neutral
  if fiber.state in {LOADING, UNLOADING}: fiber.pending = target; return
  if target not empty:
      fiber.state = LOADING
      apply(fiber)                        # registra efectos
      fiber.committed = target
      fiber.state = ACTIVE
  else:
      fiber.state = UNLOADING             # deja de proveer primero
      notify_dependents(fiber)            # los dependientes se descargan antes
      fiber.dispose_all()                 # inversas en LIFO
      fiber.committed = {}
      fiber.state = INACTIVE
```

La diferencia con Cordis es que no hay `await`: la descarga es inmediata. Si en el
futuro un indexador necesita cancelación asíncrona, se puede añadir una cola de tareas
sin cambiar el contrato de `Effect`.

### 3.3 Lenguajes como descriptores, no enums

Ésta es la pieza que hace posible Odin sin tocar 15 archivos:

```cpp
struct LanguageDescriptor
{
    std::string id;                    // "cxx", "java", "python", "odin"
    std::string displayName;
    std::vector<std::string> extensions;          // {".odin"}
    std::vector<std::string> sourceGroupTypeIds;  // {"odin_empty"}
    std::function<std::shared_ptr<SourceGroupSettings>(const std::string& id,
                                                       const ProjectSettings*)> makeSettings;
    std::function<std::shared_ptr<IndexerBase>()> makeIndexer;
    std::function<bool(const FilePath&)> detect;
    // Opcional, sólo GUI:
    std::function<QWidget*(QWidget*)> makeWizardPage;
    std::string iconName;
};

class LanguageRegistry
{
public:
    Effect add(LanguageDescriptor descriptor);
    const LanguageDescriptor* find(std::string_view id) const;
    const LanguageDescriptor* findByExtension(std::string_view ext) const;
};
```

Los enums actuales se conservan como **alias de compatibilidad** para los built-ins
(`CXX`, `JAVA`, `PYTHON`), pero la fuente de verdad pasa a ser el ID string. El proyecto
`.srctrlprj` sigue guardando el nombre legible; se agrega una tabla de migración
`enum ↔ string` para no romper proyectos existentes. Un plugin nuevo sólo registra su
descriptor; no necesita un `case` nuevo en `QtProjectWizard::selectedProjectType()`.

### 3.4 Efectos sobre las registraciones existentes

Cada registro actual se envuelve en un `Effect`:

| Registración actual | Efecto / inversa |
|---|---|
| `SourceGroupFactory::addModule` | `removeModule(id)` |
| `LanguagePackageManager::addPackage` | `removePackage(id)` |
| `Message` handler / listener | `unsubscribe(handle)` |
| Acción de menú/toolbar Qt | `removeAction(action)` |
| Página de preferencias/wizard | `removePage(id)` |
| Indexer factory | `unregisterIndexer(id)` |
| Detector de toolchain | `unregisterDetector(id)` |

Con esto, descargar un plugin de lenguaje deja el sistema exactamente como estaba:
sin módulos huérfanos, sin handlers zombis y sin páginas de UI fantasma.

### 3.5 Frontera C y carga dinámica

El paper §6.4 señala que en código nativo la introducción/retirada se hace con
`dlopen`/`dlclose` o `LoadLibrary`/`FreeLibrary`. Para no cruzar STL por la frontera:

```c
/* sourcetrail_plugin_api.h — versión 1 */
typedef struct SourcetrailPluginApiV1
{
    uint32_t size;                       /* sizeof, para compatibilidad futura */
    void*    (*require_service)(const char* key);
    void     (*register_language)(const SourcetrailLanguageDesc* desc,
                                  void (*destroy)(void*), void* user);
    void     (*log)(int level, const char* message);
} SourcetrailPluginApiV1;

typedef struct SourcetrailPluginDesc
{
    uint32_t api_version;                /* 1 */
    const char* id;                      /* "odin" */
    const char* version;                 /* "0.1.0" */
    const char* const* inject;           /* NULL-terminated */
    void (*apply)(SourcetrailPluginApiV1* api, void* user);
    void (*dispose)(void* user);
    void* user;
} SourcetrailPluginDesc;

extern "C" SOURCETRAIL_EXPORT const SourcetrailPluginDesc*
sourcetrail_plugin_v1(void);
```

El host:

1. comprueba `api_version` y `size`;
2. resuelve `id`, `version` e `inject` contra el manifiesto;
3. llama `apply`, que registra efectos a través de la API;
4. al descargar, llama `dispose`, ejecuta las inversas y sólo entonces libera la DLL.

**Restricción práctica:** C++/Java no se pueden descargar con seguridad mientras haya
ASTs, JVM o hilos de Clang vivos. Se declaran `unloadable: false` y se descargan sólo al
cerrar el proyecto o la aplicación. Odin, que no arrastra LLVM, sí puede ser
`unloadable: true` desde el primer día.

### 3.6 Perfiles y loader

Un manifiesto JSON (o XML, para no agregar dependencias) describe la composición:

```json
{
  "profile": "embedded",
  "plugins": [
    {"id": "core",       "version": "1.0"},
    {"id": "python",     "version": "1.0"},
    {"id": "odin",       "version": "0.1", "enabled": false}
  ]
}
```

- **Embedded:** plugins enlazados estáticamente; el loader sólo aplica el orden y los
  `inject`. Cero `dlopen`, cero Qt.
- **Desktop:** plugins dinámicos en `plugins/`; el loader reconcilia el manifiesto con
  lo que encuentra.
- **Dev:** watch de la DLL + `dispose` + recarga (HMR) sólo si el plugin es
  `unloadable`. No es requisito para producción.

---

## 4. Plan de modularización por fases

Estimaciones en jornadas de una persona con el contexto actual; no son compromisos.

| Fase | Alcance | Archivos principales | Gate | Est. |
|---|---|---|---|---|
| **F0** | Inventario de servicios/efectos y tests de contrato | `src/lib`, `src/lib_gui`, docs | lista de costuras + baseline de counts/size | 1–2 |
| **F1** | Núcleo `st::plugin` header-only + tests | `src/lib/plugin/*`, `src/lib/CMakeLists.txt` | tests unitarios; build embedded sin Qt | 2–3 |
| **F2** | Registro de lenguajes por ID string; alias de enums; migración de proyectos | `LanguageType`, `SourceGroupType`, settings, factory, wizard | proyectos viejos abren igual; samples indexan igual | 4–6 |
| **F3** | Host de plugins: manifiesto, C API v1, `Effect`, unload seguro | `PluginManager`, `lib_api`, `lib_cxx`, `lib_java` | load/unload de un plugin de prueba; CXX/Java no unloadable | 4–6 |
| **F4** | Plugin Odin (ver doc 22) | `src/lib_odin` o indexer externo | parse coverage y counts del sample | 5–8 |
| **F5** | Costuras GUI/CLI: acciones, páginas, comandos, perfiles | `lib_gui`, `cli` | desktop smoke; CLI embedded sin Qt | 5–10 |
| **F6** | Loader declarativo + HMR dev (opcional) | host, manifiestos | recarga de Odin sin reiniciar | 3–5 |

**Slice mínimo útil:** F0–F4 (≈3–4 semanas). Deja Odin funcionando como plugin y el
modelo listo para el próximo lenguaje. F5–F6 se pueden hacer después sin bloquear.

### 4.1 Gates concretos

- `sourcetrail_cli` embedded: sin Qt, sin LLVM, ≤2 MB, indexa los samples.
- Desktop: `Sourcetrail.exe` abre; plugins C++/Java cargan; counts idénticos al baseline.
- Un plugin de prueba (p. ej. `odin`) se carga, registra un lenguaje, se descarga y el
  registro queda vacío; no quedan handlers ni páginas.
- Test de doble carga: cargar el mismo plugin dos veces no duplica indexers.
- Test de dependencia faltante: un plugin con `inject: ["storage"]` no se activa si
  storage no está; no hay crash.
- ASan/valgrind en el ciclo load/unload (Linux) y Application Verifier (Windows).
- Proyectos `.srctrlprj` viejos: la migración enum→string no pierde el tipo de source
  group.

### 4.2 Riesgos

| Riesgo | Mitigación |
|---|---|
| Migrar enums a strings rompe proyectos | tabla de migración + fallback por nombre legible; tests con los 4 samples |
| ABI C++ entre DLL y core | C API v1; nunca pasar `std::string`/`std::shared_ptr` por la frontera |
| Descargar C++/Java con LLVM/JVM vivo | `unloadable: false`; descarga sólo al cerrar proyecto |
| Sobrecarga en indexado | resolver servicios al activar, no por nodo; perfil embedded estático |
| Reescritura de GUI fuera de control | limitar F5 a acciones/páginas; no tocar `QtMainWindow` internals |
| Dependencias circulares | el modelo las deja inactivas; detectarlas al cargar y reportar |

---

## 5. Comparación con `dsh`

| Aspecto de `dsh` | Traslado a Sourcetrail | ¿Vale la pena? |
|---|---|---|
| Todo es un plugin (modelo, tools, sesión, loop) | lenguajes, indexers, comandos, páginas | **sí**, en las costuras |
| `inject` + servicios tipados | `LanguageDescriptor.inject` | **sí** |
| `ctx.effect` + dispose LIFO | `Effect`/RAII | **sí** |
| Perfiles y bundles | `embedded`, `desktop`, `cli-*` | **sí** |
| Loader con reconciliación y HMR | recarga de plugins dev | **opcional** |
| `Proxy` y eventos tipados de TS | `Message` bus + efectos | adaptar, no copiar |
| Async/await y fibers concurrentes | pump síncrono | **no** en la primera fase |
| Pluginizar cada widget | no | **no**; costo alto, beneficio bajo |

---

## 6. Recomendación

Adoptar **“Cordis en las costuras”**, no un “todo es un plugin” literal:

1. Implementar F0–F3 con un plugin de prueba y los CXX/Java migrados.
2. Implementar Odin (F4) como el primer lenguaje nuevo creado contra el modelo.
3. Dejar F5–F6 para después; el perfil embedded sigue estático y sin Qt.
4. No reescribir la GUI ni el pipeline de indexado; envolver sus puntos de extensión.

Con eso se obtiene lo que el usuario pidió: lenguajes independientes, un núcleo
estable y la posibilidad de agregar Odin sin modificar el centro del programa. El costo
realista es de **3–4 semanas** para el slice mínimo, no un fin de semana.

**Decisión pendiente:** ¿se aprueba F0–F3 y luego F4, o se prefiere empezar directo por
un PoC de Odin con el modelo actual (sin refactor de plugins) para validar el lenguaje
antes de invertir en la infraestructura? La segunda opción entrega valor antes, pero
deja el problema de fondo para después.

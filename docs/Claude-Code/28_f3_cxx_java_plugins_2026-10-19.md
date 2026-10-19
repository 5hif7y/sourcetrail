# F3: CXX y Java como plugins del `PluginHost`

**Fecha:** 2026-10-19
**Árbol canónico:** `_work\modernized`
**Base:** `fc12fc0` (docs 25–27)
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`

---

## 1. Alcance

Cerrar el resto de F3 que la nota 27 dejaba pendiente:

- migrar los language packages CXX/Java y sus `SourceGroupFactoryModule` al
  `PluginHost`;
- marcarlos `unloadable=false` (LLVM/JVM no se pueden descargar con seguridad);
- eliminar los `addModule`/`addPackage` directos de los puntos de entrada;
- probar carga/descarga y no romper el perfil embedded.

Las páginas/acciones de la GUI y los efectos de mensajes siguen siendo F5
(doc 21): F3 es la costura de lenguajes, no una reescritura de la GUI.

---

## 2. Cambios

### 2.1 Registro reversible de un lenguaje

`src/lib/plugin/PluginLanguageRegistration.h` (nuevo) encapsula el patrón
común a CXX y Java:

1. crea el `SourceGroupFactoryModule` y el `LanguagePackage`;
2. los registra en `SourceGroupFactory` y `LanguagePackageManager`;
3. registra un efecto con `host->add_effect`;
4. al descargar, elimina el paquete y el módulo **en orden inverso** y libera
   el estado.

Si `add_effect` falla, deshace el registro inmediatamente. El estado vive en
el plugin y se libera antes de que el host cierre la biblioteca.

### 2.2 `LanguagePackageManager`

Se agregó `removePackage(const std::shared_ptr<LanguagePackage>&)` como
inversa de `addPackage`, análoga a `SourceGroupFactory::removeModule`.

### 2.3 Descriptores C ABI de CXX y Java

`src/lib_cxx/PluginInit.cpp` y `src/lib_java/PluginInit.cpp` exportan ahora:

- `sourcetrail_plugin_descriptor()` con ABI v1, ids estables `cxx` y `java`,
  y `unloadable = 0`;
- `sourcetrail_plugin_init()` se conserva para hosts antiguos.

`apply` usa `LanguagePluginRegistration`; no hay estado estático nuevo.

### 2.4 `PluginManager`

`src/lib/utility/PluginManager.cpp` ahora:

- intenta primero `PluginHost::loadDynamic` (descriptor C ABI v1);
- trata `plugin already loaded` como éxito idempotente;
- si la biblioteca no tiene descriptor, cae al `sourcetrail_plugin_init`
  legado y recuerda la ruta para no registrarla dos veces;
- distingue errores reales (ABI, dependencias, `apply`) de la ausencia de
  descriptor, y no hace fallback en esos casos;
- envía los mensajes del host al logger.

`PluginHost.h` incorpora `st::plugin::globalHost()`, un host de proceso
compartido por el cargador y por Odin. Se filtra a propósito para que los
efectos y las bibliotecas sobrevivan a la destrucción estática de los
registros.

### 2.5 Puntos de entrada

- `src/app/main.cpp`: se eliminaron los `#include` de CXX/Java y el bloque
  estático que registraba ambos bajo `!SOURCETRAIL_SHARED`. Todo pasa por
  `PluginManager`.
- `src/lib/project/OdinPlugin.cpp`: usa `globalHost()`.
- `src/lib_api/libsourcetrail.cpp`: usa
  `PluginManager::loadPluginsBesideModule()` en lugar de
  `program_location()/plugins`. Así la API busca los plugins junto al módulo
  del core y no junto al ejecutable que la hospeda.

### 2.6 Tests

`plugin_abi_test` carga ahora un segundo plugin compartido “sticky”
(`TEST_PLUGIN_STICKY`) con `unloadable=0` y verifica que el host rechaza su
descarga pero lo mantiene cargado. Cubre la propagación del flag del
descriptor, que es exactamente lo que usan CXX/Java.

---

## 3. Verificación

| Prueba | Resultado |
|---|---|
| Build desktop `Sourcetrail`, `sourcetrail_cli`, `sourcetrail_api` | OK |
| Carga CXX/Java por C ABI en CLI | `cxx` y `java` cargados (log) |
| `tictactoe_cpp` | **383 nodos / 819 aristas / 0 errores** |
| `tutorial-py` | **103 / 281 / 0** |
| `javaparser` (Java) | **4.126 / 25.601 / 138 archivos** |
| Sample Odin embedded | **32 / 42 / 0** |
| `plugin_context`, `plugin_host`, `plugin_abi` | **3/3** |
| `language_registry` | **1/1** |
| Indexador Odin | **6/6** |
| API (`srctrl_init`) con CXX | **383 / 819** |

Notas de medición:

- El log del CLI muestra `Loaded plugin through the C ABI` para
  `sourcetrail_cxx_plugin.dll` y `sourcetrail_java_plugin.dll`; ya no se usa
  `sourcetrail_plugin_init`.
- `javaparser` produce 187 diagnósticos del parser Java en este árbol de
  compilación. El grafo coincide exactamente con el histórico
  (4.126/25.601/138), así que no se atribuye a la migración; el árbol
  `neo-sourcetrail` contiene cambios WIP ajenos a esta sesión. No se registró
  el contador de errores previo en esa corrida.
- La API se probó copiando las dependencias y `plugins/` junto a
  `build/src/lib_api/Release/sourcetrail.dll` (sólo en el árbol de build).
  Para distribuir la API hay que mantener esa misma disposición: el núcleo
  busca `<directorio del core>/plugins`.
- `unload` real de CXX/Java se rechaza por diseño; el host lo verifica con el
  plugin sticky. Odin sigue siendo `unloadable=true` in-process.

---

## 4. Pendiente

- **F5**: acciones/páginas de la GUI y filtros de mensajes como efectos
  reversibles. No se tocó `QtProjectWizard` ni `MessageQueue`.
- **F3 opcional**: un plugin de lenguaje descargable de prueba que registre un
  módulo y un paquete reales; hoy el camino de dispose sólo se ejerce con
  plugins de prueba genéricos y con Odin.
- **E5**: cross-build ARM64 en Linux/QEMU/placa.
- **Historial**: purge opcional de `error.log`/`build_log.txt` (aprobación y
  force-push).
- **GUI**: recorrer el wizard Odin con IDs nuevos.
- **Parser Odin**: 178 no resueltas.
- **Writer**: un proceso por source group con lista de archivos.

---

## 5. Reproducir

```powershell
cmake --build build --config Release --target Sourcetrail sourcetrail_cli sourcetrail_api
build\Release\app\sourcetrail_cli.exe `
  bin\app\user\projects\tictactoe_cpp\tictactoe_cpp.srctrlprj --refresh=all

cmake --build build-embedded --config Release --target sourcetrail_cli
build-embedded\Release\app\sourcetrail_cli.exe `
  bin\app\user\projects\odin-sample\odin-sample.srctrlprj --refresh=all

cmake -S src/lib/plugin/tests -B _study/plugin-context-test `
  -G "Visual Studio 18 2026" -A x64
cmake --build _study/plugin-context-test --config Release
ctest --test-dir _study/plugin-context-test -C Release --output-on-failure

cmake -S src/lib/settings/tests -B _study/language-registry-test `
  -G "Visual Studio 18 2026" -A x64
cmake --build _study/language-registry-test --config Release
ctest --test-dir _study/language-registry-test -C Release --output-on-failure
```

---

## 6. Seguridad

- Sin rutas personales en archivos rastreados; usar `<WORKSPACE>`/`<STUDY>`.
- Sin binarios rastreados.
- Los logs del plugin incluyen rutas absolutas sólo en tiempo de ejecución,
  no en el repositorio.
- Commits con `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

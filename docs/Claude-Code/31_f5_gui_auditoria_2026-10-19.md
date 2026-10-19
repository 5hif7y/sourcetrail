# F5 GUI y auditoría de cierre

**Fecha:** 2026-10-19
**Árbol canónico:** `_work\modernized`
**Base:** `57ae7d7` (docs 30)
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`

---

## 1. F5 GUI

### 1.1 Registros reversibles

Cuatro registros header-only, sin dependencia de Qt en su lógica:

| Registro | Contenido | Inversa |
|---|---|---|
| `QtMenuRegistry` | contribuyentes por id de menú (`project`, `edit`, `view`, `history`, `bookmarks`, `help`) | `removeContributor` |
| `QtPreferencesPageRegistry` | fábricas de páginas de preferencias | `removePage` |
| `SourceGroupUiRegistry` | tipo, lenguaje, recomendado, icono y descripción de cada source group | `removeEntry` |
| `QtProjectWizardPageRegistry` | contenido por source group en el wizard | `removeContributor` |

`UiRegistryTest` cubre alta, duplicados, populate, baja y baja repetida de
los cuatro.

### 1.2 Plugin `ui`

`src/lib_gui/qt/window/UiPlugin.*` registra dentro de un `ctx.effect`:

- los seis menús de la ventana principal;
- las páginas de preferencias (core, header search global y, en macOS,
  framework search global);
- las entradas del wizard para C, C++, Java, Python, Odin y custom command;
- la cadena de contenido por source group del wizard.

El manifiesto es `unloadable=false`; el dispose existe y elimina todo en orden
inverso. La carga se asegura desde `QtMainWindow`, `QtPreferencesWindow` y
`QtProjectWizardContentSelect`.

### 1.3 Refactor

- `QtMainWindow`: los `setup*Menu()` crean el `QMenu` y delegan en el registro;
  los cuerpos pasaron a `populate*Menu()`.
- `QtPreferencesWindow`: construye la página desde el registro, sin includes
  de CXX/Java.
- `QtProjectWizardContentSelect`: obtiene lenguajes, tipos, iconos,
  descripciones y recomendados del registro; se eliminaron los bloques
  estáticos `#if BUILD_CXX/JAVA`.
- `QtProjectWizard`: `populateSourceGroupContents()` contiene la cadena de
  dynamic casts y se invoca a través del registro.

### 1.4 Null-safety

Si el plugin `ui` no está cargado:

- `updateRecentProjectsMenu()` retorna sin tocar el submenú;
- `addView()` agrega la acción al final o no la agrega si no hay `m_viewMenu`;
- el wizard no hace `constFirst()->click()` sobre una lista vacía.

---

## 2. Verificación

| Prueba | Resultado |
|---|---|
| `ui_registry_test` (root y standalone) | **1/1** |
| `cli_options_test` (root y standalone) | **1/1** |
| Root ctest (`plugin_context`, `plugin_host`, `plugin_abi`, `cli_options`, `ui_registry`) | **5/5** |
| Plugins standalone | **3/3** |
| Registro de lenguajes | **1/1** |
| Indexador Odin | **7/7** |
| GUI desktop build + offscreen 8–12 s | sin crash |
| Odin sample desktop / embedded | **32 / 42 / 0** |
| `tictactoe_cpp` | **383 / 819 / 0** |
| `tutorial-py` completo / shallow | **103 / 263 / 0** · **100 / 187 / 0** |
| API `srctrl_init` + CXX | **383 / 819** |
| Graphify | **27.762 nodos / 81.597 aristas / 813 comunidades** |

CI: se agregaron los jobs `cli-options` y `ui-registry` (Ubuntu, Windows,
macOS), además de los existentes de plugins y registro.

---

## 3. Hallazgos de la auditoría

1. **`plugin_abi_test` no construía sus DLLs** al compilar sólo ese target:
   `$<TARGET_FILE:...>` no crea dependencia y el test fallaba con
   `LoadLibrary` error 126. Se agregó `add_dependencies` en el CMake raíz y en
   el standalone (`607a816`). Root ctest quedó 5/5.
2. **Referencias nulas** en la ventana principal y el wizard si faltaba el
   plugin `ui`; se agregaron guards en `updateRecentProjectsMenu`, `addView` y
   la selección de lenguaje.
3. **El plugin `ui` no se descarga en producción** (`unloadable=false`). Las
   inversas de los registros están probadas, pero un menú ya abierto no se
   refresca automáticamente al descargar. Aceptable mientras el plugin sea de
   proceso; si se quiere HMR, hace falta un `refreshMenus()`.
4. **Listeners de mensajes**: `MessageListenerBase` ya registra en el
   constructor y desregistra en el destructor; no necesitan un efecto extra.
5. **Perfiles**: `embedded` y `desktop` siguen siendo composición de CMake; no
   se agregó un registro de perfiles en runtime.
6. **Python**: el número de aristas varía 263–283 entre corridas con 0 errores;
   el baseline histórico ya oscilaba. No es una regresión.
7. **Seguridad**: 0 identificadores personales, 0 binarios rastreados.

---

## 4. F5: estado

Cerrado en lo funcional:

- acciones/menús de la ventana principal;
- páginas de preferencias;
- listado y contenido del wizard para source groups;
- opciones del CLI standalone.

Opcional / fuera del gate:

- refresco vivo de menús/páginas al descargar el plugin `ui`;
- registro runtime de perfiles de composición;
- recorrido manual interactivo del wizard (el camino de datos y la GUI
  offscreen están verificados).

---

## 5. Pendientes generales

- **Parser Odin**: 31 no resueltas (15 en `image/png` por `using header`, 4
  por un typo del core en `avx.odin`, 3 por `$T` en un `where` anidado, 2 por
  un símbolo inexistente en `linalg`, y casos aislados).
- **Writer**: el coste está en volumen de filas/páginas; índices, caché y
  tamaño de lote ya se descartaron con mediciones.
- **E5**: cross-build ARM64 en Linux/QEMU/placa; no ejecutado en este host.
- **Historial**: purge de `error.log`/`build_log.txt`; requiere aprobación
  explícita y force-push.
- **GUI**: recorrido manual del wizard.

---

## 6. Reproducir

```powershell
cmake -S src/lib_gui/tests -B _study/ui-registry-test `
  -G "Visual Studio 18 2026" -A x64
cmake --build _study/ui-registry-test --config Release
ctest --test-dir _study/ui-registry-test -C Release --output-on-failure

cmake -S src/lib/utility/commandline/tests -B _study/cli-options-test `
  -G "Visual Studio 18 2026" -A x64
cmake --build _study/cli-options-test --config Release
ctest --test-dir _study/cli-options-test -C Release --output-on-failure

cmake --build build --config Release --target Sourcetrail
$env:QT_QPA_PLATFORM = "offscreen"
build\Release\app\Sourcetrail.exe bin\app\user\projects\odin-sample\odin-sample.srctrlprj
```

---

## 7. Seguridad

- Sin rutas personales en archivos rastreados; usar `<WORKSPACE>`/`<STUDY>`.
- Sin binarios rastreados.
- Commits con `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

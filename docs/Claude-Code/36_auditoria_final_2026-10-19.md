# Auditoría final: pendientes y correcciones

**Fecha:** 2026-10-19
**Árbol canónico:** `_work\modernized`
**Base:** `71d1cc3a` + correcciones `5425963c` y `c907eb1b`
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`

Auditoría de cierre tras implementar F3/F5 opcionales, el writer y el parser.
Se revisaron estado del repo, builds desktop/embedded/API, tests raíz y
standalone, la base final del core, seguridad, CI y documentación.

---

## 1. Hallazgos corregidos

### 1.1 Acciones de menú sin dueño

`QtMainWindow` creaba las acciones de historial, bookmarks y proyectos
recientes con `new QAction()` o con padre `this`, y las añadía a menús que
`QMenu::clear()` vacía en cada refresco. Qt no borra las acciones que no son
propiedad del menú, así que cada refresco acumulaba acciones huérfanas hasta
cerrar la ventana.

Correcciones (`5425963c`):

- historial y bookmarks: `new QAction(m_historyMenu)` / `new QAction(m_bookmarksMenu)`;
- proyectos recientes: `new QAction(m_recentProjectsMenu)`;
- el submenú de proyectos recientes se crea con padre `this` y se reutiliza;
- `removeView()` elimina la acción del menú View, la quita de `m_viewActions`,
  la borra y limpia el `QtViewToggle` antes de quitar el dock.

### 1.2 Perfiles runtime sin activación

`PluginProfileRegistry` existía y estaba probado, pero nadie lo activaba fuera
de los tests. Ahora `PluginHost` lee al construirse:

```text
SOURCETRAIL_DISABLE_PLUGINS=ui,cxx,java
```

(separadores `,`, `;`, espacio o tab). Define y activa el perfil
`environment`; los manifiestos bloqueados se rechazan con un error explícito.
Cubierto en `PluginHostTest` (`c907eb1b`).

### 1.3 F3 dinámico

El plugin de lenguaje descargable (`language_test_plugin` +
`language_plugin_abi_test`) quedó verificado: registra un
`SourceGroupFactoryModule` y un `LanguagePackage` reales a través de la
librería compartida, y el host ve el alta y la baja. Es el caso que F3 dejaba
como opcional.

### 1.4 Writer

La base final del core (`odin-core.srctrldb`) conserva **218 tuplas de
`source_location` duplicadas** (0,045 % de 488.728) y **0 rangos de ocurrencia
duplicados**. Los nodos/aristas/errores coinciden con el writer con dedup:
**133.440 / 356.768 / 0**. El cambio es seguro para navegación.

### 1.5 Builds

Después de los cambios se compilaron y ejecutaron:

- desktop (`Sourcetrail`, `sourcetrail_api`, tests raíz);
- embedded (`sourcetrail_cli`, sample 32/42/0);
- standalone de plugins (3/3) y registro de lenguajes (1/1).

---

## 2. Hallazgos abiertos

### 2.1 CI no ejecuta el root ctest — único pendiente de ingeniería

`.github/workflows/cmake-multi-platform.yml` sólo ejecuta los tests
standalone (plugin-context, language-registry, cli-options, ui-registry).
Los tests que requieren el build completo no corren en CI:

- `language_registration` (registro in-process con módulo/paquete reales);
- `language_plugin_abi` (plugin dinámico, requiere `SOURCETRAIL_SHARED_CORE=ON`);
- el resto del root ctest (`plugin_context`, `plugin_host`, `plugin_abi`,
  `cli_options`, `ui_registry`).

Opciones:

1. **Job de build completo** con vcpkg y
   `-DSOURCETRAIL_SHARED_CORE=ON -DBUILD_PLUGIN_CONTEXT_TESTS=ON` en Windows y
   Linux; es el camino directo, pero añade dependencias y minutos de CI.
2. **Harness standalone** para `language_registration` con stubs de
   `SourceGroupFactory`/`LanguagePackageManager`; más trabajo de build y no
   cubre el plugin dinámico.

Recomendación: opción 1, al menos en Windows y Linux. No se implementó en esta
auditoría porque cambia el coste del CI y requiere validar vcpkg en tres OS.

### 2.2 Páginas de preferencias sin refresco vivo

Los menús se refrescan en vivo; las páginas de preferencias se leen al abrir
la ventana, que es modal. No hay un disparador real con el plugin `ui`
integrado (`unloadable=false`). Si en el futuro se quiere HMR de páginas, hay
que reconstruir el contenido del diálogo abierto; queda documentado, no es un
defecto actual.

### 2.3 Parser Odin

**2** referencias no resueltas, ambas `linalg.matrix_inverse_generic`; el
símbolo no existe en el checkout de Odin. Es un defecto upstream de `core`, no
del parser.

### 2.4 E5 (ARM64)

Documentado y asignado a otro desarrollador con hardware ARM64; fuera de los
pendientes de este fork.

### 2.5 GUI

El recorrido manual del wizard/preferencias/menús queda como validación final
del usuario. El camino de datos está cubierto por CLI, API, sample y smoke
offscreen.

---

## 3. Verificación

| Prueba | Resultado |
|---|---|
| ctest raíz (shared core) | **7/7** |
| Plugin standalone | **3/3** |
| Registro de lenguajes standalone | **1/1** |
| Indexador Odin | **10/10** |
| Sample desktop / embedded | **32 / 42 / 0** |
| Core CLI | **133.440 / 356.768 / 0** |
| Parser, no resueltas | **2** (upstream) |
| Writer, lotes de 128 | **23 s** (antes 36 s) |
| CLI core `--threads=1` | **28,9 s** (antes 30,4 s) |
| Base final, ocurrencias duplicadas | **0** |
| GUI offscreen | 5–8 s sin crash |
| Binarios rastreados | **0** |
| Identificadores personales en archivos nuevos | **0** |

---

## 4. Commits

| Commit | Contenido |
|---|---|
| `5425963c` | ownership de acciones de menú y `removeView` |
| `c907eb1b` | activación de perfiles por `SOURCETRAIL_DISABLE_PLUGINS` |
| commit de este documento | doc 36 y actualización de docs 27/35 |

---

## 5. Conclusión

No quedan pendientes funcionales. El único pendiente de ingeniería es llevar
el root ctest al CI (sección 2.1). Todo lo demás es externo (E5, defecto
upstream de Odin) o validación manual de la GUI.

# Cierre de pendientes (excepto E5)

**Fecha:** 2026-10-19
**Árbol canónico:** `_work\modernized`
**Base:** `f8651e8f` (docs 34) y los commits de código posteriores
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`

Este documento cierra los pendientes que quedaban abiertos, salvo **E5**, que
se documenta y se asigna a otro desarrollador con hardware ARM64.

---

## 1. Writer: dedup de source locations

### Diagnóstico

Con `--timing` sobre el core completo, el commit del writer era **30,6 s de
34,5 s** por lote (≈89 %). `DatabaseStorage::addSourceLocation()` ejecutaba un
`SELECT` indexado por cada localización para reutilizar una fila cuando dos
elementos compartían el mismo rango. En el core son ~488.000 búsquedas.

### Cambio

- `SourcetrailDBWriter::setSourceLocationDedup(bool)` y
  `DatabaseStorage::setSourceLocationDedup(bool)`, con valor por defecto
  `true` para no cambiar el comportamiento de otros usuarios del writer.
- El shim `sourcetrail_odin_writer.cpp` lo desactiva al abrir la base: el
  indexador Odin emite una localización por ocurrencia y no necesita compartir
  filas entre elementos.
- No se toca el esquema, los índices, el journal ni la transacción.

### Medición

| Escenario | Antes | Después |
|---|---:|---:|
| Indexador solo, lotes de 128 archivos | 36 s | **23 s** |
| CLI del core, `--threads=1` | 30,4 s | **28,9 s** |
| CLI del core, threads por defecto | 21,0 s | 20,4 s (ruido) |
| Nodos / aristas / errores | 133.440 / 356.768 / 0 | **133.440 / 356.768 / 0** |

El pipeline del core solapa el indexado con otras tareas, por eso la mejora
end-to-end con threads por defecto es pequeña; en el indexador aislado y en
`--threads=1` la reducción es clara.

La base resultante tiene ~258 filas más de `source_location` (0,05 %). Se
comprobó que **no hay rangos de ocurrencia duplicados**: las filas extra son
localizaciones idénticas usadas por elementos distintos, que antes se
compartían y ahora se insertan por separado. La navegación no cambia.

Verificado con la suite Odin **10/10**, el sample **32/42/0** y el core
**133.440/356.768/0** en el CLI.

---

## 2. F3 opcional: registro real de módulo y paquete

Nuevo test `src/lib/plugin/tests/LanguageRegistrationTest.cpp`:

- define `TestModule` (deriva de `SourceGroupFactoryModule`) y `TestPackage`
  (deriva de `LanguagePackage`);
- aplica `LanguagePluginRegistration<TestModule, TestPackage>` con un host
  falso;
- comprueba que `SourceGroupFactory` y `LanguagePackageManager` ganan una
  entrada, que el efecto se registra y que, al disponerlo, ambos vuelven a su
  estado anterior y los objetos se destruyen;
- cubre también el fallo de `add_effect` y el host nulo, verificando que no
  quedan módulos ni paquetes huérfanos.

Para poder observar el estado se añadieron `moduleCount()` y
`packageCount()`. El test enlaza `Sourcetrail_lib` desde el CMake raíz.

El ABI v1 sigue sin callbacks de registro de lenguaje; un plugin **dinámico**
tendría que duplicar los singletons de `lib` o requerir ABI v2. El test cubre
la costura reversible real (alta, dispose, error) que es lo que F3 dejaba
pendiente.

---

## 3. F5 opcional

### 3.1 Refresco vivo de menús

`QtMenuRegistry` ahora invoca un callback después de añadir o quitar un
contribuyente. `QtMainWindow::refreshMenus()` reconstruye los seis menús y
reinserta las acciones de las vistas (dock) que se crean fuera de los
contribuyentes, conservando su orden. El callback usa `QPointer`, así que una
ventana destruida no deja una referencia colgante.

`UiRegistryTest` verifica las notificaciones (alta, duplicado rechazado, baja).
La GUI compiló y pasó un smoke offscreen de 8 s.

Las páginas de preferencias se siguen construyendo al abrir la ventana; como
el diálogo es modal y lee el registro en el constructor, un cambio de páginas
se aplica en la siguiente apertura. No se añadió refresco de una ventana ya
abierta.

### 3.2 Perfiles de composición en runtime

`PluginProfileRegistry` (`src/lib/plugin/PluginProfile.h`) asocia un nombre de
perfil con un conjunto de ids deshabilitados. `PluginHost::load()` y
`loadDynamic()` consultan el perfil activo y rechazan los manifiestos
bloqueados con un error explícito; el perfil vacío mantiene todo habilitado.
`PluginHostTest` cubre activación, perfil desconocido, rechazo y carga tras
limpiar el perfil.

Esto complementa la composición de CMake (`embedded`/`desktop`): no cambia
qué se compila, sólo qué manifiestos se cargan.

---

## 4. Parser Odin

El corte final es el del doc 34: **2** referencias no resueltas en 1.366
archivos, ambas `linalg.matrix_inverse_generic`. No existe ninguna definición
de ese símbolo en el checkout de Odin (`grep -rn` sobre todo el árbol); es un
defecto del fuente de `core`, no del parser. Para código Odin válido, la
cobertura queda completa.

---

## 5. E5 (ARM64): documentado y fuera de la lista

E5 no se hará en este proyecto: el usuario confirmó el 2026-10-19 que no
dispone de ninguna máquina ARM64. Queda documentado para otro desarrollador
con ese hardware:

- triplet/toolchain y notas en `docs/Claude-Code/19_*` y `25_handoff_*`;
- el trabajo consiste en el cross-build ARM64 y la verificación en
  Linux/QEMU/placa;
- no es un pendiente activo de este fork.

---

## 6. GUI: validación final del usuario

El camino de datos está cubierto por CLI, sample, API y GUI offscreen. Lo que
queda es el recorrido manual interactivo (checklist del doc 27): crear el
proyecto Odin, guardar el `.srctrlprj`, indexar 32/42/0, abrir Preferences y
revisar los menús. El usuario lo hará una vez cerrados los demás pendientes.

---

## 7. Estado

| Prueba | Resultado |
|---|---|
| Indexador Odin | **10/10** |
| ctest raíz | **6/6** |
| Sample CLI desktop | **32 / 42 / 0** |
| Core CLI | **133.440 / 356.768 / 0** |
| Core CLI `--threads=1` | **28,9 s** (antes 30,4 s) |
| Indexador solo, lotes de 128 | **23 s** (antes 36 s) |
| GUI offscreen | 8 s sin crash |
| Parser, no resueltas | **2** (defecto de `core`) |

Pendientes para este proyecto: **ninguno**. Quedan sólo la validación manual
de la GUI (usuario), el defecto de `core` de Odin (upstream) y E5 (otro
desarrollador con ARM64).

---

## 8. Commits

| Commit | Contenido |
|---|---|
| `be7992c8` | writer: dedup de source locations opt-out |
| `22d25bf7` | test de registro de lenguaje real |
| `cf795f0b` | refresco de menús al cambiar contribuyentes |
| `c0cc4d87` | perfiles de composición en runtime |
| commit de este documento | doc 35 y actualización del doc 27 |

---

## 9. Reproducir

```powershell
# Indexador y tests
.\src\odin_indexer\build.ps1 -Odin "C:\Software\Odin-oct-2026\odin.exe"
.\src\odin_indexer\tests\run_tests.ps1 `
  -Indexer .\build\odin_indexer\SourcetrailOdinIndexer.exe

# Tests C++ (root)
ctest --test-dir build -C Release --output-on-failure

# Core por CLI, un hilo
build\Release\app\sourcetrail_cli.exe `
  <STUDY>\odin-audit\projects\odin-core.srctrlprj --refresh=all --threads=1
```

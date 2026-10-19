# Auditoría post-compactación y cierre del estado

**Fecha:** 2026-10-18
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Fork:** `git@github.com:5hif7y/sourcetrail.git`, rama `modernized`
**Alcance:** verificar que la compactación no dejó trabajo a medias, revisar paridad
fuente/fork, builds, smokes y seguridad, antes de los estudios Cordis y Odin.

---

## 1. Resumen ejecutivo

- **No se encontraron regresiones de código.** Los bins compilados están presentes, los
  smokes registran `0 Fatal Errors` y la paridad de fuentes con el fork es correcta.
- **Sí había dos artefactos con rutas personales rastreados en el fork.** Se eliminaron:
  `79c7b5b6` (`error.log`) y `ade8d23e` (`build_log.txt`). El HEAD actual del fork es
  `ade8d23e` y el árbol rastreado ya no contiene identificadores personales.
- **El único pendiente funcional sigue siendo E5**: la cross-compilación ARM64 no se
  ejecutó porque este host Windows no tiene `aarch64-linux-gnu-gcc`. El triplet, el
  toolchain y la documentación están listos; falta verificarlo en Linux/QEMU/placa.
- **Deuda de tooling:** el índice Graphify es anterior a E2–E5 y está desactualizado.
  No afecta al producto; conviene regenerarlo después de estos estudios.
- **Conclusión:** se puede continuar con los estudios 21 (Cordis/plugins) y 22 (Odin)
  sin bloquearse por problemas del proyecto.

---

## 2. Git y paridad

| Verificación | Resultado |
|---|---|
| Fork `_audit/fork` | limpio, rama `modernized`, HEAD `ade8d23e` |
| Remoto | `origin/modernized` sincronizado (sin ahead/behind) |
| Árbol de trabajo `neo-sourcetrail` | rama `master`, HEAD `20ffe0b`, sin remoto; sucio por diseño |
| Archivos clave del fork | `SourcetrailLibExport.h`, `DebugStubs.cpp`, `SourcetrailCxxExport.h`, `cmake/toolchains`, `vcpkg-triplets`, docs 15–19: **todos rastreados** |
| Paridad directa | 5 diferencias: `error.log` (ya resuelto) y 4 docs locales con rutas/encoding; el fork tiene las versiones redactadas |
| Paridad inversa | todos los archivos rastreados del fork existen en el árbol; sólo falló un nombre Unicode por una limitación de `Test-Path` en PowerShell |
| Entradas locales no portadas | 3.721, todas artefactos de build, logs, `graphify-out`, scripts temporales o `target/`; ninguna fuente del producto |

Las copias locales de `docs/Claude-Code/03`, `05`, `06` y `07` tenían rutas absolutas y
mojibake; se reemplazaron por las versiones redactadas del fork (`<WORKSPACE>`, UTF-8).
También se eliminaron dos residuos locales: `build_log.txt` y un
`src/lib_gui/qt/QtMainWindow.cpp` de **0 bytes** en la ruta equivocada (el archivo real
es `src/lib_gui/qt/window/QtMainWindow.cpp`).

El árbol `neo-sourcetrail` sigue en `master` con los cambios sin commitear y sin remoto:
es el espacio de trabajo histórico. **La copia canónica es el fork**, no ese árbol.

---

## 3. Seguridad: hallazgos y corrección

### 3.1 `error.log` (corregido en `79c7b5b6`)

- Estaba rastreado en el fork (3.670 bytes) y contenía rutas absolutas del desarrollador.
- Estaba en **UTF-16**, así que `git grep` y los escaneos de texto lo trataban como
  binario y no lo detectaban. Ése fue el motivo por el que pasó el filtro anterior.
- Se eliminó del árbol, se agregó `error.log` a `.gitignore` y se pusheó.

### 3.2 `build_log.txt` (corregido en `ade8d23e`)

- Estaba rastreado (14.660 bytes) y era un log de MSBuild con rutas absolutas.
- Se eliminó, se agregó `build_log.txt` a `.gitignore` y se pusheó.

### 3.3 Estado posterior

Un escaneo de **todos los archivos rastreados** (incluyendo UTF-16) no encontró los
identificadores personales del usuario (nombre de cuenta de Windows, ruta del workspace
original ni ruta del árbol de estudio). Los únicos falsos positivos fueron:

- `DOCUMENTATION.md`: un ejemplo genérico de ruta de usuario;
- tests upstream de Sourcetrail con rutas de ejemplo de sus autores;
- doc 07: la propia lista de patrones de redacción.

**Residual de historial:** ambos archivos siguen existiendo en el commit base
`5f735e9e`, ancestro de `modernized`. El árbol actual está limpio, pero un `git log`
todavía puede mostrar esos blobs. Para borrarlos del historial público hace falta un
`git filter-repo --path error.log --path build_log.txt --invert-paths` y un
force-push, que reescribe todos los hashes. No se hizo porque es una operación
destructiva y outward-facing; queda a decisión explícita. Si se hace, hay que actualizar
las referencias a commits en `state.json` y en los docs 07–19.

---

## 4. Builds, smokes y logs

| Artefacto | Estado |
|---|---|
| `build\Release\app\sourcetrail_cli.exe` (desktop, core compartido) | 65.024 B |
| `build-embedded\Release\app\sourcetrail_cli.exe` (estático) | 1.700.864 B |
| `build\Release\app\Sourcetrail.exe` | 2.927.612 B |
| Plugins C++/Java | presentes junto al exe y en `plugins/` |
| Logs E4 | 0 errores de compilación |
| Smokes E4 C++/Java/Python | `0 Fatal Errors`; los `errors=1` del conteo eran ese texto, no fallos |
| Último smoke GUI desktop | `GUI closed, exit=0` |

No se reconstruyó desde cero en esta pasada: los artefactos y logs son los de E5 y su
contenido coincide con el árbol. Los dos commits nuevos sólo eliminan logs y ajustan
`.gitignore`; no cambian fuentes ni CMake.

---

## 5. Pendientes y deuda

| Ítem | Severidad | Estado |
|---|---|---|
| E5: cross-build ARM64 no ejecutado | media | pendiente de host Linux/QEMU/placa |
| Historial del fork con `error.log`/`build_log.txt` | media | HEAD limpio; purge opcional |
| Índice Graphify anterior a E2–E5 | baja | regenerar con `graphify --update` |
| Árbol de trabajo sucio en `master` | informativa | por diseño; el fork es canónico |
| Residuos locales (`err.txt`, `out.txt`, scripts de chequeo) | baja | no rastreados; limpiar si se desea |

### E5: qué falta exactamente

En un host Linux con `aarch64-linux-gnu-gcc` (o en la placa/QEMU):

1. `cmake --preset` con el toolchain `cmake/toolchains/aarch64-linux-gnu.cmake` y el
   triplet `vcpkg-triplets/arm64-linux-embedded.cmake`.
2. Build de `sourcetrail_cli`.
3. `file` del binario (ELF aarch64) y smoke de indexado.
4. Runtime del target: Python + `jedi`/`parso`/`numbat` si se indexa Python, o base
   pre-indexada/`SOURCETRAIL_PYTHON_SHALLOW=1`.

---

## 6. Conclusión

El estado es sano: no hay código perdido ni builds rotos, y los dos problemas de
seguridad detectados en esta auditoría ya están corregidos en el HEAD del fork. Los
estudios de arquitectura Cordis (doc 21) y de soporte Odin (doc 22) pueden ejecutarse
sin depender de E5.

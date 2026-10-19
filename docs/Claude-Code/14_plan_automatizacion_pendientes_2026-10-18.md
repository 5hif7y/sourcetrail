# Plan de automatización del backlog (E2–E5, B1)

**Fecha:** 2026-10-18
**Autor:** Claude Code
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Objetivo:** completar los pendientes de `11_plan_embedded` de forma reproducible:
cada ítem se implementa, se valida con la matriz de builds/smoke, se mide, se documenta,
se commitea y se pushea; el runner mantiene el estado y se detiene ante el primer gate
fallido.

---

## 1. Principios

1. **Un ítem a la vez.** Los builds son pesados (memoria/CPU); no se paralelizan builds
   de perfiles distintos.
2. **Gate antes de commit.** Ningún ítem se commitea/pushea sin: build 0 errores,
   smoke de los tres lenguajes + GUI, conteos esperados y redacción de datos personales.
3. **Idempotencia y reanudación.** `state.json` registra el estado; un ítem `done` se
   saltea; un `blocked` deja logs y no avanza.
4. **Reversibilidad.** Un commit por ítem; si un gate posterior falla, se revierte el
   commit y se deja el diagnóstico en `BLOCKED.md`.
5. **Espacio acotado.** Los builds de perfiles de prueba se borran tras el gate; el
   build desktop se conserva sólo si se pide.
6. **Sin datos personales.** Los docs se escanean contra los identificadores personales
   conocidos (usuario, workspace y workspace original) antes de commitear.

---

## 2. Arquitectura de automatización

Todo vive en `<WORKSPACE>\_tools\auto\` (fuera del repo; no se commitea):

| Archivo | Función |
|---|---|
| `state.json` | Estado por ítem: `pending/in_progress/done/blocked`, commit, métricas, timestamp, último error. |
| `common.ps1` | Paths, logging, `Invoke-Configure`, `Invoke-Build`, `Test-BuildErrors`, `Invoke-Smoke`, `Test-Redaction`, `Update-State`. |
| `run_item.ps1` | `-Item E2|E3|E4|E5|B1`; ejecuta los gates del ítem y actualiza el estado. |
| `bench.ps1` | Benchmarks por perfil: `embedded`, `cli-java`, `cli-cxx`, `desktop`. |
| `smoke.ps1` | Indexa tutorial (C++), javaparser (Java), tutorial-py (Python), abre/cierra GUI. |
| `cleanup.ps1` | Borra builds de prueba/logs reproducibles; deja el build principal. |
| `README.md` | Uso y convenciones. |

**Qué automatiza el runner:** configure/build de la matriz, conteo de errores, smoke
tests, benchmarks, verificación de redacción, commit/push y estado. **Qué no puede
automatizar:** escribir la implementación de cada ítem; eso lo hace Claude guiado por
la spec de §4. Si se quisiera generación automática de código, habría que usar agentes
(ver §8, opción B), con el riesgo de conflictos en CMakeLists/docs.

### 2.1 `state.json` (esquema)

```json
{
  "updated": "2026-10-18T23:00:00-03:00",
  "items": {
    "E1": { "status": "done", "commit": "6f03bbf4", "metrics": { "embedded_s": 311.9, "embedded_gb": 0.6 } },
    "E2": { "status": "pending", "commit": null, "metrics": {}, "last_error": null },
    "E3": { "status": "pending" },
    "E4": { "status": "pending" },
    "B1": { "status": "pending" },
    "E5": { "status": "pending" }
  }
}
```

### 2.2 Flujo por ítem

```
load state
if item.status == done: exit 0
item.status = in_progress
implementar (Claude) + docs
for gate in item.gates:
    ejecutar; si falla -> status=blocked, log, BLOCKED.md, exit 1
commit + push (mensaje del ítem)
item.status = done; metrics; save state
```

---

## 3. Dependencias y orden

| Orden | Ítem | Depende de | Motivo |
|---|---|---|---|
| 1 | **E2** límites de recursos | E1 | Cambia el CLI y settings; base para E3. |
| 2 | **E3** daemon Python | E2 | Reutiliza el modo shallow/refresh y el comando Python. |
| 3 | **E4** tamaño/RAM | E1 | Flags/strip/PDB; independiente de E3, pero secuencial por CPU. |
| 4 | **B1** presets/submódulo | E1 | Cierre de configuración. |
| 5 | **E5** cross-compile/empaquetado | E1/E4 | Último: toolchain ARM64 y docs. |
| — | I6 batch Java | E1 | Opcional; sólo con lote 4–8 y heap bajo (no para embedded). |
| — | I7 / 0,2 % | — | Cerrados. |

---

## 4. Specs y gates por ítem

### E2 — Límites de recursos

**Implementación:**
- `utilityApp.cpp/h`: `SOURCETRAIL_INDEXER_THREADS` (env) con prioridad; default
  `max(1, min(hardware_concurrency, 2))`; desktop preset sube el valor.
- `JavaEnvironmentFactory.cpp`: `SOURCETRAIL_JAVA_HEAP` o auto
  `min(2048 MB, max(256 MB, 25 % RAM))`; `-Xms = min(64, heap/4)`.
- `src/cli/main.cpp`: `--refresh=all|updated` (default updated), `--threads=N`,
  `--java-heap=...`, `--python-shallow`.
- `SourceGroupSettingsPythonEmpty`: setting/env `SOURCETRAIL_PYTHON_SHALLOW` agrega
  `--shallow`.
- `PersistentStorage`: FTS opcional (`indexing/full_text_search_enabled`, default OFF en
  embedded) y `PRAGMA cache_size=-2000`.
- Docs: `BUILD_EMBEDDED.md` / `BUILD_WINDOWS.md`.

**Gates:**
1. Builds: embedded, CLI+Java, CLI+CXX, desktop → 0 errores.
2. Smoke: C++ 99/153/21, Java 4.126/25.601/138, Python 103/177/20, GUI exit 0.
3. `SOURCETRAIL_INDEXER_THREADS=1` se refleja (log/medición de workers).
4. `SOURCETRAIL_JAVA_HEAP=512m` arranca la JVM y indexa javaparser.
5. Segunda indexación con `--refresh=updated` < 10 % del tiempo de la primera.
6. Shallow Python indexa tutorial-py con counts coherentes (documentar diferencias).
7. FTS off no rompe la indexación; RSS medido.
8. Redacción + fork limpio.

### E3 — Daemon Python

**Implementación:** `run.py` cliente liviano + daemon por base (named pipe/Unix socket),
timeout, fallback local, `SOURCETRAIL_PYTHON_DAEMON=0`; docs.

**Gates:**
1. Counts idénticos (tutorial-py 103/177/20; muestra matplotlib).
2. Sin procesos huérfanos tras cerrar.
3. Fallback probado matando el daemon a mitad.
4. Benchmark: 2,44× en serie (tutorial), ~6 % en corpus grande.
5. Build embedded 0 errores.

### E4 — Tamaño/RAM

**Implementación:** `SOURCETRAIL_EMBEDDED` con `/O1 /Gy /Gw` u `-Os` + secciones;
strip; no copiar PDBs; triplet estático; datos selectivos (ya en E1).

**Gates:**
1. Embedded build size y CLI size medidos (objetivo < 60 MB sin runtime Python).
2. Desktop con `SOURCETRAIL_EMBEDDED=OFF` sigue compilando y midiendo `/O2`.
3. Deploy sin `*.pdb`.
4. Smoke completo.

### B1 — Presets y submódulo vcpkg

**Implementación:** toolchain fallback (`VCPKG_ROOT`/submódulo), overlays ya presentes;
documentar `git submodule update --init`; verificar presets.

**Gates:** `cmake --preset embedded-release` (o configure equivalente) 0 errores;
`vcpkg install --dry-run` sin paquetes inesperados; docs.

### E5 — Cross-compile y empaquetado

**Implementación:** triplet `arm64-linux` embedded, toolchain, `BUILD_EMBEDDED.md`,
empaquetado (CLI + core + datos + Python runtime), flujo pre-indexado.

**Gates:**
1. Configure/build cruzado 0 errores (o documentar bloqueo si no hay toolchain).
2. Artefacto ARM64 inspeccionado (`file`); smoke en QEMU si está disponible.
3. Tamaño del paquete; instrucciones reproducibles.

---

## 5. Matriz de validación común

| Perfil | Configure | Build | Artefactos |
|---|---|---|---|
| Embedded | sin Qt/LLVM/Java | CLI-only | CLI, python, fallback/license |
| CLI+Java | GUI off, Java on | plugin Java | plugin, jar, data/java |
| CLI+CXX | GUI off, CXX on | plugin C++ + indexer | plugin, indexer, data/cxx |
| Desktop | GUI/CXX/Java on | GUI + ambos plugins | exe, cli, plugins, data completa |

**Smoke mínimo:** C++ 99/153/21; Java 4.126/25.601/138; Python 103/177/20; GUI exit 0;
ambos plugins cargan en desktop.

---

## 6. Presupuesto

| Ítem | Esfuerzo | Builds/benchmarks |
|---|---|---|
| E2 | 1–2 días | 4 perfiles + 2 benchmarks |
| E3 | 2–3 días | embedded + smoke + benchmark |
| E4 | 1–2 días | embedded + desktop |
| B1 | 0,5 día | configure/dry-run |
| E5 | 2–3 días | cross + QEMU |
| **Total** | **~7–11 días** | builds de ~5–6 min; benchmarks aparte |

Espacio: builds de prueba ~1–3 GB; se borran tras cada gate. El desktop (34,5 GB) se
conserva sólo si se pide.

---

## 7. Política de fallos y rollback

- Primer gate fallido → `status=blocked`, logs en `_auto/`, no commit/push.
- `BLOCKED.md` con: ítem, gate, comando, error, archivos afectados, próximo paso.
- Si el fallo ocurre después de commitear, revertir ese commit (`git revert`), no
  reescribir historia ya pusheada.
- Nunca `--force` al remoto.
- Antes de commitear: `Test-Redaction` sobre docs; `git status` limpio salvo lo del ítem.

---

## 8. Opciones de ejecución

- **A (recomendada): runner secuencial determinista.** Un ítem a la vez, gates
  estrictos, commits pequeños. Es la que implementa este plan.
- **B: agentes/workflow en paralelo.** E3 y E4 son independientes; se podrían correr en
  worktrees separados con un verificador. Requiere opt-in explícito y cuidado con
  conflictos en `CMakeLists.txt`/docs. No se usa por defecto.
- **C: `/loop` de reanudación.** Para sesiones largas: el runner se invoca por ítem y
  `ScheduleWakeup`/`/loop` retoma tras cada build. No cambia los gates.

---

## 9. Puesta en marcha

1. Crear `_tools/auto/` con `state.json`, `common.ps1`, `run_item.ps1`, `bench.ps1`,
   `smoke.ps1`, `cleanup.ps1`, `README.md`.
2. Marcar E1 `done` en `state.json`.
3. Ejecutar `run_item.ps1 -Item E2`; implementar E2 según §4 hasta que los gates pasen.
4. Commit/push del ítem + doc; actualizar estado; continuar con E3.
5. Al cerrar todos: doc de cierre, fork limpio, `state.json` todo `done`.

**Criterio de terminado:** E2–E5 y B1 en `done`, builds de la matriz verdes, smoke
completo, docs 14/15/… actualizados, fork `modernized` pusheado y `state.json`
consistente.

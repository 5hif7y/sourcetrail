# Cierre del re-run de matplotlib y estado del proyecto

**Fecha:** 2026-10-18 03:23 (AST)
**Autor:** Claude Code
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Antecedente:** `03_auditoria_y_cambios.md` y `04_plan_performance.md`

---

## 1. Re-run completo de matplotlib — cerrado

Se relanzó el indexado completo con el binario que ya incluye todos los fixes
(§2.6 jedi, §2.7 UTF-8) y un timeout de 5 h.

```
commands started:      913
finished successfully: 913
non-zero return codes:   0
archivos en disco:     913
archivos indexados:    913
faltantes:               0
```

| | Corrida anterior | Re-run final |
|---|---:|---:|
| Archivos | 904 / 913 | **913 / 913** |
| Nodos | 25.378 | **25.973** |
| Aristas | 590.804 | **622.480** |
| LOC | 260.786 | **281.546** |
| Fallos del indexer | 9 | **0** |
| Tiempo | 3 h 28 min | 3 h 29 min |

- Los 2 nodos `unsolved symbol` restantes son referencias genuinamente irresolubles del
  código, no fallos del indexador.
- El tiempo no subió pese a indexar 9 archivos más (varios grandes) porque los fixes
  eliminan el trabajo perdido por crashes.
- Base resultante: `<USERPROFILE>\pruebas-sourcetrail\matplotlib\matplotlib-10-17-2026.srctrldb`.

## 2. Documentación actualizada

- `03_auditoria_y_cambios.md` — resultado final, comparativa y pendientes.
- `04_plan_performance.md` — plan de optimización de compilación e indexación.
- Índice de Graphify regenerado: 65.512 nodos / 185.135 aristas.

## 3. Resumen del plan de performance

**Compilación** (~470 TU; plugin C++ de 51,8 MB con LLVM):

1. Sacar Maven del `ALL` (hoy corre en cada build).
2. PCH en `lib_gui` (161 TU re-parsean los mismos headers de Qt).
3. Copia de `Sourcetrail_lib.dll` en un target propio (hoy exige compilar la GUI).
4. Unity build, exports explícitos y, al final, desacoplar el plugin por C-API.

**Indexación** (el costo está en el modelo de ejecución):

1. Python batch/daemon: hoy se pagan 0,94 s de arranque por archivo (~15 min sólo en
   matplotlib); ahorro estimado 30–50 %.
2. Java: cachear el `FileSystem` de JDT por thread (elimina el O(archivos × roots)).
3. Inject paralelo (backpressure visto en kafka).
4. Baratas: 4 hilos en el CLI, `-Xmx` en la JVM y bajar el ruido de logs (kafka generó
   12.357 repeticiones del mismo warning).

## 4. Pendientes

- Aplicar la fase 1 del plan (Maven condicional, copia del DLL, hilos del CLI, logging,
  `-Xmx`).
- Problemas menores de Java: `-Xmx`, `s_nextParserId++` no atómico, `catch (Exception)`
  que no cubre `Error`, estándar `"26"` ofrecido por `ToolChain.cpp`.
- **Tarea 3**: portar todo al fork `git@github.com:5hif7y/sourcetrail.git` en commits
  chicos, sin empezar todavía.

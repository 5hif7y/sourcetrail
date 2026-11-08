# Soporte de Odin: estudio de viabilidad

**Fecha:** 2026-10-18
**Árbol:** `<WORKSPACE>\Sourcetrail-profundo\neo-sourcetrail`
**Repos leídos:**
- Odin: <https://github.com/odin-lang/Odin> (commit `c6e2f0b`).
- Gramática: <https://github.com/ap29600/tree-sitter-odin> (commit `4d6b644`,
  2023-05-16).
- Referencia de integración: `docs/Claude-Code/02_como_escribir_un_indexer.md`.

---

## 1. Resumen ejecutivo

**Sí es viable**, pero no con la gramática tree-sitter actual tal cual.

- La gramática `tree-sitter-odin` es de **mayo de 2023** y está muy atrasada respecto
  del Odin de 2026. En una muestra de 1.416 archivos de `core` + `base`, **1.187
  (83,8 %)** producen nodos de error; **los 305 archivos que empiezan con `#+build`
  fallan todos**.
- Las construcciones básicas —`package`, `struct`, `enum`, `proc`, atributos
  `@(private)`— **sí parsean** en archivos mínimos. El problema son las directivas y
  sintaxis nuevas: `#+build`, `#assert`, `#type`, `bit_field`, atributos con argumentos,
  `#optional_allocator_error`, `#no_bounds_check`, etc.
- La solución recomendada es **propia e independiente**: mantener nuestro indexador y
  nuestro mapeo a Sourcetrail, y **actualizar/forkear la gramática** usando el EBNF
  oficial de Odin (`odin.ebnf`). No se usa ningún plugin de Odin de otro proyecto de
  Sourcetrail.
- El compilador oficial de Odin está escrito en C++ y tiene licencia zlib, pero es
  monolítico (`src/parser.cpp`, `src/checker.cpp`, etc.); no es una librería embebible.
  Sirve como oráculo de pruebas (`odin check`), no como parser del plugin.
- **Primer paso recomendado:** indexador externo C++ por archivo, con gramática
  actualizada, y luego empaquetarlo como plugin cuando exista el host de plugins del
  doc 21. El MVP es de **10–15 jornadas**; la versión completa, **3–4 semanas**.

---

## 2. Odin: lo que hay que indexar

Odin es un lenguaje compilado, de tipado estático y orientado a datos. Para Sourcetrail
interesan las declaraciones y referencias, no la evaluación semántica completa.

| Construcción Odin | Ejemplo | Nodo Sourcetrail | Referencia |
|---|---|---|---|
| Archivo | `foo.odin` | `NODE_FILE` | — |
| Paquete | `package fmt` | `NODE_PACKAGE` | — |
| Import | `import "core:fmt"` / `import f "foo"` | `NODE_PACKAGE`/`NODE_MODULE` | `IMPORT` |
| Struct | `User :: struct { ... }` | `NODE_STRUCT` | `TYPE_USAGE` |
| Union | `Value :: union { int, string }` | `NODE_UNION` | `TYPE_USAGE` |
| Enum | `Color :: enum { Red, Green }` | `NODE_ENUM` + `NODE_ENUM_CONSTANT` | `TYPE_USAGE` |
| `bit_set` | `Flags :: bit_set[Flag]` | `NODE_TYPEDEF`/`NODE_TYPE` | `TYPE_USAGE` |
| `bit_field` | `Bits :: bit_field u8 { a: bool \| 1 }` | `NODE_STRUCT` + `NODE_FIELD` | `TYPE_USAGE` |
| `distinct` | `Id :: distinct u64` | `NODE_TYPEDEF` | `TYPE_USAGE` |
| Proc | `add :: proc(a, b: int) -> int` | `NODE_FUNCTION` | `CALL`/`TYPE_USAGE` |
| Campo | `name: string` | `NODE_FIELD` | `TYPE_USAGE` |
| Constante/variable de paquete | `Value :: 42` / `x: int` | `NODE_GLOBAL_VARIABLE` | `USAGE` |
| Parámetro polimórfico | `$T` | `NODE_TYPE_PARAMETER` | `TYPE_USAGE` |
| `using` (embedding) | `using base: Base` | `NODE_FIELD` | `INHERITANCE` |
| `foreign` | `foreign import "lib"` | `NODE_FUNCTION`/`NODE_FIELD` | `IMPORT` |
| Atributo | `@(private, require_results)` | `NODE_ANNOTATION` (opcional) | `ANNOTATION_USAGE` |

Notas:

- Odin no tiene clases ni herencia. `using` incrusta campos; se modela como
  `INHERITANCE` hacia el tipo incrustado, que es lo más cercano en Sourcetrail.
- No hay un `NODE_CONSTANT`; las constantes de paquete se indexan como
  `NODE_GLOBAL_VARIABLE`. Es una limitación del esquema, no del indexador.
- Los `proc` con un primer parámetro receptor (`self: ^T`) se pueden marcar como
  `NODE_METHOD` por convención; el resto, `NODE_FUNCTION`.
- Los comentarios de documentación (`//` antes de una declaración) no tienen un campo
  de doc en el esquema actual; se pueden indexar como `NODE_ANNOTATION` o ignorar en
  el MVP.

---

## 3. Prueba de concepto de la gramática

### 3.1 Montaje

Se compiló un arnés C mínimo (`<STUDY>\odin-parse`) contra:

- `tree-sitter` runtime actual (ABI de lenguaje 15, mínimo compatible 13);
- `tree-sitter-odin/src/parser.c` (ABI 14, generado en 2023);
- MSVC 19.50, C11, generador Visual Studio 18 2026.

El ejecutable resultante (`odin_parse.exe`, 1.039.360 B) acepta archivos o una lista y
reporta, por archivo: cantidad de nodos de error, primer error (línea, columna, tipo).

### 3.2 Resultados sobre `core` + `base`

| Métrica | Valor |
|---|---|
| Archivos `.odin` analizados | 1.416 |
| Archivos con al menos un error | **1.187 (83,8 %)** |
| Archivos limpios | 229 (16,2 %) |
| Nodos de error totales | 39.832 |
| Tiempo, una pasada en frío | 27,93 s |
| Tiempo, pasada en caliente | 8,7 s |
| Archivos que empiezan con `#+build` | 305; **305 con errores** |
| Archivos sin `#+build` | 1.111; 882 con errores; 229 limpios |

La muestra incluye código de plataforma, `vendor` y `tests`; no es una métrica de
“calidad” del lenguaje, sino de **cobertura de la gramática**.

### 3.3 Clasificación del primer error

| Constructo en la primera línea del error | Archivos |
|---|---:|
| Etiqueta `#+...` (`#+build`, `#+private`, `#+feature`) | 565 |
| Otros (en su mayoría cascadas) | 405 |
| `#assert` | 52 |
| Otra directiva `#...` | 34 |
| `#` no clasificada | 32 |
| `when` | 26 |
| Comentario/línea vacía | 20 |
| `or_return` / `or_else` / `or_break` | 13 |
| `#type` | 9 |
| Atributo `@(...)` | 9 |
| `#no_bounds_check` | 7 |
| `foreign` | 6 |
| `bit_set` | 3 |
| `#force_inline` | 3 |
| `#optional_allocator_error` | 2 |
| `using` | 1 |

Las “cascadas” explican por qué un solo `#type` o un `#assert` puede generar cientos de
nodos de error: el parser pierde el hilo de la declaración y reinterpreta el resto del
archivo. Ejemplos concretos:

- `core/fmt/fmt.odin`: `User_Formatter :: #type proc(...)`; el error en la línea 78
  (`Register_User_Formatter_Error :: enum { ... }`) es una cascada, no un enum inválido.
- `core/strings/strings.odin`: `#optional_allocator_error #no_bounds_check` en la firma
  de `join`.
- `core/container/pool/pool.odin`: `@(require_results, synchronizes=.Acquire)`.
- `core/c/libc/types.odin`: `#assert(...)`.
- `core/container/pool/pool_arena_virtual.odin`: `#+build ...` en la línea 1.

### 3.4 Archivos mínimos

| Archivo | Resultado |
|---|---|
| `min_enum.odin` (`enum` con variantes) | limpio |
| `min_struct.odin` (`struct` + `proc`) | limpio |
| `min_proc.odin` (`proc` + slice abierto `x[1:]`) | limpio |
| `min_attr.odin` (`@(private)`) | limpio |
| `min_build.odin` (`#+build windows, linux`) | **2 errores en la línea 1** |

Conclusión: el parser reconoce bien el núcleo del lenguaje; lo que falta es la capa de
directivas y sintaxis moderna. **No hace falta escribir un parser desde cero para el
MVP**, pero sí actualizar la gramática.

### 3.5 Ejemplos del repo

Los 11 archivos de `examples/` usan `#+build` en la línea 1: **9 fallan en la línea 1**
y 2 quedan limpios. Es consistente con el resto: el tag de archivo es el mayor bloqueo
individual.

---

## 4. Opciones de parser

| Opción | Ventajas | Desventajas | Veredicto |
|---|---|---|---|
| **A. tree-sitter actual sin tocar** | ya compila, rápido, MIT, parser.c generado | 84 % de archivos con error; no sirve para producción | sólo PoC |
| **B. Forkear/actualizar tree-sitter-odin** | rápido, incremental, error recovery, MIT; se ajusta con `odin.ebnf` | hay que mantener la gramática; parser.c de ~7,7 MB de fuente | **recomendada** |
| **C. Parser del compilador Odin** | gramática exacta y actualizada; checker disponible | monolito C++ (~2 MB de fuentes, muchas dependencias); sin API de librería limpia; no es “independiente” | sólo oráculo de tests |
| **D. Parser propio a mano** | control total, tamaño pequeño, sin dependencias | semanas de trabajo; error recovery difícil; mantenimiento alto | no para el MVP |
| **E. Python + tree-sitter** | prototipo muy rápido; reutiliza el pipeline Python | dependencia Python + wheels; más lento; no es un plugin nativo | PoC descartable |

**Recomendación:** **B + indexador propio en C++**. La gramática es una dependencia
MIT pequeña y aislada; el valor propio está en el extractor, el resolver de scopes y el
mapeo a Sourcetrail, no en reescribir un parser de 15 KB de EBNF.

El compilador Odin tiene licencia zlib y su EBNF oficial
(`odin.ebnf`, 15.916 B) incluye construcciones que la gramática 2023 no tiene, por
ejemplo `BitFieldType`. Ese archivo es la fuente para actualizar la gramática.

---

## 5. Diseño del indexador

### 5.1 Modelo externo por archivo (MVP)

Se reutiliza el camino que ya funciona para Python:

```text
sourcetrail_odin_indexer index \
  --source-file-path="<archivo>" \
  --database-file-path="<db>"
```

- `SourceGroupSettingsOdinEmpty : SourceGroupSettingsCustomCommand`.
- Extensiones: `.odin`.
- `setRunInParallel(true)`: cada archivo escribe su propia DB temporal; el core fusiona.
- El indexador abre la DB, escribe nodos/aristas/ocurrencias, hace commit y sale 0.
- Si el parseo tiene errores, **no aborta**: registra `recordError(...)` y indexa lo que
  pudo recuperar. Un archivo roto no debe tumbar el proyecto.

### 5.2 Resolución de nombres

El modelo por archivo no comparte memoria entre invocaciones. Para no perder navegación
cross-file:

1. **Tabla de paquete:** al indexar un archivo, escanear los `.odin` del mismo
   directorio (mismo `package`) y construir un mapa `nombre → declaración`. Es barato y
   se puede cachear en un archivo temporal por paquete, invalidado por mtime.
2. **Imports:** mapear el alias (`import f "core:fmt"`) al paquete; resolver
   `f.println` por nombre calificado. Los imports de `core:`/`base:`/`vendor:` se
   resuelven contra la ruta del SDK si está configurada; si no, se dejan como
   `IMPORT` sin destino.
3. **Locales:** pila de scopes para parámetros, variables, `for`, `switch`, `defer`,
   `or_return`.
4. **Genéricos:** `$T` se registra como `NODE_TYPE_PARAMETER`; las instanciaciones
   concretas no se resuelven en el MVP (no hay monomorfización).
5. **No resueltos:** se emite el nodo literal de símbolo no resuelto que ya usa el
   indexador Python; la GUI lo muestra con tooltip en vez de navegar.

Para resolución cross-file completa y rápida, el modelo **nativo** (un solo proceso)
es superior: comparte la tabla de símbolos y evita reescanear paquetes. Por eso el plan
es MVP externo → plugin nativo.

### 5.3 Casos especiales

| Caso | Decisión MVP |
|---|---|
| `#+build` / `#+feature` / `#+private` | Indexar el archivo igual; opcional `--platform` para filtrar |
| `when` | Indexar todas las ramas (todas son código válido); evitar duplicados por nombre |
| `foreign` / `foreign import` | Indexar como funciones/campos con `IMPORT` |
| `using` | `NODE_FIELD` + `INHERITANCE` al tipo incrustado |
| `bit_field` | `NODE_STRUCT` con campos y `NODE_FIELD`; el ancho (`\| expr`) se ignora |
| `@(...)` | `NODE_ANNOTATION` o se ignora; no afecta la navegación |
| Doc comments | Se ignoran en el MVP |
| Identificadores Unicode | Soportados por el lexer de Odin; el indexador debe tratar el nombre como UTF-8 |
| Archivos de test inválidos | No forman parte del proyecto; excluirlos por path |

---

## 6. Integración en Sourcetrail

| Opción | Cómo | Pros | Contras | Cuándo |
|---|---|---|---|---|
| **A. Python externo** | `run_odin.py` + tree-sitter Python + `sourcetraildb` | prototipo en días; sin tocar CMake | Python + wheel; más lento; no es plugin | PoC |
| **B. C++ externo** | `sourcetrail_odin_indexer.exe` + `SourceGroupSettingsOdinEmpty` | sin Python; rápido; proceso aislado; fácil de medir | resolución cross-file limitada; nuevo exe | **MVP** |
| **C. Plugin nativo** | `src/lib_odin` con `LanguagePackageOdin`, `SourceGroupFactoryModuleOdin`, `IndexerOdin` | integración total; un proceso; wizard; unloadable | requiere host de plugins y tocar el core | tras doc 21 F3 |
| **D. Estático** | `lib_odin` enlazado al core embedded | cero DLL; ideal para placa | rebuild del core al cambiar Odin | embedded final |

**Recomendación:** B para el MVP, C/D cuando F3 esté listo. El `SourceGroupType` y el
`LanguageType` se agregan una vez; con el registro por ID del doc 21, el plugin los
registra sin tocar enums.

---

## 7. Plan de implementación

| Paso | Entregable | Gate | Est. |
|---|---|---|---|
| 1 | Fork de `tree-sitter-odin` actualizado: `#+...`, `#assert`, `#type`, `bit_field`, atributos con args, `#optional_allocator_error`, `#no_bounds_check`, `matrix`, `or_*` | ≥95 % de los 1.416 archivos sin errores (excluyendo tests inválidos) | 3–5 d |
| 2 | Extractor de declaraciones: package, imports, structs, enums, procs, campos, constantes | 20 archivos de muestra con nodos correctos | 2–3 d |
| 3 | Resolver v0: scopes, imports, calls, tipos, `using`, no resueltos | ≥80 % de referencias resueltas en el sample | 3–5 d |
| 4 | `SourceGroupSettingsOdinEmpty` + sample + CLI | `sourcetrail_cli` indexa el sample; counts estables | 2–3 d |
| 5 | Plugin nativo (doc 21 F3) | carga/descarga sin residuos; GUI wizard | 3–5 d |
| 6 | QA: 2.660 archivos sin crash; navegación GUI; tamaño embedded | deploy embedded ≤ +2 MB; sin Python | 3–5 d |

**MVP (pasos 1–4): 10–15 jornadas.**
**Completo (1–6): 3–4 semanas.**

### 7.1 Gates de aceptación

- Gramática: ≥95 % de archivos de `core` + `base` con cero errores.
- Indexador: 2.660 archivos de Odin sin crash; tiempo lineal; memoria acotada.
- Navegación: en un sample con 2 paquetes, ir a definición de struct, proc, campo,
  enum y call; `using` muestra herencia.
- Base: la DB abre en la GUI y en `sourcetrail_cli`; counts estables entre corridas.
- Embedded: sin Python; incremento de deploy ≤2 MB; `sourcetrail_cli` sigue <2 MB.
- Sin floods de “unsolved”: <20 % de referencias sin resolver en el sample.

---

## 8. Riesgos

| Riesgo | Impacto | Mitigación |
|---|---|---|
| Odin sigue evolucionando | la gramática se vuelve a atrasar | tests de coverage en CI con `core` actualizado; fork versionado |
| Gramática tree-sitter incompleta | nodos de error, indexado parcial | fallback léxico para declaraciones; no abortar |
| Resolución semántica (genéricos, `when`, `using`) | enlaces incorrectos | MVP aproximado; no prometer precisión de compilador |
| Cross-file en modelo por archivo | referencias sin resolver | caché de tabla de paquete; migrar a nativo |
| ABI del plugin | crash al cargar/descargar | C API v1 + `unloadable`; probar con ASan |
| Tamaño del parser | +1–2 MB | medir; el perfil embedded ya es de 41,3 MB |
| Python en el target | dependencia extra | opción C++ recomendada |

---

## 9. Decisión

La recomendación es:

1. **Forkear/actualizar `tree-sitter-odin`** con el EBNF oficial; no usar la gramática
   2023 tal cual y no escribir un parser desde cero.
2. **Indexador propio en C++** (externo primero), con mapeo explícito a
   `NodeKind`/`ReferenceKind`; sin depender de otro proyecto de Sourcetrail.
3. **Integrarlo como plugin** cuando el host del doc 21 esté disponible.

Con eso, Odin se agrega sin tocar el núcleo y el mismo camino sirve para el próximo
lenguaje. La alternativa de empezar por el plugin nativo sin actualizar la gramática
entrega una demo rápida pero no un soporte utilizable: los errores de parseo afectan a
la mayoría de los archivos reales.

---

## 10. Nota posterior: PoC ejecutado (2026-10-19)

El PoC del documento 23 se hizo con un **scanner propio en Python** (lexer +
reconocimiento de declaraciones), no con la gramática tree-sitter, justamente porque
ésta falla en la mayoría de los archivos. El scanner validó la integración con
Sourcetrail y la navegación en el sample, `core/strings` y `core/bufio` + `core/fmt`,
sin errores. Eso **no cierra** la decisión del parser final: actualizar tree-sitter con
`odin.ebnf` sigue siendo la opción de menor riesgo para el plugin nativo, y el scanner
sirve como referencia y como fallback léxico.

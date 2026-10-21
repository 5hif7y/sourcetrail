---
name: sourcetrail
description: Relaciones del código C y C++ desde una base .srctrldb de Sourcetrail (indexada con
  Clang, con tipos). Usar para preguntas de relación entre símbolos que pedirían recorrer muchos
  archivos con Grep - quién llama a una función, a quién llama, quién usa un campo o un tipo, quién
  hereda o sobrescribe, qué miembros tiene una clase - y para consultar la base con SQL. También
  para armar la base de un proyecto desde su compile_commands.json (CMake, Ninja, Bear, Unreal sin
  UBT). No usar para ubicar una declaración, buscar texto ni entender un flujo: ahí Grep y Read.
---

# Sourcetrail

Una base `.srctrldb` es una SQLite con los símbolos y las relaciones que Clang resolvió. Esta skill
trae los dos scripts para usarla en cualquier proyecto, en Git Bash:

- `scripts/srctrl <base> <consulta> [nombre]`: consultas de relación ya hechas, con los nombres de
  Sourcetrail decodificados. Usa `SRCTRL_SQLITE`, un `sqlite3.exe` copiado al lado del script o el `sqlite3` del PATH.
- `scripts/srctrl-index <compile_commands.json> <carpeta> <nombre>`: escribe el `.srctrlprj` y corre
  el CLI de Sourcetrail. El CLI sale de `SOURCETRAIL_CLI` o del PATH (`sourcetrail_cli`):
  `<build>/Release/app/sourcetrail_cli.exe` al compilar este repo ([compilar](references/compilar.md)).

`<skill>` abajo es la carpeta de esta skill (`~/.claude/skills/sourcetrail`).

## Flujo

1. **¿Hay base?** Buscar un `*.srctrldb` del proyecto. Si no hay, armarla
   ([armar la base](references/indexar.md)); para Unreal Engine, [unreal](references/unreal.md).
2. **Comprobarla:** `<skill>/scripts/srctrl <base> stats` da archivos, nodos, aristas por tipo y
   errores de Clang. Muchos errores fatales o pocas aristas `call` = la base está incompleta.
3. **Una consulta por relación**, con `--limit` si la lista puede ser larga y `--in <ruta>` para
   quedarse con el código propio. La fila trae símbolo y `archivo:línea`: no abrir el archivo
   para confirmarla.
4. **Lo que no es relación** (dónde está algo, qué hace): Grep y Read.

## Consultas

```sh
<skill>/scripts/srctrl <base> stats
<skill>/scripts/srctrl <base> find <patrón> [--limit N] [--in <ruta>]
<skill>/scripts/srctrl <base> callers <nombre> [--limit N] [--in <ruta>]
<skill>/scripts/srctrl <base> callees <nombre> [--limit N] [--in <ruta>]
<skill>/scripts/srctrl <base> refs <nombre> [--limit N] [--in <ruta>]
<skill>/scripts/srctrl <base> members <nombre> [--limit N]
<skill>/scripts/srctrl <base> sql "<SQL>"
```

- `<nombre>`: el nombre calificado sin firma (`Box::grow`) o su final (`grow`); coincide exacto o
  como `...::<nombre>`. Un final corto trae homónimos: la columna del nombre calificado los separa.
- `find`: por subcadena; da la definición (el cuerpo si existe, si no el token).
- `callers` / `callees`: una fila por sitio de llamada, con el `archivo:línea` de la llamada.
- `refs`: llamadas, usos, uso como tipo, herencia y override (no `member` ni `include`).
- `members`: campos y métodos de una clase, struct o namespace.
- `--in <ruta>`: solo el otro extremo definido en un archivo cuya ruta contiene `<ruta>`. Saca la
  matemática inline, la STL y las macros de los `callees`.
- `sql`: SQL libre con las vistas `sym(id, kind, qname, sig)`, `edge_kind` y `site`
  ([esquema](references/esquema.md)).
- Salida con columnas separadas por ` | `, cortada en 50 filas sin `--limit`.

## Cómo leer los resultados

- Una función que "se llama a sí misma" en la línea de una macro es la expansión, no recursión.
- Un llamador en una unidad que no se indexó no aparece: confirmar con Grep antes de afirmar que
  algo no tiene llamadores.
- Las funciones `static` llevan el archivo en la firma (`helper(Box &) (a.cpp)`); cada `main`
  queda como `main`.
- Los encabezados fuera de `--headers` se parsean pero no se guardan: sus símbolos aparecen solo
  como extremos de relaciones del código indexado.

## Mapa

- Armar una base desde `compile_commands.json`, y cómo obtenerlo en cada sistema de build:
  [indexar](references/indexar.md).
- Unreal Engine sin UBT ni UHT: [unreal](references/unreal.md).
- Tablas, tipos de nodo y arista, formato de los nombres, SQL de ejemplo: [esquema](references/esquema.md).
- Compilar el fork rápido (LLVM `/MD` de la caché de vcpkg): [compilar](references/compilar.md).

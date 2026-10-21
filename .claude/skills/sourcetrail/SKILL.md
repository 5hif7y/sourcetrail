---
name: sourcetrail
description: Relaciones del código desde una base .srctrldb de Sourcetrail, sea cual sea el
  lenguaje que se indexó (C, C++, Python, Odin, Java). Usar para preguntas de relación entre
  símbolos que pedirían recorrer muchos archivos con Grep - quién llama a una función, a quién
  llama, quién usa un campo o un tipo, quién hereda o sobrescribe, qué miembros tiene una clase -
  y para consultar la base con SQL. Si el proyecto no tiene base, armarla con el indexador de
  Sourcetrail. No usar para ubicar una declaración, buscar texto ni entender un flujo: ahí Grep y
  Read.
---

# Sourcetrail

Dos responsabilidades separadas:

- **El indexador** (el CLI de Sourcetrail) resuelve el lenguaje: parsea el código y escribe una
  base `.srctrldb`. Qué lenguajes hay depende de cómo se compiló el fork (C y C++ con Clang,
  Python, Odin, Java).
- **Esta skill** consulta la base. El esquema es el mismo para todos los lenguajes, así que las
  consultas no saben de qué lenguaje vino cada símbolo: solo leen nodos, aristas y ubicaciones.

`<skill>` abajo es la carpeta de esta skill (`~/.claude/skills/sourcetrail`). Scripts en Git Bash:

- `scripts/srctrl <base> <consulta> [nombre]`: las consultas de relación, con los nombres
  decodificados. Usa `SRCTRL_SQLITE`, un `sqlite3.exe` copiado al lado o el `sqlite3` del PATH.
- `scripts/srctrl-index`: un atajo para correr el indexador sin la interfaz; escribe el
  `.srctrlprj` y llama al CLI, que sale de `SOURCETRAIL_CLI` o del PATH
  (`<build>/Release/app/sourcetrail_cli.exe` al compilar este repo; [armar la base](references/indexar.md)).

## Flujo

1. **¿Hay base?** Buscar un `*.srctrldb` del proyecto. Si no hay, armarla
   ([indexar](references/indexar.md); Unreal Engine: [unreal](references/unreal.md)).
2. **Comprobarla:** `<skill>/scripts/srctrl <base> stats` da archivos, nodos, aristas por tipo y
   errores del indexador. Muchos errores fatales o pocas aristas `call` = base incompleta.
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

- `<nombre>`: el nombre calificado sin firma o su final. Los niveles se unen con el separador que
  el indexador guardó para ese lenguaje: `Box::grow` en C++, `app.Box.grow` en Python,
  `demo.grow` en Odin. Coincide exacto y con mayúsculas (`grow` no trae `Grow`), entero o como
  final `...::<nombre>` o `....<nombre>`; un final corto trae homónimos, que la columna del nombre
  calificado separa.
- `find`: por subcadena; da la definición (el cuerpo si existe, si no el token).
- `callers` / `callees`: una fila por sitio de llamada, con el `archivo:línea` de la llamada.
- `refs`: toda arista que apunta al símbolo menos `member`, `include` e `import`: llamadas, usos,
  uso como tipo, herencia y override, y también argumento de plantilla, especialización de
  plantilla, uso de macro y de anotación. La primera columna dice el tipo de cada fila.
- `members`: campos y métodos de una clase, struct, módulo o namespace.
- `--in <ruta>`: solo el otro extremo definido en un archivo cuya ruta contiene `<ruta>`. Saca la
  biblioteca estándar, las funciones inline y las macros de los `callees`.
- `sql`: SQL libre con la tabla `sym(id, kind, qname, sig)` y las vistas `edge_kind` y `site`
  ([esquema](references/esquema.md)).
- Salida con columnas separadas por ` | `, cortada en 50 filas sin `--limit`.

## Cómo leer los resultados

- Lo que el indexador no resolvió no está: en lenguajes dinámicos (Python) una llamada a través
  de una variable sin tipo conocido puede faltar, y un llamador en un archivo no indexado tampoco
  aparece. Confirmar con Grep antes de afirmar que algo no tiene llamadores.
- Una función que "se llama a sí misma" en la línea de una macro es la expansión, no recursión.
- En C y C++ las funciones `static` llevan el archivo en la firma (`helper(Box &) (a.cpp)`) y
  cada `main` queda como `main`.
- Las plantillas de C++ llevan sus parámetros en el nombre
  (`TIdBasedArray<typename IndexType, typename ItemType, typename Allocator>::operator[]`), y cada
  instanciación es otro símbolo con los argumentos concretos. `TIdBasedArray::operator[]` no
  coincide con ninguno: consultar por el final (`operator[]`), que trae la plantilla y sus
  instanciaciones, o copiar el nombre completo de `find`.

## Mapa

- Armar una base por lenguaje, y cómo obtener un `compile_commands.json`: [indexar](references/indexar.md).
- Unreal Engine sin UBT ni UHT: [unreal](references/unreal.md).
- Tablas, tipos de nodo y arista, formato de los nombres, SQL de ejemplo: [esquema](references/esquema.md).
- Compilar el fork rápido (LLVM `/MD` de la caché de vcpkg): [compilar](references/compilar.md).

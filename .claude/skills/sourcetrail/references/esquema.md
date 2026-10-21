# Esquema de la .srctrldb


Tablas de Sourcetrail, verificadas en `src/lib/data/storage/sqlite/SqliteIndexStorage.cpp`:

| Tabla | Columnas | Qué es |
| --- | --- | --- |
| `node` | `id, type, serialized_name` | símbolos y archivos |
| `edge` | `id, type, source_node_id, target_node_id` | relaciones |
| `symbol` | `id, definition_kind` | 0 ninguna, 1 implícita, 2 explícita |
| `file` | `id, path, language, modification_time, indexed, complete, line_count` | `path` absoluto |
| `source_location` | `id, file_node_id, start_line, start_column, end_line, end_column, type` | rangos |
| `occurrence` | `element_id, source_location_id` | qué nodo o arista ocurre dónde |
| `error` | `id, message, fatal, indexed, translation_unit` | diagnósticos de Clang |
| `filecontent`, `local_symbol`, `component_access`, `meta`, `element`, `element_component` | | |

- `edge.type` (bits de `src/lib/data/graph/Edge.h`): 1 member, 2 type_usage, 4 usage, 8 call,
  16 inheritance, 32 override, 64 type_argument, 128 template_specialization, 256 include,
  512 import, 1024 bundled, 2048 macro_usage, 4096 annotation_usage.
- `node.type` (bits de `src/lib/data/NodeKind.h`): 1 symbol, 2 type, 4 builtin, 8 module,
  16 namespace, 32 package, 64 struct, 128 class, 256 interface, 512 annotation, 1024 global,
  2048 field, 4096 function, 8192 method, 16384 enum, 32768 enum_constant, 65536 typedef,
  131072 type_param, 262144 file, 524288 macro, 1048576 union, 2097152 record, 4194304 concept,
  8388608 c_cast.
- `source_location.type` (`src/lib/data/location/LocationType.h`): 0 token, 1 scope,
  2 qualifier, 3 local_symbol, 4 signature, 5 comment, 6 error, 9 unsolved.
- `serialized_name` (`src/lib/data/name/NameHierarchy.cpp`): `<delim>\tm` y por cada nivel
  `<nombre>\ts<prefijo>\tp<postfijo>`, niveles unidos por `\tn`. `<delim>` es el separador del
  lenguaje: `::` en C++, `.` en Python, Java y Odin. El prefijo es el tipo de retorno y el
  postfijo la lista de parámetros; un `main` de C/C++ viene como `.:main:.<archivo>`. `srctrl` lo
  traduce en la tabla temporal `sym(id, kind, qname, sig)`, disponible también en `sql`: `qname`
  une los nombres de los niveles con ese separador, sin firmas, y `sig` es el postfijo del último.
  Los niveles intermedios también pueden tener firma (una clase local o un lambda dentro de una
  función: `Render::lambda at 120:8`); `qname` la omite.
- El sitio de una relación es la `occurrence` de la arista: `occurrence.element_id = edge.id`.

Ejemplo de SQL libre, quién usa un campo:

```sh
<skill>/scripts/srctrl <base> sql "SELECT s.qname, f.path, l.start_line FROM edge e
  JOIN sym t ON t.id=e.target_node_id JOIN sym s ON s.id=e.source_node_id
  JOIN occurrence o ON o.element_id=e.id JOIN source_location l ON l.id=o.source_location_id
  JOIN file f ON f.id=l.file_node_id WHERE e.type=4 AND t.qname LIKE '%::MaxFramesAccumulated'"
```


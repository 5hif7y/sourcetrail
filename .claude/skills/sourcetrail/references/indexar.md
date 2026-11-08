# Armar la base

El lenguaje lo resuelve el indexador de Sourcetrail. `srctrl-index` solo escribe el `.srctrlprj`
y corre el CLI (`sourcetrail_cli --refresh=all`); lo mismo se puede hacer desde la interfaz de
Sourcetrail, que además arma proyectos Java (Maven, Gradle). La base sale con el mismo esquema
para cualquier lenguaje.

| Lenguaje | Qué necesita el indexador | Comando |
| --- | --- | --- |
| C y C++ | un `compile_commands.json` (los flags de cada archivo) | `srctrl-index cdb <json> <carpeta> <nombre> --headers <src>` |
| Python | la carpeta de fuentes | `srctrl-index src python <fuentes> <carpeta> <nombre>` |
| Odin | la carpeta de fuentes | `srctrl-index src odin <fuentes> <carpeta> <nombre>` |
| Java | un proyecto Maven o Gradle | desde la interfaz de Sourcetrail |

Probado (2026-10-21) con el CLI de este repo: los tres comandos dan
`callers` correctos sobre un proyecto chico de cada lenguaje.

- La base queda en `<carpeta>/<nombre>.srctrldb`. Agregar la carpeta al `.gitignore`.
- `--threads N`: hilos del indexador (4 por defecto).
- Reindexar solo lo cambiado: `sourcetrail_cli --refresh=updated --threads=4 <carpeta>/<nombre>.srctrlprj`.

## C y C++: el `compile_commands.json`

Sourcetrail indexa C y C++ con Clang, con los mismos flags con los que se compila cada archivo:

| Build | Cómo |
| --- | --- |
| CMake (Ninja o Makefiles) | `cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; queda en `build/` |
| CMake con Visual Studio | el generador VS no lo exporta: configurar aparte con `-G Ninja` desde un "Developer Command Prompt" |
| Meson | `build/compile_commands.json`, siempre |
| Make u otro | `bear -- make` (Linux/MSYS2) o `compiledb make` |
| MSBuild (`.vcxproj`) | la extensión "Clang Power Tools" |
| Unreal Engine | `scripts/unreal_cdb.py` ([unreal](unreal.md)) |

- No hace falta compilar: alcanza con el JSON y los encabezados generados (protobuf, Qt `moc`,
  `config.h`) en su lugar. Correr antes el paso de generación del build si lo hay.
- `--headers <ruta>` (obligatorio, repetible): las carpetas de encabezados cuyos símbolos se
  guardan. Poner solo el código propio; los de terceros se parsean igual. No hay valor por
  defecto: la carpeta del JSON suele ser `build/`, y la base saldría sin los símbolos de los `.h`.
- Para un subconjunto, filtrar el JSON (una lista de `{directory, command|arguments, file}`):
  indexar todo un motor puede tardar horas. Medida: 66 unidades del renderer de Unreal en 14 min
  con 4 hilos, base de 82 MB.

## Comprobar

```sh
<skill>/scripts/srctrl <carpeta>/<nombre>.srctrldb stats
<skill>/scripts/srctrl <carpeta>/<nombre>.srctrldb sql "SELECT message, translation_unit FROM error WHERE fatal = 1 LIMIT 20"
```

- `srctrl-index` falla con "la base no tiene archivos indexados" si el CLI dejó una base vacía
  (necesita `sqlite3` para contarlos). Con `0/N source files indexed` y "Cannot start indexer
  process because executable is missing" en el log, falta `Sourcetrail_indexer.exe` junto al CLI
  ([compilar](compilar.md)). Con `0/0`, el proyecto no tenía fuentes o no se pudo leer.
- Errores fatales de includes en C/C++: falta un `-I` o un encabezado generado. Las relaciones de
  las unidades que sí compilaron quedan igual.

# Armar la base

## 1. Un `compile_commands.json`

Sourcetrail indexa C y C++ con los mismos flags con los que se compila cada archivo. Cómo
obtenerlos:

| Build | Cómo |
| --- | --- |
| CMake (Ninja o Makefiles) | `cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`; queda en `build/` |
| CMake con Visual Studio | el generador VS no lo exporta: configurar aparte con `-G Ninja` desde un "Developer Command Prompt" |
| Meson | `build/compile_commands.json`, siempre |
| Make u otro | `bear -- make` (Linux/MSYS2) o `compiledb make` |
| MSBuild (`.vcxproj`) | la extensión "Clang Power Tools" o `msbuild -t:ClangCompileCommands` si está configurado |
| Unreal Engine | `scripts/unreal_cdb.py` ([unreal](unreal.md)) |

No hace falta compilar: alcanza con que exista el JSON y los encabezados generados (protobuf,
Qt `moc`, `config.h`) estén en su lugar. Correr antes el paso de generación del build si los hay.

Para un subconjunto, filtrar el JSON (es una lista de `{directory, command|arguments, file}`)
antes de indexar: indexar todo un motor puede tardar horas.

## 2. Indexar

```sh
<skill>/scripts/srctrl-index build/compile_commands.json .srctrl proyecto --headers src --threads 4
```

- Escribe `.srctrl/proyecto.srctrlprj` y corre `sourcetrail_cli --refresh=all`. La base queda en
  `.srctrl/proyecto.srctrldb`.
- `--headers <ruta>` (repetible): los encabezados cuyos símbolos se guardan. Poner solo el código
  propio; los de terceros se parsean igual. Sin `--headers`, la carpeta del JSON.
- Agregar `.srctrl/` al `.gitignore` del proyecto.
- Medida de referencia: 66 unidades del renderer de Unreal en 14 min con 4 hilos, base de 82 MB.

Para reindexar solo lo cambiado:
`sourcetrail_cli --refresh=updated --threads=4 .srctrl/proyecto.srctrlprj`.

## 3. Comprobar

```sh
<skill>/scripts/srctrl .srctrl/proyecto.srctrldb stats
<skill>/scripts/srctrl .srctrl/proyecto.srctrldb sql "SELECT message, translation_unit FROM error WHERE fatal = 1 LIMIT 20"
```

- `0/N source files indexed` y "Cannot start indexer process because executable is missing" en
  el log: falta `Sourcetrail_indexer.exe` junto al CLI ([compilar](compilar.md)).
- Errores fatales de includes: falta un `-I` o un encabezado generado. Las relaciones de las
  unidades que sí compilaron quedan igual.

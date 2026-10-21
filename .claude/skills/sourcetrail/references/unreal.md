# Unreal Engine sin UBT ni UHT

Unreal no trae `compile_commands.json` sin compilar el motor con UBT. `scripts/unreal_cdb.py` lo
arma para un subconjunto del renderer sin compilar nada:

- junta los `Public`, `Classes` e `Internal` de cada módulo (carpetas con `*.Build.cs`) como `-I`;
- define vacíos los `<MÓDULO>_API`;
- reemplaza los `.generated.h` de UHT por stubs que anulan `GENERATED_BODY` y compañía;
- escribe también un `.srctrlprj` con los encabezados del renderer como indexados.

```sh
py -3 -I <skill>/scripts/unreal_cdb.py <UnrealEngine> <salida> Lumen MegaLights IndirectLightRendering.cpp
<skill>/scripts/srctrl-index cdb <salida>/compile_commands.json <salida> ue-renderer \
  --headers <UnrealEngine>/Engine/Source/Runtime/Renderer --threads 4
```

Los argumentos después de `<salida>` son carpetas o archivos relativos a `Renderer/Private`.

## Qué sale y qué no

- Las relaciones del renderer salen completas (quién arma cada pasada, quién la elige).
- Las clases `UObject` pierden lo que genera UHT (`Super`, `StaticClass`): esos errores caen en
  encabezados de `CoreUObject` y `Engine` y quedan en la tabla `error`, sin cortar el renderer.
- Los shaders (`.usf`, `.ush`) no se indexan: Grep y Read.
- `callees` sin filtro trae cientos de filas de matemática inline y STL; con
  `--in Renderer/Private` queda la tubería de pasadas.
- Una "recursión" en la línea de un `RDG_EVENT_SCOPE_STAT` o un `ensureMsgf` es la macro.

Medido (Unreal 6.0, renderer GI): 66 unidades en 14 min con 4 hilos, base de 82 MB.

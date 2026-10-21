"""Genera un compile_commands.json y un .srctrlprj para indexar con Sourcetrail un subconjunto
del renderer de Unreal Engine sin compilar el motor (sin UBT ni UHT).

UBT no se corre: las rutas de include salen de recorrer los módulos (carpetas con *.Build.cs) y
los .generated.h de UHT se reemplazan por stubs que anulan GENERATED_BODY y compañía. El índice
resultante sirve para relaciones (quién llama, a quién llama, quién usa); los errores de Clang
por lo que UHT no generó quedan en la tabla error de la base.

Uso: py -3 -I unreal_cdb.py <UnrealEngine> <salida> [carpeta-o-archivo relativo a Renderer/Private ...]
"""

import json
import os
import re
import sys

STUB_BODY = """#pragma once
// Stub: UHT no se corre al indexar con Sourcetrail.
#undef GENERATED_BODY
#undef GENERATED_BODY_LEGACY
#undef GENERATED_UCLASS_BODY
#undef GENERATED_USTRUCT_BODY
#undef GENERATED_UINTERFACE_BODY
#undef GENERATED_IINTERFACE_BODY
#undef UCLASS
#undef UINTERFACE
#define UCLASS(...)
#define UINTERFACE(...)
#define GENERATED_BODY(...) public: static class UClass* StaticClass();     static class UScriptStruct* StaticStruct();
#define GENERATED_BODY_LEGACY(...) GENERATED_BODY()
#define GENERATED_UCLASS_BODY(...) GENERATED_BODY()
#define GENERATED_USTRUCT_BODY(...) GENERATED_BODY()
#define GENERATED_UINTERFACE_BODY(...) GENERATED_BODY()
#define GENERATED_IINTERFACE_BODY(...) GENERATED_BODY()
"""

DEFINES = [
    "UE_BUILD_DEVELOPMENT=1", "UE_GAME=1", "WITH_EDITOR=0", "WITH_EDITORONLY_DATA=0",
    "WITH_ENGINE=1", "WITH_UNREAL_DEVELOPER_TOOLS=0", "WITH_PLUGIN_SUPPORT=0",
    "IS_MONOLITHIC=1", "IS_PROGRAM=0", "WITH_SERVER_CODE=1", "WITH_TESTS=0",
    "WITH_LOW_LEVEL_TESTS=0", "EXPLICIT_TESTS_TARGET=0", "UE_IS_ENGINE_MODULE=1",
    "UBT_COMPILED_PLATFORM=Windows", "UBT_COMPILED_TARGET=Game", "PLATFORM_WINDOWS=1",
    "PLATFORM_MICROSOFT=1", "OVERRIDE_PLATFORM_HEADER_NAME=Windows", "WIN32=1", "_WIN64=1",
    "_WIN32_WINNT=0x0A00", "WINVER=0x0A00", "UNICODE", "_UNICODE", "UE_ENABLE_ICU=1",
    "WITH_VERSE_VM=0", "UE_MODULE_NAME=\"Renderer\"", "UE_PLUGIN_NAME=\"\"",
    "UE_PROJECT_NAME=", "UE_TARGET_NAME=", "WITH_LIVE_CODING=0", "UE_TRACE_ENABLED=0",
    "WITH_STATE_STREAM=0", "UE_MERGED_MODULES=0",
]


def modules(runtime):
    for root, dirs, files in os.walk(runtime):
        for f in files:
            if f.endswith(".Build.cs"):
                yield f[: -len(".Build.cs")], root
        dirs[:] = [d for d in dirs if d not in ("Private", "Public", "Classes", "Internal")
                   or root == runtime]


def main():
    ue, out = sys.argv[1], sys.argv[2]
    targets = sys.argv[3:] or ["Lumen"]
    source = os.path.join(ue, "Engine", "Source")
    renderer_private = os.path.join(source, "Runtime", "Renderer", "Private")

    includes, defines = [], list(DEFINES)
    for top in ("Runtime", "Developer"):
        for name, path in modules(os.path.join(source, top)):
            for sub in ("Public", "Classes", "Internal"):
                p = os.path.join(path, sub)
                if os.path.isdir(p):
                    includes.append(p)
            api = name.upper()
            defines += [f"{api}_API=", f"{api}_VTABLE=", f"{api}_NON_ATTRIBUTED_API="]
    for extra in (renderer_private, source, os.path.join(ue, "Engine", "Shaders", "Shared"),
                  os.path.join(ue, "Engine", "Shaders")):
        includes.append(extra)

    stubs = os.path.join(out, "stubs")
    os.makedirs(stubs, exist_ok=True)
    gen = re.compile(rb'#include\s+"([A-Za-z0-9_]+\.generated\.h)"')
    names = set()
    for top in ("Runtime", "Developer"):
        for root, _, files in os.walk(os.path.join(source, top)):
            for f in files:
                if f.endswith(".h"):
                    with open(os.path.join(root, f), "rb") as fh:
                        names.update(m.decode() for m in gen.findall(fh.read()))
    for n in names:
        with open(os.path.join(stubs, n), "w", newline="\n") as fh:
            fh.write(STUB_BODY)
    includes.insert(0, stubs)

    files = []
    for t in targets:
        p = os.path.join(renderer_private, t)
        if os.path.isdir(p):
            files += [os.path.join(p, f) for f in sorted(os.listdir(p)) if f.endswith(".cpp")]
        elif os.path.isfile(p):
            files.append(p)

    rsp = os.path.join(out, "flags.rsp")
    with open(rsp, "w", newline="\n") as fh:
        for d in defines:
            fh.write(f'-D{d}\n'.replace("\\", "/"))
        for i in includes:
            fh.write(f'-I"{i}"\n'.replace("\\", "/"))

    base = ["clang++", "--target=x86_64-pc-windows-msvc", "-std=c++20", "-fms-extensions",
            "-fms-compatibility", "-fms-compatibility-version=19.44", "-fsyntax-only",
            "-Wno-everything", "-include", os.path.join(renderer_private, "RendererPrivatePCH.h")]
    base += [f"-D{d}" for d in defines] + [f"-I{i}" for i in includes]
    cdb = [{"directory": renderer_private, "file": f, "arguments": base + [f]} for f in files]
    with open(os.path.join(out, "compile_commands.json"), "w") as fh:
        json.dump(cdb, fh, indent=1)

    # Solo se indexan los encabezados del renderer: Core, Engine y RHI se parsean pero no se
    # guardan, lo que mantiene chica la base y rápido el indexado.
    renderer = os.path.join(source, "Runtime", "Renderer").replace("\\", "/")
    prj = f"""<?xml version="1.0" encoding="utf-8" ?>
<config>
    <source_groups>
        <source_group_5f0c1a2e-6b1d-4c55-9d0e-0a1e2f3b4c5d>
            <build_file_path>
                <compilation_db_path>./compile_commands.json</compilation_db_path>
            </build_file_path>
            <indexed_header_paths>
                <indexed_header_path>{renderer}</indexed_header_path>
            </indexed_header_paths>
            <name>Unreal Renderer GI</name>
            <pch_flags>
                <use_compiler_flags>0</use_compiler_flags>
            </pch_flags>
            <status>enabled</status>
            <type>C/C++ from Compilation Database</type>
        </source_group_5f0c1a2e-6b1d-4c55-9d0e-0a1e2f3b4c5d>
    </source_groups>
    <version>8</version>
</config>
"""
    with open(os.path.join(out, "ue-gi.srctrlprj"), "w", newline="\n") as fh:
        fh.write(prj)

    print(f"{len(files)} archivos, {len(includes)} includes, {len(defines)} defines, "
          f"{len(names)} stubs")


if __name__ == "__main__":
    main()

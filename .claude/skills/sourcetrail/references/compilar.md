# Compilar el fork rápido

Fork: rama `modernized` de `5hif7y/sourcetrail`.


El indexador C++ necesita un LLVM/Clang 21 con CRT dinámica (`/MD`), igual que el resto del fork.
El LLVM oficial de Windows es `/MT` y hace caer el CLI al indexar (destructores de `locale` entre
DLLs con CRT distintas). La caché binaria de vcpkg ya tiene el LLVM correcto: el paquete
`llvm:x64-windows@21.1.2` (con el overlay del fork, `ONLY_STATIC_LIBRARY` y `ONLY_DYNAMIC_CRT`).
Compilarlo de cero son horas; extraerlo de la caché, minutos. Cambiar de triplet cambia el ABI de
vcpkg y obliga a recompilarlo.

| Paso | Tiempo medido (2026-10-20) |
| --- | --- |
| Extraer el zip de LLVM de `%LOCALAPPDATA%\vcpkg\archives` (sin `debug/`) | 7,4 min |
| Configurar con dependencias `x64-windows-release` ya instaladas | 40 s |
| `sourcetrail_cli` + plugin C++ | 205 s |
| `Sourcetrail_indexer` (proceso hijo que indexa) | 7 s |

Configuración usada (Git Bash, con `MSYS_NO_PATHCONV=1`):

```sh
cmake -S <fork> -B <build> -G "Visual Studio 18 2026" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_MANIFEST_MODE=OFF \
  -DVCPKG_TARGET_TRIPLET=x64-windows-release -DBUILD_CXX_LANGUAGE_PACKAGE=ON \
  -DLLVM_DIR=<llvm>/share/llvm -DClang_DIR=<llvm>/lib/cmake/clang
cmake --build <build> --config Release --target sourcetrail_cli Sourcetrail_lib_cxx Sourcetrail_indexer --parallel
```

Sin `Sourcetrail_indexer` el CLI termina con `0/N source files indexed` y el error
`Cannot start indexer process because executable is missing` en el log.

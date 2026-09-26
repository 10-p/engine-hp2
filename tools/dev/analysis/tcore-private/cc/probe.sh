#!/bin/bash
# probe.sh <gcc|wasm> file.cpp  -> errors
W=$(dirname $0); cd $W
DEFS="-DUNREAL_STATIC -DPLATFORM_LINUX -DPLATFORM_SDL -DCORE_API= -DCORE_EXPORTS -DUPACKAGE_NAME=Core -I Inc -I Src"
case $1 in
 gcc) g++ -m32 -std=gnu++17 -fsyntax-only -fno-strict-aliasing -Wno-write-strings -w $DEFS "$2" ;;
 wasm) /home/ion/emsdk/upstream/bin/clang++ --target=wasm32-unknown-emscripten -std=gnu++17 -fsyntax-only -D__EMSCRIPTEN__ -fpack-struct=4 -Wno-everything -Wnon-pod-varargs $DEFS -isystem /home/ion/emsdk/upstream/emscripten/cache/sysroot/include/c++/v1 -isystem /home/ion/emsdk/upstream/emscripten/cache/sysroot/include/compat -isystem /home/ion/emsdk/upstream/emscripten/cache/sysroot/include "$2" ;;
esac 2>&1

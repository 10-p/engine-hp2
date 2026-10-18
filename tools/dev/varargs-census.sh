#!/usr/bin/env bash
# varargs-census.sh <Pkg>... -- syntax-check each package's sources with em++ and -Wnon-pod-varargs on (the
# build silences it, and clang compiles an FString passed through ... into a runtime trap). Prints one line
# per hit: file:line: message. Uses build-wasm's per-target flags, so configure the wasm preset first.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
B="$REPO/build-wasm"
source ~/emsdk/emsdk_env.sh >/dev/null 2>&1
for P in "$@"; do
  D="$B/$P/CMakeFiles/$P.dir"
  [ -f "$D/flags.make" ] || { echo "no $D/flags.make" >&2; continue; }
  DEFS=$(sed -n 's/^CXX_DEFINES = //p' "$D/flags.make")
  FLAGS=$(sed -n 's/^CXX_FLAGS = //p' "$D/flags.make" | sed 's/-Wno-error=non-pod-varargs//; s/-Wno-non-pod-varargs//')
  INCS="@$D/includes_CXX.rsp"
  for SRC in $(grep -o '"Src/[^"]*\.cpp"\|"[A-Za-z]*\.cpp"' "$REPO/Source/$P/CMakeLists.txt" | tr -d '"'); do
    ( cd "$B/$P" && em++ $DEFS $INCS $FLAGS -fsyntax-only -Wno-everything -Wnon-pod-varargs \
        "$REPO/Source/$P/$SRC" 2>&1 ) | grep -E "warning: cannot pass|error:" | sed "s|$REPO/Source/||"
  done
done | sort -u

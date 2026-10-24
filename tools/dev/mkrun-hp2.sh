#!/usr/bin/env bash
# mkrun-hp2.sh -- assemble a private HP2 run tree for a native (or --windows) build, with run-hp2.sh as run.sh.
#
#   tools/dev/mkrun-hp2.sh <build-dir> <run-dir> [--windows]      (game content: $UE_ASSETS_DIR, default Assets/)
#
# The game content (never in git) is only read: big read-only trees are symlinked, System/ is a real copy
# because the engine writes Game.ini/User.ini/Game.log into it. The retail Win32 binaries (*.dll *.exe *.bat)
# are not copied: on Windows a retail OpenAL32.dll/Game.exe would shadow ours.
#
# --remove-destination on every copy is load-bearing: a plain `cp` onto a symlink follows the link and
# writes straight through into the content tree.
set -eu

BUILD="${1:?usage: mkrun-hp2.sh <build-dir> <run-dir> [--windows]}"
RUN="${2:?}"
WINDOWS=0; [ "${3:-}" = "--windows" ] && WINDOWS=1
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
A="${UE_ASSETS_DIR:-$REPO/Assets}"
[ -d "$A/System" ] || { echo "no retail tree at $A (set UE_ASSETS_DIR)"; exit 1; }

rm -rf "$RUN"
mkdir -p "$RUN"

# Read-only bulk: symlinked, never written.
for d in Maps Textures Sounds Music Fonts Help SystemLocalized; do
    [ -d "$A/$d" ] && ln -sfn "$A/$d" "$RUN/$d"
done

# System is a real copy (minus the retail Win32 binaries and build scripts), then our inis.
mkdir -p "$RUN/System"
( cd "$A/System" && find . -mindepth 1 -maxdepth 1 \
      ! -iname '*.dll' ! -iname '*.exe' ! -iname '*.bat' -exec cp -a --remove-destination {} "$RUN/System/" \; )
# The copy is ours: a read-only asset tree must not make the run tree read-only too, or the game cannot write
# its inis and log there and the next rebuild's rm -rf fails on its subfolders.
chmod -R u+w "$RUN/System"
cp -f --remove-destination "$REPO/Config/System/Default.ini" "$REPO/Config/System/DefUser.ini" "$RUN/System/"
mkdir -p "$RUN/Save"

# Our build last, so it wins over anything above.
if [ "$WINDOWS" = 1 ]; then
    cp -f --remove-destination "$BUILD/Game/Game.exe" "$RUN/System/"
    for dll in "$BUILD"/Game/*.dll; do [ -e "$dll" ] && cp -f --remove-destination "$dll" "$RUN/System/"; done
    [ -e "$BUILD/UCC/ucc.exe" ] && cp -f --remove-destination "$BUILD/UCC/ucc.exe" "$RUN/System/"
else
    cp -f --remove-destination "$BUILD/Game/Game" "$RUN/System/"
    chmod +x "$RUN/System/Game"
    [ -e "$BUILD/Game/libopenal.so.1" ] && cp -f --remove-destination "$BUILD/Game/libopenal.so.1" "$RUN/System/"
    [ -e "$BUILD/UCC/ucc-bin" ] && cp -f --remove-destination "$BUILD/UCC/ucc-bin" "$RUN/System/"
fi

# Launcher, so a rebuilt tree is immediately runnable.
cp -f --remove-destination "$(dirname "$0")/run-hp2.sh" "$RUN/run.sh"
chmod +x "$RUN/run.sh"

echo "$RUN ready from $BUILD"

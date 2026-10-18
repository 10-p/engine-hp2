#!/usr/bin/env bash
# mkrun-ut99.sh -- assemble a private UT99 run tree for an engine-ut99 native build (UnrealTournament + *.so
# modules), with run-template.sh as run.sh. This tree's own native build uses mkrun-hp2.sh.
#
#   tools/dev/mkrun-ut99.sh <build-dir> <run-dir>      (UT99 content: $UT99_ASSETS_DIR or $UT99_REF_RUNTREE)
#
# $UT99_ASSETS_DIR is a shared overlayfs, so nothing here may write into it. Big read-only trees are
# symlinked; System/ is a real copy because the engine rewrites inis and logs into it.
#
# --remove-destination on every copy is load-bearing: a plain `cp` onto a symlink follows the link and
# writes straight through into the shared asset tree.
set -eu

BUILD="${1:?usage: mkrun-ut99.sh <build-dir> <run-dir>}"
RUN="${2:?}"
# The reference run tree's plain asset copy is used when $UT99_ASSETS_DIR has no Maps/: the overlay is
# unmounted after a reboot, and symlinks into it then dangle ("Failed to load 'Entry'").
A="${UT99_ASSETS_DIR:-/home/ion/WorkUnreal/assets/ut99-assets}"
REF_ASSETS=${UT99_REF_RUNTREE:-/home/ion/WorkUnreal/native-runtrees/ut99}
[ -d "$REF_ASSETS/Maps" ] && [ ! -d "$A/Maps" ] && A="$REF_ASSETS"

rm -rf "$RUN"
mkdir -p "$RUN"

# Read-only bulk: symlinked, never written.
for d in Textures Sounds Music Help GLSetup Web UnrealEd; do
    [ -d "$A/$d" ] && ln -sfn "$A/$d" "$RUN/$d"
done

# Maps is a real directory of symlinks rather than one symlink to the tree, so locally-added test
# maps can sit alongside the stock ones without writing into shared assets.
mkdir -p "$RUN/Maps"
for m in "$A"/Maps/*; do [ -e "$m" ] && ln -sfn "$m" "$RUN/Maps/$(basename "$m")"; done
for extra in /home/ion/WorkUnreal/temp/*.unr; do
    [ -e "$extra" ] && cp -f --remove-destination "$extra" "$RUN/Maps/"
done

# System is a real copy -- the engine rewrites UnrealTournament.ini and the log.
cp -a "$A/System" "$RUN/System"
mkdir -p "$RUN/Logs"

# The retail ini names WinDrv.WindowsClient as the viewport manager, which a Linux build does not have:
# take the reference run tree's native config when it is there, otherwise patch the one line.
REF=${UT99_REF_RUNTREE:-/home/ion/WorkUnreal/native-runtrees/ut99}
if [ -f "$REF/System/UnrealTournament.ini" ]; then
    cp -f --remove-destination "$REF/System/UnrealTournament.ini" "$RUN/System/"
    [ -f "$REF/System/User.ini" ] && cp -f --remove-destination "$REF/System/User.ini" "$RUN/System/"
else
    sed -i -E 's#^ViewportManager=.*#ViewportManager=NSDLDrv.NSDLClient#' \
        "$RUN/System/UnrealTournament.ini"
fi

# Our build wins over whatever the asset tree happens to hold.
find "$BUILD" -name '*.so' -not -path '*_deps*' -exec \
    cp -f --remove-destination {} "$RUN/System/" \;
cp -f --remove-destination "$BUILD/UnrealTournament/UnrealTournament" "$RUN/System/"
chmod +x "$RUN/System/UnrealTournament"

# Launcher, so a rebuilt tree is immediately runnable.
cp -f --remove-destination "$(dirname "$0")/run-template.sh" "$RUN/run.sh"
chmod +x "$RUN/run.sh"

echo "$RUN ready: $(ls "$RUN"/System/*.so 2>/dev/null | wc -l) modules from $BUILD"

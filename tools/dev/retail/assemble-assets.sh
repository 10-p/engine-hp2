#!/usr/bin/env bash
# assemble-assets.sh -- build the RETAIL install tree (US English) from the retail disc image.
#
#   tools/dev/retail/assemble-assets.sh <HarryPotter2.iso> <work-dir> <out-dir> <unshield-binary>
#
# The disc (volume HPCOS, build HPCos_021009_1205-1, InstallShield cabs dated 2002-10-15) carries the game in
# setup/data1.cab + data2.cab, plus a LOOSE setup/hgame.u the installer copies over. Component -> folder, read off
# the cab contents (docs/porting/retail-assets.md):
#   Component_5  -> System/  (packages, .int, CUTSCENES/, Thumbnails/; retail *.dll/*.exe are copied but never
#                             used -- mkrun-hp2.sh skips them)      Component_6 = an identical copy of CUTSCENES/
#   Component_2  -> Maps/    Component_3 -> Music/    Component_4 -> Textures/ (*.utx only; *_utx are other languages)
#   Component_7  -> System/  (*.usa: the US-English localisation; Language=usa)
#   Component_8  -> Sounds/AllDialog.uax (AllDialog.USA_uax)  -- every other sound lives in System/HPSounds.u
#   Component_9/10/11/12 = Spanish/French text + dialog, Component_1 = installer UI: not installed here.
# Nothing here is ever committed; the out-dir lives outside every repo.
set -euo pipefail
ISO="${1:?iso}"; WORK="${2:?work dir}"; OUT="${3:?out dir}"; UNSHIELD="${4:?unshield binary}"
[ -e "$OUT" ] && { echo "refusing: $OUT exists (assemble into a fresh directory)"; exit 1; }
mkdir -p "$WORK"
[ -f "$WORK/setup/data1.cab" ] || 7z x -y -o"$WORK" "$ISO" setup/ >/dev/null
[ -d "$WORK/extracted/Component_5" ] || \
	LD_LIBRARY_PATH="$(dirname "$(readlink -f "$UNSHIELD")")/../lib" "$UNSHIELD" -d "$WORK/extracted" x "$WORK/setup/data1.cab" >/dev/null || true
X="$WORK/extracted"
[ -f "$X/Component_5/Engine.u" ] || { echo "extraction failed"; exit 1; }
mkdir -p "$OUT"/{System,Maps,Music,Textures,Sounds,Save}
cp -r "$X/Component_5/." "$OUT/System/"
cp "$WORK/setup/hgame.u" "$OUT/System/HGame.u"
cp "$X"/Component_7/*.usa "$OUT/System/"
cp "$X"/Component_2/*.unr "$OUT/Maps/"
cp "$X"/Component_3/*.ogg "$OUT/Music/"
cp "$X"/Component_4/*.utx "$OUT/Textures/"
cp "$X/Component_8/AllDialog.USA_uax" "$OUT/Sounds/AllDialog.uax"
# Source-control droppings shipped on the disc (vssver.scc, mssccprj.scc) are not game files.
find "$OUT" -iname "*.scc" -delete
chmod -R a-w "$OUT"/{System,Maps,Music,Textures,Sounds}
echo "retail tree: $OUT ($(find "$OUT" -type f | wc -l) files, $(du -sh "$OUT" | cut -f1))"

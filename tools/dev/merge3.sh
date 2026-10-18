#!/usr/bin/env bash
# merge3.sh — three-way merge dry run of HP2 against engine-ut99, per package (default: Core Engine Render Fire
# SoftDrv Editor). Deletes and recreates <outdir>; only reads the three source trees.
#
#   tools/dev/merge3.sh <outdir> [Pkg ...]
#   with EPIC_SOURCE=/home/ion/WorkUnreal/epic-sources/ut99-v400 UT99_SOURCE=/home/ion/WorkUnreal/engine-ut99/Source
#
# base   = $EPIC_SOURCE   Epic UT99 v400, the common ancestor (CRLF)
# ours   = $HP2_SOURCE    HP2 build 433 (LF): this repo's Source/ by default; the original drop is
#                         /home/ion/WorkUnreal/epic-sources/hp2-ue1-433-proto/Source
# theirs = $UT99_SOURCE   engine-ut99's Source/ (LF)
#
# For every file under <Pkg>/{Inc,Src} of ours, writes the `git merge-file --diff3` result to
# <outdir>/<Pkg>/<sub>/<file> and one line per file to <outdir>/summary.tsv:
#   THEIRS-UNCHANGED         engine-ut99 did not change it: nothing to port
#   OURS-UNCHANGED           HP2 matches v400: engine-ut99's file is copied to the output as-is
#   CLEAN                    merged without conflict: review, copy in
#   CONFLICTS=<n>            n conflict hunks: resolve by hand (the file has diff3 markers)
#   HP2-ONLY-OR-DROPPED      engine-ut99 has no such file: keep HP2's
#   NO-BASE                  HP2 and engine-ut99 have it, v400 does not
#   ERROR                    git merge-file failed
set -u
HP2=${HP2_SOURCE:-"$(cd "$(dirname "$0")/../../Source" && pwd)"}
EPIC=${EPIC_SOURCE:-/home/ion/epic-sources/ut99-v400}
UT=${UT99_SOURCE:-/home/ion/engines/engine-ut99/Source}
OUTROOT=${1:?usage: merge3.sh <outdir> [Pkg ...]}
shift
PKGS=("$@"); [ ${#PKGS[@]} -eq 0 ] && PKGS=(Core Engine Render Fire SoftDrv Editor)

rm -rf "$OUTROOT"; mkdir -p "$OUTROOT"
SUM="$OUTROOT/summary.tsv"; : > "$SUM"
norm() { perl -pe 's/\r\n/\n/g; s/\r$//' "$1"; }

for pkg in "${PKGS[@]}"; do
  for sub in Inc Src; do
    [ -d "$HP2/$pkg/$sub" ] || continue
    for f in "$HP2/$pkg/$sub"/*; do
      [ -f "$f" ] || continue
      b=$(basename "$f")
      case "$b" in *.dsp|*.dsw|*.scc|makefile|*.asm|*.vsm|*.dsm|CMakeLists.txt) continue;; esac
      ours="$f"; base="$EPIC/$pkg/$sub/$b"; theirs="$UT/$pkg/$sub/$b"
      mkdir -p "$OUTROOT/$pkg/$sub"
      if [ ! -f "$theirs" ]; then echo -e "$pkg/$sub/$b\tHP2-ONLY-OR-DROPPED" >> "$SUM"; continue; fi
      if [ ! -f "$base" ];   then echo -e "$pkg/$sub/$b\tNO-BASE" >> "$SUM"; continue; fi
      if diff -q --strip-trailing-cr -w "$base" "$theirs" >/dev/null; then
        echo -e "$pkg/$sub/$b\tTHEIRS-UNCHANGED" >> "$SUM"; continue
      fi
      if diff -q --strip-trailing-cr -w "$base" "$ours" >/dev/null; then
        echo -e "$pkg/$sub/$b\tOURS-UNCHANGED" >> "$SUM"; cp "$theirs" "$OUTROOT/$pkg/$sub/$b"; continue
      fi
      nb=$(mktemp); norm "$base" > "$nb"
      nt=$(mktemp); norm "$theirs" > "$nt"
      out="$OUTROOT/$pkg/$sub/$b"
      git merge-file -p --diff3 -L HP2-433 -L epic-v400 -L engine-ut99 "$ours" "$nb" "$nt" > "$out" 2>/dev/null
      rc=$?
      if   [ $rc -eq 0 ]; then echo -e "$pkg/$sub/$b\tCLEAN" >> "$SUM"
      elif [ $rc -lt 128 ]; then echo -e "$pkg/$sub/$b\tCONFLICTS=$rc" >> "$SUM"
      else echo -e "$pkg/$sub/$b\tERROR" >> "$SUM"; fi
      rm -f "$nb" "$nt"
    done
  done
done

echo "=== per-status counts";  cut -f2 "$SUM" | sed 's/=.*//' | sort | uniq -c
echo "=== conflicts (hunks)";  grep CONFLICTS "$SUM" | sed 's/CONFLICTS=//' | sort -k2 -n -r
echo "=== take theirs";        grep OURS-UNCHANGED "$SUM" | cut -f1
echo "=== clean";              grep -P "\tCLEAN$" "$SUM" | cut -f1
echo "results: $OUTROOT  (summary.tsv)"

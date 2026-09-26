#!/usr/bin/env bash
# HP2 -- native Linux launcher (installed into a run tree by mkrun-hp2.sh).
#
#   ./run.sh                       software renderer, title map
#   ./run.sh null                  NullDrv (GPU-free)
#   ./run.sh gl                    XOpenGLDrv
#   ./run.sh utglr                 OpenGLDrv (UTGLR)
#   ./run.sh soft Ch1Rictusempra   straight into a map (extra args go to the game)
#
#   RES=1920x1080   viewport size (written to Game.ini's [NSDLDrv.NSDLClient] once it exists)
#
# Headless: `Xvfb :99 -screen 0 1280x720x24 & DISPLAY=:99 ./run.sh`. On Wayland, prefix with
# SDL_VIDEODRIVER=x11 if no window appears.
set -eu
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
INI="$HERE/System/Game.ini"

ARGS="-SOFTWARERENDERER"
case "${1:-}" in
    soft)  shift ;;
    null)  ARGS="-NULLRENDERER"; shift ;;
    gl)    ARGS="-XOPENGLRENDERER"; shift ;;
    utglr) ARGS="-UTGLRRENDERER"; shift ;;
esac

if [ -n "${RES:-}" ] && [ -f "$INI" ]; then
    sed -i -E "s|^WindowedViewportX=.*|WindowedViewportX=${RES%x*}|;
               s|^WindowedViewportY=.*|WindowedViewportY=${RES#*x}|" "$INI"
fi

cd "$HERE/System"
exec env LD_LIBRARY_PATH="$HERE/System:${LD_LIBRARY_PATH:-}" ./Game "$@" $ARGS -log

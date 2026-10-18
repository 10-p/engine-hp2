# SoftDrv on the web — what a native run does not show

`SoftDrv.SoftwareRenderDevice` is the v400 software renderer with HP2's build-433 render face
(`DrawTriangles`, `MaxVertices`, the 5-argument `DrawComplexSurface`). `BUILD_SOFTDRV` is on by default on every
preset. The launcher selects it with `-SOFTWARERENDERER` (the pick then persists) or when no hardware device is
compiled in; it is not in the hardware preference order, and an ini that merely names it does not make it the
default.

Read this before changing anything under `Source/SoftDrv/`: a change validated natively can still be broken on the
web, because the two builds draw into different memory at different sizes.

## 1. The rasteriser code is the same on every build

Its inner loops exist twice, hand-written x86 `__asm` and portable C under `#if !ASM`. Every build here compiles
the portable C: `Source/SoftDrv/CMakeLists.txt` defines `ASM=0` (so does `Source/Core/Inc/UnGnuG.h` for every
GCC/Clang target), and `SoftDrvPrivate.h` forces `GIsMMX` to false wherever `__INTEL__` is set (every little-endian
target), so `Lock` picks the `Pentium32`/`Pentium16`/`Pentium15` colour modes, never the MMX ones. What differs
between native and web is the framebuffer, its size, the pointer width (`native64` is 64-bit, wasm is 32-bit) and
the optimisation level.

## 2. Native and web draw into different memory

| | native | web (the engine's worker) |
|---|---|---|
| target | an SDL streaming texture | `UNSDLViewport::SoftFB`, a plain heap buffer |
| obtained in `UNSDLViewport::Lock` via | `SDL_LockTexture` | the pointer itself |
| presented in `UNSDLViewport::Unlock` by | `SDL_RenderCopy` + `SDL_RenderPresent` | `putImageData` on a 2D context of the transferred canvas, with an ARGB-to-RGBA swizzle |
| resized by | SDL, when `SetClientSize` re-creates the texture | `Lock`, explicitly |

The web has its own buffer because SDL's Emscripten renderer is GLES-backed: `SDL_CreateRenderer` goes through
`eglCreateContext`, which Emscripten proxies to the browser main thread, where the canvas has been transferred to
the worker and cannot give a context; the proxied call never returns and the engine thread blocks. A 2D context is
available on the worker because SoftDrv never creates a WebGL one (a canvas has only one context).

The consequence: on the web, viewport size and buffer size are two facts that can disagree. Natively they cannot,
because SDL owns both. A bug of that shape is web-only and invisible to a native run.

## 3. The web viewport size is the canvas, and it changes

`UNSDLViewport::OpenWindow` adopts the canvas' device-pixel size (CSS size times `devicePixelRatio`, unaligned) over
the ini size, and `TickInput` re-applies it on every `SDL_WINDOWEVENT_SIZE_CHANGED`/`SDL_WINDOWEVENT_RESIZED`
through `SetClientSize`; the window-creation block of `OpenWindow` (`if( !hWnd )`) does not run again. So:

- the ini resolution is not the rendered resolution, and a test that passes at the ini size proves nothing about
  the size a player gets;
- the size changes after startup.

`SoftFB` is therefore sized in `UNSDLViewport::Lock`, the one point where the buffer is about to be written and
`SizeX`/`SizeY` are final; it is reallocated whenever the needed size differs, so it shrinks as well as grows, and a
device that never locks (NullDrv, the GL devices) allocates nothing. Keep it there: a buffer sized at window
creation or where the drawable size is adopted goes stale on one of the two paths.

## 4. The screen-indexed tables are static

The `SizeX`/`SizeY`-indexed scratch tables (`Setup`, `MMXSetup`, `Photon`, `FogPhoton`, `UCoordTable`, the two
`SpanIndex`) are static arrays bounded by `MaximumXScreenSize`/`MaximumYScreenSize`, both `SOFTDRV_MAX_SCREEN`. They
cost their full size in `.bss` at every resolution, so another screen-indexed static is not free and its cost does
not follow what is being drawn.

`SOFTDRV_MAX_SCREEN` is a CMake cache variable, 4096 by default; the wasm presets set 8192 because the canvas is the
display's device-pixel size. A viewport above the bound degrades rather than corrupting memory: `SetRes` clamps to
it, `DrawTriangles` draws nothing when `SizeY` exceeds it, and `DrawTile` clamps its span fill.

## 5. A clean dev build does not clear a memory bug

`wasm` compiles at `-O2 -g` with assertions; `wasm-perf`, the build that ships, links at `-O3 -g0` without them.
A heap overrun that one heap layout absorbs faults under another, and it shows as `memory access out of bounds`,
`table index is out of bounds` or `null function` depending on what the write lands on. If a rasteriser fault
appears and disappears across build modes, assume corruption and look for the write, not the read; run the change on
`wasm-perf` before calling it done.

## 6. Testing a SoftDrv change on the web

The frontend's `packages/e2e/tools/softdrv-matrix.cjs` (in `/home/ion/WorkUnreal/unreal-frontend-arch-private`)
picks the software renderer in the launcher, boots a map at each requested size and waits past level load, because a
rasteriser can come up and die on the first real draw:

```bash
cd /home/ion/WorkUnreal/unreal-frontend-arch-private/packages/e2e
SOFT_URL=https://localhost:8092 SOFT_CASES="1920x1080:Adv1Willow,640x480:Adv1Willow" node tools/softdrv-matrix.cjs
```

Vary the resolution, not only the map: one default-size smoke test covers one buffer size. The boot spec
`packages/e2e/hp2-proto-wasm-boot.spec.js` has a `renderer-software` test that checks `-SOFTWARERENDERER` runs on the
CPU, not on WebGL.

# OpenGLDrv — UTGLR on HP2 (UE1 build 433)

`OpenGLDrv.OpenGLRenderDevice` is [UTGLR](https://www.cwdohnal.com/utglr/), Chris Dohnal's legacy-OpenGL UE1 render
device, in the fork that adapts it to this engine's platform layer (NSDLDrv, SDL2, gl4es, the web) and to HP2's
build-433 render-device face. The web side (gl4es, the WebGL context, antialiasing in the browser) is in
[`WEB-INTEGRATION.md`](WEB-INTEGRATION.md).

## What this folder holds

Only the build glue: [`CMakeLists.txt`](CMakeLists.txt) and these two notes. The driver source is fetched.

| | |
|---|---|
| Repository | `UTGLR_GIT_REPOSITORY`, default `https://github.com/10-p/utglr-copy.git` |
| Branch | `UTGLR_GIT_TAG`, default `hp2` |
| Local clone | `-DFETCHCONTENT_SOURCE_DIR_UTGLR=<path>` builds an unpushed change |
| Compiled | `OpenGL.cpp`, `OpenGLDrv.cpp`, `c_gclip.cpp` (the fork is flat: every file at its root) |
| Definitions | `UPACKAGE_NAME=OpenGLDrv`, `UTGLR_UT_BUILD`, `UTGLR_ENGINE_UT99DC`, `UTGLR_ENGINE_HP2`, `__UNIX__`; `UTGLR_USE_GL4ES` with gl4es |
| Links | `Render`, `Engine`, `Core` and SDL2; gl4es when `USE_GL4ES` is on; no GL library (SDL loads the system one with the context) |

The fetch block is in [`Source/CMakeLists.txt`](../CMakeLists.txt); `Source/Game/CMakeLists.txt` records the
repository and branch in the web artifact's `meta.json`.

## Build

| Option | Default | Effect |
|---|---|---|
| `BUILD_UTGLR` | `OFF` | builds the package; set by the `native`, `native64`, `windows` and `windows64` presets, forced on under Emscripten and on Android |
| `USE_GL4ES` | `OFF` native, forced `ON` under Emscripten and on Android | links gl4es (desktop GL to GLES 2) instead of resolving GL from the context; turns `BUILD_UTGLR` on; the `windows` presets set it `OFF` |
| `GL4ES_SOURCE_DIR` | empty | a local gl4es checkout instead of the fetch of `GL4ES_GIT_REPOSITORY` (`https://github.com/10-p/gl4es.git`) at `GL4ES_GIT_TAG` (`ufront`) |

gl4es is fetched only when `BUILD_UTGLR` and (`USE_GL4ES` or Emscripten), and is configured `NOX11 NOEGL STATICLIB
DEFAULT_ES=2 NO_INIT_CONSTRUCTOR`: a static library, a GLES 2 target, no window-system code (NSDLDrv owns the
context), explicit initialisation.

```bash
cd Source
cmake --preset native && cmake --build --preset native -j"$(nproc)"     # UTGLR on the desktop context
cmake --preset native -DUSE_GL4ES=ON && cmake --build --preset native -j"$(nproc)"   # UTGLR through gl4es
```

The two backends:

- **Desktop GL** (`USE_GL4ES=OFF`): `GetGL1Proc` and `GetGLExtProc` resolve every entry point with
  `SDL_GL_GetProcAddress` against the context NSDLDrv made current, so the package links no GL library.
- **gl4es** (`USE_GL4ES=ON`): `<GL/gl.h>` comes from gl4es (its include directory is searched first), `Init` sets
  `LIBGL_ES=2`, `LIBGL_NOBANNER=1` and `LIBGL_USEVBO=0` (UTGLR feeds client-side vertex arrays), hands gl4es a
  resolver (`emscripten_webgl_get_proc_address` on the threaded web build, `SDL_GL_GetProcAddress` elsewhere) and
  calls `initialize_gl4es()`; entry points then resolve through `gl4es_GetProcAddress`, so every call goes through
  gl4es' translation. On the desktop this runs gl4es' GLES 2 path on the desktop compatibility context, on Android
  on an OpenGL ES 2 context; there the launcher has also set `LIBGL_NOPSA`, and `LIBGL_NOBGRA` on the emulator
  (BUILD.md, "Android").

## Selection and the GL context

The launcher (`Source/Game/Src/Launch.cpp`, `SelectRenderDevice`) picks the device before `Engine->Init()`:

1. `-UTGLRRENDERER` selects `OpenGLDrv.OpenGLRenderDevice` when it is compiled in, and the choice is written back
   (with the `ufrontUserRenderDevice` marker) once the engine has initialised, so it sticks on the next boot;
2. otherwise a persisted `GameRenderDevice` that is compiled in and is a hardware default or the user's pick;
3. otherwise the preference order, XOpenGLDrv first, then UTGLR.

The chosen class is written to `GameRenderDevice`, `WindowedRenderDevice` and `RenderDevice` of `[Engine.Engine]`.

`UNSDLViewport::OpenWindow` gives a window a GL context only when the render device's name contains `OPENGL`
(case-insensitive). The desktop gets a desktop compatibility-profile context, Android an OpenGL ES 2 context; the web
gets a WebGL context (see
[`WEB-INTEGRATION.md`](WEB-INTEGRATION.md)). The context is NSDLDrv's: `Init` loads no GL library (no
`SDL_GL_LoadLibrary`), and the viewport presents the frame. A rename of the package or class must keep `OpenGL` in
the name.

## Antialiasing

`UseAA` and `NumAASamples` (default 4) in `[OpenGLDrv.OpenGLRenderDevice]`. On the web, `SetRes` calls
`ufront_backbuffer_samples(UseAA ? NumAASamples : 0)` and the engine's `Source/Game/msaa-backbuffer.js` multisamples
framebuffer 0, which is where UTGLR draws (WebGL 2 only). Natively UTGLR draws without antialiasing: its SDL path
requests no multisampled pixel format and NSDLDrv creates a single-sample context.

## S3TC / DXT1

The only compressed format of this engine is `TEXF_DXT1`. The engine hands DXT1 to a device only when
`SupportsTC` is set, and decompresses it to RGBA itself otherwise. `SetRes` reads `UseS3TC` from the ini with a
default of on (UTGLR's per-parameter config default does not reach the device instance in this engine) and sets
`SupportsTC` from it; it drops to 0 when the context lacks `GL_EXT_texture_compression_s3tc` or
`GL_ARB_texture_compression`.

- Desktop GL: DXT1 uploads as `GL_COMPRESSED_RGBA_S3TC_DXT1_EXT` when the driver has the extension.
- gl4es: gl4es always reports both extensions and decodes DXT1 in software at `glCompressedTexImage2D`, uploading
  an uncompressed texture (non-4-aligned sizes included).

## The fork's engine gates

`OpenGL.h` derives the engine flavour after its build-config check:

| Macro | Meaning | Defined for |
|---|---|---|
| `UTGLR_ENGINE_UT99DC` | the platform layer: SDL2 shims, NSDLDrv-owned context, gl4es, web | UT99 v400 and HP2 |
| `UTGLR_ENGINE_V400` | derived (`UT99DC && !HP2`): v400's types, 3-arg `DECLARE_CLASS`, no `FTime` | UT99 v400 |
| `UTGLR_ENGINE_HP2` | the 433 render face and HP2's blend semantics | HP2 |
| `UTGLR_HAS_ALPHABLEND` | derived: upstream's Rune/227 `PF_AlphaBlend` plumbing | Rune, 227, HP2 |
| `PF_AlphaBlend` | synthetic `0x20000000`, the one `EPolyFlags` bit 433 leaves free; stripped from incoming flags | HP2 |

HP2 takes upstream v436's branches at the type sites: the 4-arg `DECLARE_CLASS(…, CLASS_Config, OpenGLDrv)` and an
`FTime m_prevFrameTimestamp` for the frame limiter (`appSeconds()` returns `FTime`).

Platform layer (`UTGLR_ENGINE_UT99DC`):

| Site | Upstream (v436, Win32) | The fork | Why |
|---|---|---|---|
| top of `OpenGL.cpp` | engine headers first | `<new>`, `<map>`, `<vector>` before `OpenGLDrv.h` | Core's inline `operator delete` must follow `<new>`'s declaration; on Emscripten's libc++ `<map>` pulls `<time.h>`, which must precede `UnFile.h`'s `#define clock(Timer)` |
| `OpenGL.h` | `#include <tchar.h>` | only without `__UNIX__` | gcc has no `<tchar.h>`; the debug strings use `= 0` instead of `_T('\0')` |
| `OpenGLDrv.h` | SDL 1.2 on Unix, WGL on Windows | `SDL2/SDL.h` on every platform, `UTGLR_USE_SDL` instead of `UTGLR_USE_WGL`; `SDL_GL_SwapBuffers` and `SDL_SetGammaRamp` are no-ops, `SDL_ListModes` reports any mode | NSDLDrv owns the window, the context, the swap and the resolution |
| `Init` | `SDL_GL_LoadLibrary("libGL.so.1")`, fatal on failure | no library load; gl4es initialisation with gl4es | the context is current before `Init`, and SDL reports a second load as an error |
| `SetRes` | `UseS3TC` from the config default | read from the ini, default on; `ufront_backbuffer_samples` on the web | see above |
| `Unlock` | — | a log line `OpenGLDrv(UTGLR): Unlock <n> (Blit=<b>)` every 60th frame | shows the render loop is running in a headless run |

The 433 face (`UTGLR_ENGINE_HP2`; the retail `Source/D3DDrv/Src/Direct3D7.cpp` is the behavioural reference):

| What | Why |
|---|---|
| `DrawComplexSurface(Frame, Surface, Facet, DWORD PolyFlags, BYTE cAlpha)` draws with the flags argument. A Lumos surface is `PF_HighShadowDetail` and (`PF_Modulated` or `PF_Translucent`): its markers are stripped, its opacity `1 - cAlpha/255` rides `glColor4f`'s alpha on unit 0, and the later modulate units pass that alpha through (`GL_COMBINE_ALPHA` = `GL_REPLACE` of `GL_PREVIOUS`). A fully faded surface without `PF_Occlude` is skipped; a see-through one gets no detail pass and a fog pass faded with it | `cAlpha` is transparency (255 = gone) and means something only on Lumos surfaces (`Source/Render/Src/UnRender.cpp`); a plain `PF_Modulated` poly (a blob shadow) keeps the modulate blend |
| `MaxVertices()` returns 256; `DrawTriangles(…, _WORD* Indices, INT NumIndices, …)` takes an indexed list, a fan (`Indices == NULL && NumIndices == NumPts`) or a plain list, each triangle through the buffered gouraud path | Render sizes its mesh and particle buffers from `MaxVertices()` once; every mesh (skeletal included), particle and decal comes through `DrawTriangles` |
| no `DrawGouraudPolygon` override; UTGLR's buffered body is the non-virtual `HP2DrawGouraudPolygon` | 433's base `DrawGouraudPolygon` is a forwarder to `DrawTriangles` |
| `HP2AdjustPolyFlags`: `PF_Highlighted\|PF_Translucent` becomes `PF_AlphaBlend`; with `PF_Masked` the pair is dropped and the poly draws masked | fading actors (`Opacity < 1`) blend straight, not additively |
| `PF_AlphaBlend` blends `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`, gains no `PF_Occlude` and no additive fog pass; its vertex alpha is `Light.W` (`FPlaneTo_RGBA`) | 433 puts per-vertex opacity in `Light.W` |
| an alpha-palette tile (`Palette[128].A != 255`, not translucent) draws `PF_Highlighted` without `PF_Occlude` | as D3D7 433: transparent texels of world sprites write no depth |
| `Draw3DLine` skips the camera transform for `LINE_PreTransformed` | as 433's base `Draw3DLine` |
| `UseFragmentProgram` is forced off (`ConfigValidate_RequiredExtensions`) | the ARB complex-surface programs use the primary alpha as the lightmap-scale selector and cannot carry Lumos opacity; gl4es has no ARB programs either |
| `GetOsSurface` is not overridden | only WinDrv calls it |

The fork adds no depth bias for `PF_TwoSided` polys.

The fork's `__asm`, SEH (`__try`) and SSE blocks compile only under MSVC (`_WIN32 && _MSC_VER`), so every build
here, MinGW included, takes UTGLR's C fallbacks; the Rune build's methods are compiled out.

## Config

The web `[OpenGLDrv.OpenGLRenderDevice]` section (`Config/System/web/Default.ini`) sets `UseAA=True`,
`NumAASamples=4`, `UseS3TC=True`, `UseFragmentProgram=False`, `UsePalette=False` and `OneXBlending=False`. The native
run tree uses the retail `Config/System/Default.ini`, whose `[OpenGLDrv.OpenGLRenderDevice]` section applies as is.

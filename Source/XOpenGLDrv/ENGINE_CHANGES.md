# XOpenGLDrv — the OldUnreal renderer on HP2 (UE1 build 433)

`XOpenGLDrv.XOpenGLRenderDevice` is the OldUnreal OpenGL 3/4 renderer, in the fork that runs it on the vanilla
v400-family engines (UT99 v400 and HP2 433), on desktop GL and on WebGL 2. It is the default hardware device.
Building and testing it for the browser is in [`WEB_INTEGRATION.md`](WEB_INTEGRATION.md).

## What this folder holds

- [`CMakeLists.txt`](CMakeLists.txt), the build glue. The driver source, and its `XOpenGLDrv.int`, are fetched.

| | |
|---|---|
| Repository | `XOPENGL_GIT_REPOSITORY`, default `https://github.com/10-p/XOpenGLDrv.git` |
| Branch | `XOPENGL_GIT_TAG`, default `hp2-v433-compat` |
| Local clone | `-DFETCHCONTENT_SOURCE_DIR_XOPENGLDRV=<path>` builds an unpushed change |
| Compiled | every `Src/*.cpp` of the fetched tree (`file(GLOB)`: shaders `*_GLSL.cpp`, the vendored desktop-GL `glad.cpp`, the DXT1 decoder `decompress.cpp`) |
| Includes | the fork's `Inc/` (its own `glad.h`/`glext.h`) and `glm/`; `Render.h` from `Source/Render/Inc` through the `Render` target |
| Definitions | `XOPENGLDRV_EXPORTS`, `UPACKAGE_NAME=XOpenGLDrv`, `SDLBUILD=1 SDL2BUILD=1 SDL3BUILD=0` (the fork picks its SDL flavour from `__LINUX__`/`MACOSX`, which this engine does not define) |
| Links | `Render`, `Engine`, `Core` and SDL2 |

`UPACKAGE_NAME` must stay identical to `IMPLEMENT_PACKAGE(XOpenGLDrv)` and to the ini value
`XOpenGLDrv.XOpenGLRenderDevice`: the static build mangles the package symbols from it.

## Build and selection

`BUILD_XOPENGLDRV` defaults to `OFF` natively and is forced `ON` under Emscripten; the `native`, `native64`,
`windows` and `windows64` presets set it. The launcher's `-XOPENGLRENDERER` switch (`GRenderSwitches` in
`Source/Game/Src/Launch.cpp`) selects it, and it heads the preference order used when no switch and no eligible
persisted device are present. `UNSDLViewport::OpenWindow` gives it a GL context because its name contains
`OPENGL`; on the web that is a WebGL 2 context, and because the name contains `XOPENGL` it never falls back to
WebGL 1.

The device creates no context: on the v400-family engines `CreateOpenGLContext` adopts the one NSDLViewport made
current (`SDL_GL_GetCurrentContext`, or `emscripten_webgl_get_current_context` on the threaded web build), `Unlock`
does not swap (the viewport presents), and `Exit` drops its references without deleting the context, which the
viewport destroys in `CloseWindow`.

## The fork's gates (`Inc/XOpenGLDrv.h`)

| Macro | True on | Carries |
|---|---|---|
| `XOGL_UE1_V4XX` | `ENGINE_VERSION` 400 and 433 | the vanilla-engine adaptations below: Core/TCHAR/TArray shims, context adoption, the WebGL profile and its fixes, gamma defaults |
| `XOGL_HP2` | 433 | the 433 render-device face and HP2's blend semantics; `#include "Render.h"`; the 4-arg `DECLARE_CLASS(…, CLASS_Config, XOpenGLDrv)` |

Three sites stay `ENGINE_VERSION==400` only: the `RenderPrivate.h` include, the skipped second `UnRender.h` include,
and `PF_AlphaBlend`/`PF_Straight_AlphaBlend`/`PF_Premultiplied_AlphaBlend` defined to 0 (on v400 the 469 value of
`PF_AlphaBlend` collides with `PF_CloudWavy`). HP2's private render header is `Source/Render/Inc/Render.h`, whose
`UnRender.h` is include-guarded.

## The 433 face (`XOGL_HP2`)

The behavioural reference is the retail `Source/D3DDrv/Src/Direct3D7.cpp`.

- **`DrawComplexSurface(Frame, Surface, Facet, DWORD PolyFlags, BYTE cAlpha)`** (`Src/DrawComplex.cpp`) uses the
  flags argument, never `Surface.PolyFlags` (Render's Lumos path passes a modified copy). A surface with
  `PF_HighShadowDetail` and a blend bit (`PF_Modulated|PF_Translucent`) is Lumos: straight alpha
  (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`) with opacity `(255-cAlpha)/255` (`cAlpha` is transparency: 255 = gone),
  the blend bits, `PF_Masked` and the Lumos markers (`PF_Gouraud`, `PF_DirtyShadows`) stripped, depth writes as the
  `PF_Occlude` Render decided. A masked Lumos surface keeps its masked texture upload but blends instead of
  alpha-testing. D3D7's "any bit of `PF_LumosAffected`" test, which would also catch every plain `PF_Modulated`
  poly, is not reproduced.
- **`MaxVertices()`** returns 256, the value of every HP2 device (Render sizes its mesh and particle buffers once,
  from the first device that renders a mesh).
- **`DrawTriangles(Frame, Info, Pts, NumPts, _WORD* Indices, NumIndices, PolyFlags, Span)`**
  (`Src/DrawGouraud.cpp`): an indexed triangle list (meshes, CPU-skinned skeletal ones included, and particles);
  `Indices == NULL && NumIndices == NumPts`, one convex fan (decals, blob shadows); otherwise a plain list. Normals
  are zeroed unless hardware lighting or bump maps read them (particles never write `Normal`). Per-vertex alpha is
  `Light.W`.
- **No `DrawGouraudPolygon`**: 433's base `URenderDevice::DrawGouraudPolygon` forwards to `DrawTriangles` as a fan.
  The v400 body is the private fan path `DrawGouraudFan`.
- **`PF_AlphaBlend` and `PF_Straight_AlphaBlend` are the synthetic `PF_XOGL_AlphaBlend = 0x20000000`** (the one
  `EPolyFlags` bit 433 leaves free; `PF_Premultiplied_AlphaBlend` is 0). With it, the fork's alpha-blend paths
  (`SetBlend`, `DF_AlphaBlended`, the Gouraud vertex alpha) work unchanged.
- **`HP2AdjustPolyFlags`** (`Src/SetTexture.cpp`), called on entry by `DrawTriangles` and `DrawTile`: strips the
  synthetic bit; alpha-palette tiles (`Palette[128].A != 255`, not translucent) become `PF_Highlighted`
  (premultiplied, as D3D7's `DrawTile`); `PF_Highlighted|PF_Translucent` becomes straight alpha (fading actors,
  `Opacity < 1`), or masked when `PF_Masked` is set, as SoftDrv does; `PF_Modulated` drops `PF_RenderFog`.
- **`Draw3DLine`** (`Src/DrawSimple.cpp`) skips the camera transform for `LINE_PreTransformed`.
- `GetOsSurface` is not overridden (the base returns NULL; only WinDrv calls it).

`ActorXBlending` stays `True` (the web ini sets it, and it is the default): with it off the Gouraud shader scales the
vertex colour by 1.5, `Light.W` included, which makes fading actors too opaque.

## Adaptations for the vanilla engines (`XOGL_UE1_V4XX`)

| Where | What | Why |
|---|---|---|
| `Inc/XOpenGLDrv.h` | `BOOL`/`TRUE`/`FALSE`, `PF_None`, `NAME_DevGraphics` | absent from vanilla Core/Engine |
| `Inc/XOpenGL.h` | `clockFast`/`unclockFast`; `FShaderWriterX` uses `TArray::Empty`/`Add` | no OldUnreal stat macros or `Reserve`/`AddNoCheck` |
| `Inc/XOpenGL.h` | `INT MaxTextureSize` member | normally from the 469 base class |
| `Inc/XOpenGL.h` | 469 texture-format names aliased to the vanilla values (`TEXF_BGRA8_LM` = `TEXF_RGBA7`, `TEXF_R5G6B5` = `TEXF_RGB16`, `TEXF_BC1` = `TEXF_DXT1`, `TEXF_BGRA8` = `TEXF_RGBA8`; the rest are dead sentinels), `FIsCompressedFormat`, `FTextureFormatString`, `ERenderZTest` | the vanilla `ETextureFormat` (identical on 400 and 433) |
| `Src/XOpenGL.cpp` `Init` | `OpenGLVersion = GL_ES_WEBGL` under Emscripten | a browser gives only a WebGL 2 context; the render paths key off the profile, not the platform |
| `Src/XOpenGL.cpp` | `CreateOpenGLContext` adopts the current context; `SelectGLVersion` takes 3.3 without probing (`IsSupportedGLVersion` opens SDL windows of its own); `Unlock` does not swap; `Exit` deletes no context | NSDLViewport owns window, context and present |
| `Src/XOpenGL.cpp` | the post-process present draws at `Viewport->SizeX/SizeY` | no `PhysicalSizeX/Y` |
| `Src/XOpenGL.cpp` `SetGamma` | a `Brightness <= 0` becomes 0.5; a `GammaMultiplier <= 0` becomes 1.75 | the web client's `Brightness` is 0, and the config default of `GammaMultiplier` does not reach the instance, so `Gamma` would be 0 and the present `pow(color, 1/Gamma)` black |
| `Src/XOpenGL.cpp` `GetViewportGamma` | `Gamma <= 0` returns 1 | last guard against the same black present |
| `Src/DrawComplex.cpp`, `Src/DrawGouraud.cpp` | bump-map `Lock` at `Viewport->CurrentTime`; `NoNearZ = FALSE` | no `FTime()` default; no `GUglyHackFlags` |
| `Src/SetTexture.cpp` | `GWarn->Logf` for the editor's unpack diagnostic | no `appMsgf` |
| `Src/ShaderProgram.cpp` | `DumpShader` without `TArray<FString>::AddItem`; ANSI `ADD_OPTION` and compile-error log | vanilla `AddItem` assigns onto unconstructed memory; `TCHAR` is `char` |
| `Src/ShaderProgram.cpp` | the `DistanceFogParams` uniform block is declared only under `OPT_DistanceFog` | it is bound only where distance fog exists (227); an active unbound block makes WebGL 2 reject every draw |
| `Src/CheckExtensions.cpp` | S3TC accepted under `WEBGL_compressed_texture_s3tc` and `GL_WEBGL_compressed_texture_s3tc` | WebGL names the extension without `GL_EXT_`; the format tokens are the same |
| `Src/SetTexture.cpp` | `TEXF_BC1` reaches the compressed upload with its size computed as 8 bytes per 4x4 block | `FTextureBytes` exists only on OldUnreal engines |
| `Src/SetTexture.cpp` | on the `ES_WEBGL` profile, DXT1 whose base size is not a multiple of 4 is decoded to RGBA8 (`XOpenGLDecodeDXT1Image` over gl4es' public-domain block decoder, `Src/decompress.cpp`, `Inc/decompress.h`) and uploaded uncompressed | WebGL 2 rejects such sizes in `glCompressedTexImage2D`; block-aligned DXT1 still uploads compressed |

Format strings use `%s` throughout, ungated: every engine the fork builds for is ANSI (`TCHAR` is `char`), where
`%ls` drops the argument, and the generated GLSL `#define`s, uniform and block names, extension checks and logs all
go through them. The HW-light gather uses the 430-family field layout (`FPlane.X/Y/Z`, `Region.ZoneNumber`), which
433 falls in.

## GL profiles

`OpenGLVersion` (`[XOpenGLDrv.XOpenGLRenderDevice]`) is `Core` (0), `ES` (1) or `ES_WEBGL` (2). `ES` is the upstream
desktop GLES 3.1 path (`#version 310 es`, io-blocks). `ES_WEBGL` is the WebGL 2 subset: `#version 300 es`,
flattened varyings (`DrawTile_GLSL.cpp`, `DrawGouraud_GLSL.cpp`), S3TC under the WebGL name. `IsES()` is true for
both ES profiles and `IsWebGL()` for `ES_WEBGL` only; `SetOptionsForRendererConfig` sets `OPT_GLES` for both and
`OPT_GLES_WEBGL` for the WebGL one, and the shader generators branch on those. The web forces `ES_WEBGL`; natively the
ini decides (default `Core`), so `OpenGLVersion=ES_WEBGL` runs the WebGL shader path on a desktop context.

## Antialiasing

`UseAA` (default on) and `NumAASamples` (default 4, at least 2 with `UseAA`, at most `GL_MAX_SAMPLES`). With `UseAA`
the frame is drawn into a multisampled `RenderFBO` (`UpdateRenderFBO`: RGBA8 and DEPTH24_STENCIL8 renderbuffers),
resolved each frame into `RenderResolvedFBO` with `glBlitFramebuffer`, and post-processed to the screen; screenshot
reads go to the resolved frame. The same path runs natively and on WebGL 2. Natively, `NoAATiles` turns
`GL_MULTISAMPLE` off while tiles draw; WebGL has no such switch.

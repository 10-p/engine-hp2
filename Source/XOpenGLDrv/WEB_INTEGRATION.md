# XOpenGLDrv on the web (WebGL 2)

How the default web renderer is built, gets its context and is tested. The fork's code changes, the 433 render face
and the GL profiles are in [`ENGINE_CHANGES.md`](ENGINE_CHANGES.md); the presets in [`BUILD.md`](../../BUILD.md).

## Build

The wasm presets need no extra flag: under Emscripten `Source/CMakeLists.txt` forces `BUILD_XOPENGLDRV` on, beside
OpenGLDrv (UTGLR on gl4es), SoftDrv and NullDrv, and links them all into one binary.

```bash
source /home/ion/WorkUnreal/env.sh          # EMSDK for the presets' toolchain file
cd Source
cmake --preset wasm-perf && cmake --build --preset wasm-perf -j"$(nproc)"   # shipping, what the frontend publishes
cmake --preset wasm && cmake --build --preset wasm -j"$(nproc)"             # dev: -O2 -g, assertions
# -> build-wasm-perf/SDLLaunch/SDLLaunch.{js,wasm,data} + meta.json (and build-wasm/SDLLaunch/ for wasm)
```

`meta.json` lists the fetched `xopengl` repository and branch. The link flags XOpenGL depends on
(`Source/Game/CMakeLists.txt`): `-sMAX_WEBGL_VERSION=2` (with `-sMIN_WEBGL_VERSION=1` for UTGLR's fallback) and
`-sGL_ENABLE_GET_PROC_ADDRESS=1`, which glad needs to resolve its entry points.

`Source/XOpenGLDrv/CMakeLists.txt` collects `Src/*.cpp` with `file(GLOB)` and no `CONFIGURE_DEPENDS`: after a
`Src/*.cpp` is added to or removed from a local clone (`FETCHCONTENT_SOURCE_DIR_XOPENGLDRV`), re-run the configure
step (`cmake --preset <preset>`) before building, or the file set stays the old one.

## The context

`UNSDLViewport::OpenWindow` sees `XOPENGL` in the render device's name and creates a WebGL 2 context on the engine's
worker with `emscripten_webgl_create_context` (SDL's EGL path is proxied to a main thread that cannot reach the
transferred canvas), then makes it current. XOpenGL has no WebGL 1 fallback: without WebGL 2 the viewport stops
with `Could not create WebGL context on the worker`. The device adopts the context with
`emscripten_webgl_get_current_context()` and loads its entry points with glad's desktop-GL loader over
`emscripten_webgl_get_proc_address` (the GL 3 names cover the GLES 3 subset XOpenGL uses: uniform buffers, vertex
arrays, `glMapBufferRange`, instancing). GL 4 entry points resolve to null, and `CheckExtensions` turns off what the
context lacks (bindless textures, persistent buffers, shader draw parameters). `Unlock` does not present: the
viewport commits the frame with `emscripten_webgl_commit_frame()`.

## Selection

`-XOPENGLRENDERER` (`GRenderSwitches` in `Source/Game/Src/Launch.cpp`), or no switch at all: XOpenGL heads the
launcher's preference order, and the web `Default.ini` names it as `GameRenderDevice`. The launcher writes the
chosen device to `GameRenderDevice`, `WindowedRenderDevice` and `RenderDevice`.

## Profile and config

On the web `Init` forces `OpenGLVersion=ES_WEBGL` (`#version 300 es`, flattened varyings, S3TC under the WebGL
extension name); the web ini names it too. Natively `OpenGLVersion=ES_WEBGL` in `[XOpenGLDrv.XOpenGLRenderDevice]`
runs the same shader path on a desktop context.

The web `[XOpenGLDrv.XOpenGLRenderDevice]` section (`Config/System/web/Default.ini`) sets, among others,
`UseAA=True`, `NumAASamples=4`, `NoAATiles=True`, `ActorXBlending=True`, `OneXBlending=True`,
`GammaMultiplier=1.75`, `DetailMax=3`, and turns off what WebGL 2 cannot do (`UseBindlessTextures`,
`UsePersistentBuffers`, `UseShaderDrawParameters`, `UseHWLighting`, `UseHWClipping`). On the web `Init` reads
`DetailMax` from that section before the shaders are built (default 2).

## Antialiasing

XOpenGL multisamples its own framebuffers: with `UseAA` it draws into a multisampled `RenderFBO` and resolves it
each frame into `RenderResolvedFBO`, which the post-process pass presents and screenshots read. The WebGL context
itself is single-sample (`antialias=0`); the engine's `Source/Game/msaa-backbuffer.js` is not involved (it serves
UTGLR, which draws into framebuffer 0).

## S3TC / DXT1

WebGL exposes S3TC as `WEBGL_compressed_texture_s3tc`, not `GL_EXT_texture_compression_s3tc`; the fork accepts both
names (and `GL_WEBGL_compressed_texture_s3tc`), so `SupportsS3TC` holds and block-aligned DXT1 uploads with
`glCompressedTexImage2D` (`GL_COMPRESSED_RGBA_S3TC_DXT1_EXT` is the same token). WebGL 2 rejects DXT1 whose size is
not a multiple of 4 (a 128x2 gradient, for one); on the `ES_WEBGL` profile those are decoded to RGBA8 in software
(`Src/decompress.cpp`) and uploaded uncompressed.

## Testing in the browser

The frontend (`/home/ion/WorkUnreal/unreal-frontend-arch-private`) assembles the host page, glue and assets around
the artifact:

```bash
cd /home/ion/WorkUnreal/unreal-frontend-arch-private
yarn build --target hp2-proto --engine-artifact /home/ion/WorkUnreal/engine-hp2/build-wasm-perf/SDLLaunch \
  --assets /home/ion/WorkUnreal/assets/hp2-proto-assets
yarn serve
```

`yarn publish-all` builds both web presets of the engine itself (`wasm-perf` for shipping, `wasm` for the debug
artifact). The boot spec `packages/e2e/hp2-proto-wasm-boot.spec.js` boots the default renderer in its first-light
test (`hp2-retail-exp-wasm-boot.spec.js` for the retail target).

On native, `cmake --preset native && cmake --build --preset native` builds the same device against the desktop
context; `tools/dev/mkrun-hp2.sh` assembles a run tree for it.

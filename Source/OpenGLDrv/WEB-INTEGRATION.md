# OpenGLDrv (UTGLR) on the web

UTGLR runs in the browser through gl4es (legacy desktop GL to GLES 2), on the WebGL context NSDLDrv creates. The
driver, its fork and its native build are in [`PORTING.md`](PORTING.md).

## Build

The wasm presets need no extra flag: under Emscripten `Source/CMakeLists.txt` forces `USE_GL4ES` and `BUILD_UTGLR`
on, beside XOpenGLDrv, SoftDrv and NullDrv. gl4es is linked statically into the wasm; it is not glue and changes
neither the ABI (`vendor/glue/ue_abi.h`) nor the artifact's shape.

```bash
source /home/ion/WorkUnreal/env.sh          # EMSDK for the presets' toolchain file
cd Source
cmake --preset wasm-perf && cmake --build --preset wasm-perf -j"$(nproc)"
# -> build-wasm-perf/SDLLaunch/SDLLaunch.{js,wasm,data} + meta.json
```

`wasm` (dev, `-O2 -g`, assertions) and `wasm-debug` (`-O0`) build the same driver set. `meta.json` lists the
fetched `utglr` and `gl4es` repositories and branches.

The link flags in `Source/Game/CMakeLists.txt` that UTGLR depends on:

| Flag | Why |
|---|---|
| `-sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2` | UTGLR runs on WebGL 2 and falls back to WebGL 1 |
| `-sFULL_ES2=1` | gl4es emits client-side vertex arrays, which WebGL rejects; Emscripten emulates them with temporary buffers |
| `-sGL_ENABLE_GET_PROC_ADDRESS=1` | gl4es resolves its backend entry points at run time |
| `-sOFFSCREEN_FRAMEBUFFER` | the context presents through `emscripten_webgl_commit_frame`, and "framebuffer 0" is Emscripten's emulated one |
| `--js-library msaa-backbuffer.js` | the multisampled framebuffer 0 behind `UseAA` |

`Source/Game/offscreen-depth24.sh` rewrites the emitted loader so the emulated framebuffer 0 has a 24-bit
depth/stencil buffer (Emscripten allocates a 16-bit one), and fails the build when the rewrite does not apply.

## The WebGL context

`UNSDLViewport::OpenWindow` creates the context itself on the engine's worker with
`emscripten_webgl_create_context` (SDL's EGL path is proxied to a main thread that cannot reach the transferred
canvas), with `antialias=0`, `explicitSwapControl=1` and `renderViaOffscreenBackBuffer=1`, and makes it current
before the device's `Init`.

- Every GL device asks for WebGL 2.
- `-WEBGL=1` on the command line gives UTGLR WebGL 1; a browser without WebGL 2 does the same. XOpenGLDrv never
  falls back (it needs WebGL 2).
- gl4es is pointed at `emscripten_webgl_get_proc_address`, which resolves against the worker's current context (SDL
  owns no context there).

## Selecting UTGLR

`-UTGLRRENDERER`, from the launcher's switch table (`GRenderSwitches` in `Source/Game/Src/Launch.cpp`); the choice
persists to the next boot. Without a switch the launcher takes the persisted device or the preference order
(XOpenGLDrv first). The frontend's targets offer it in their renderer list
(`/home/ion/WorkUnreal/unreal-frontend-arch-private/targets/hp2-proto/target.json`, `videoDrivers`, and the same in
`targets/hp2-retail-exp`).

## Antialiasing

`UseAA=True` and `NumAASamples=4` in the web `[OpenGLDrv.OpenGLRenderDevice]` section. `SetRes` calls
`ufront_backbuffer_samples(UseAA ? NumAASamples : 0)`; `msaa-backbuffer.js` then replaces Emscripten's emulated
framebuffer 0 with a multisampled one (samples clamped to `MAX_SAMPLES`), resolves it into Emscripten's own
framebuffer at the present, and resolves first for a read of framebuffer 0 (`readPixels`, `copyTexImage2D`,
`copyTexSubImage2D`: screenshots and save thumbnails). On WebGL 1 it returns 0 and UTGLR draws without
antialiasing. UTGLR's own AA state stays off on the web: it would switch masked textures to alpha-to-coverage and
toggle `GL_MULTISAMPLE`, which gl4es only records.

## Textures and config

gl4es reports S3TC, so the engine hands DXT1 textures to UTGLR, and gl4es decodes them to uncompressed textures at
upload. The web ini keeps `UseFragmentProgram=False` and `UsePalette=False` (gl4es has neither ARB programs nor
paletted textures; UTGLR's fixed-function and RGBA paths cover both, and HP2 forces `UseFragmentProgram` off in
code).

## Testing in the browser

The frontend (`/home/ion/WorkUnreal/unreal-frontend-arch-private`) assembles the host page, glue and assets around
the artifact: `yarn build --target hp2-proto --engine-artifact /home/ion/WorkUnreal/engine-hp2/build-wasm-perf/SDLLaunch`
(the asset tree comes from `--assets` or `.ufront/publish.local.json`), then `yarn serve`. Its boot spec
`packages/e2e/hp2-proto-wasm-boot.spec.js` has a `renderer-utglr` test that selects UTGLR and checks it, not
XOpenGL, came up.

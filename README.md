# engine-hp2 — Harry Potter and the Chamber of Secrets (PC), Unreal Engine 1 build 433

The engine of Harry Potter and the Chamber of Secrets (KnowWonder / Electronic Arts, 2002), ported from its MSVC 6
Windows build to Linux, Windows (MinGW-w64) and the web (Emscripten/WebAssembly). The repository holds engine source
only: the game content is never in git. The web build is consumed by the frontend
(`unreal-frontend-arch-private`), which owns the host page, the launcher UI and the browser glue behind the thin ABI
shims in `vendor/glue/`.

Two branches, one engine:

- `main` (this checkout, `engine-hp2`) runs the September 2002 prototype content: frontend target `hp2-proto`.
- `retail` (the second checkout, `engine-hp2-retail`) adds what the retail disc (build 021009) needs, recovered from
  the retail DLLs: target `hp2-retail-exp`. It holds retail-specific commits only and is rebased on `main`.

Building and running: [`BUILD.md`](./BUILD.md). Agent guide: [`CLAUDE.md`](./CLAUDE.md). The documents in `docs/`:
[`docs/README.md`](./docs/README.md).

## Changes from the original source

The baseline is the restructured prototype drop (`/home/ion/WorkUnreal/epic-sources/hp2-ue1-433-proto`, import commit
`754be39`). The full list, with the medium and small changes, is in the frontend's
`/home/ion/WorkUnreal/unreal-frontend-arch-private/docs/14-change-inventory.md`.

- **WebAssembly build** — the `wasm`, `wasm-debug` and `wasm-perf` presets build one static `SDLLaunch.{js,wasm,data}`
  plus `meta.json` (pthreads with the engine on a worker, WasmFS, Asyncify, an offscreen framebuffer); the web boot
  inis are preloaded, the main loop yields every frame, the web driver set is forced, and the package loaders fail
  soft because exception catching is off.
- **SDL2 client driver** — NSDLDrv on HP2's 433 interfaces: window, input, game pads with hot-plug, the options-page
  commands; every GL device gets WebGL 2 on the web.
- **OpenGL renderers** — XOpenGLDrv (WebGL 2) and UTGLR OpenGLDrv (WebGL 1/2 through gl4es), fetched from their forks,
  which carry HP2's 433 render interface; this repository carries the build glue and the multisampled back buffer.
- **Software renderer** — the v400 SoftDrv (portable C paths) with HP2's 433 render interface (`-SOFTWARERENDERER`).
- **Null renderer** — NullDrv, a GPU-free render device for headless runs (`-NULLRENDERER`).
- **WebRTC net driver** — WebRTCDrv over the glue's `ue_net_*` imports, the web's `NetworkDevice` (P2P is off for HP2).
- **OpenAL Soft audio** — NOpenALDrv, HP2's retail ALAudio ported to openal-soft, with an in-tree EA-XA ADPCM decoder
  and Ogg Vorbis music, natively and on the web.
- **Android build** — a Gradle/NDK project builds the game as `libHP2.so` with SDL2, a launcher picks the game
  folder, touch is the mouse in the menus and an on-screen overlay drives play, game pads take the shared path;
  one app per branch (prototype and retail) that install side by side.
- **Windows build (MinGW-w64)** — one self-contained `Game.exe` (32- and 64-bit) with `OpenAL32.dll` beside it.
- **64-bit builds** — `native64` (Linux x86_64) and `windows64` run the shipped 32-bit packages unchanged: script object
  references and names are 4-byte indices, script-mirrored headers pack to 4, pointer-sized integers wherever a
  pointer passes through an integer.
- **GCC/Clang portability** — the MSVC 6-era source compiles under GCC and clang: a `<stdint.h>` type layer, no inline
  assembly, ISO for-scope, bytecode operands read by copy, the layout pads that keep the script offsets.
- **Build-433 interface adaptations** — the template layer bent to HP2's 433 interfaces (`DECLARE_CLASS` with a
  package, `FTime` timing, HP2's audio subsystem and process API); the template's static registry replaces HP2's own.
- **Save games** — HP2's slot saves and persistent-actor files work on case-sensitive file systems.
- **Languages** — `appInit` reads the language block in the ini's on-disk case, and `-ini:` overrides win.

## Credits

- **KnowWonder / Electronic Arts** — the Harry Potter and the Chamber of Secrets (UE1 build 433) prototype source this
  engine is built from, under licence from Epic Games.
- **Epic Games** — Unreal Engine 1; the v400 source is the common ancestor the drivers were merged against.
- **engine-ut99** — the build skeleton and the template drivers (SDL2 client, OpenAL driver shape, WebRTC driver,
  IpDrv), which descend from maximqaxd/ut99dc and fgsfdsfgs/UE1.
- **OldUnreal** — XOpenGLDrv (fork `10-p/XOpenGLDrv`; BSD-3). **Chris Dohnal** — UTGLR OpenGLDrv (fork
  `10-p/utglr-copy`), on Daniel Vogel's (Loki) OpenGLDrv. **ptitSeb** — gl4es. **OpenAL Soft** (Chris Robinson, kcat;
  LGPL). **SDL2**. **Xiph.Org** — Ogg, Vorbis. **TooLAME** (the in-tree encoder, from the drop). **Emscripten**.
  **Ghidra** (the retail recovery). **unshield** (twogood).

Harry Potter and the Chamber of Secrets is © Electronic Arts / KnowWonder; Harry Potter characters, names and related
indicia are trademarks of and © Warner Bros. Entertainment Inc.; Unreal Engine is a trademark of Epic Games. This
repository is not affiliated with or endorsed by any of them. Do not use for commercial purposes.

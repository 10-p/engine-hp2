# Building HarryPotter2 (native + WebAssembly)

Harry Potter and the Chamber of Secrets (PC, UE1 build 433), built on the engine-ut99 template. This file
covers how to build and run the port; the reasons behind each convention are in the template's BUILD.md
(`/home/ion/engines/engine-ut99/BUILD.md`, read-only) and in `docs/porting/`. Everything is built and run
inside the workspace `$W` (`~/WorkHP2` here; layout in `docs/porting/BOOTSTRAP-NEW-MACHINE.md`).

## Build matrix (CMake presets — `Source/CMakePresets.json`, run from `Source/`)

| Preset | Output | Use |
|--------|--------|-----|
| `native` | `../build-native/Game/Game`, `../build-native/UCC/ucc-bin` | 32-bit Linux oracle, one static binary; every renderer |
| `wasm` | `../build-wasm/SDLLaunch/SDLLaunch.{js,wasm,data}` + `meta.json` | web dev build (-O2 + DWARF) |
| `wasm-debug` | `../build-wasm-debug/…` | -O0, last resort |
| `wasm-perf` | `../build-wasm-perf/SDLLaunch/…` | what the ufrontend clone consumes (`main` → target `hp2-proto`; `retail`, built in the `$W/HarryPotter2-retail` worktree → `hp2-retail-exp`) |
| `windows` | `../build-windows/Game/Game.exe` | MinGW-w64 (PLAN Phase F) |

```bash
cd Source
cmake --preset native && cmake --build --preset native -j"$(nproc)"
source ~/emsdk/emsdk_env.sh            # sets $EMSDK for the Emscripten toolchain file
cmake --preset wasm-perf && cmake --build --preset wasm-perf -j"$(nproc)"
```

The renderer forks are fetched by branch, like the other engines': XOpenGLDrv `hp2-v433-compat`, utglr-copy `hp2`,
gl4es `fix/cubemap-bind-active-unit` (`Source/CMakeLists.txt`). A dependency that moves and breaks the build is the
signal to update the engine or the branch. To build an UNPUSHED renderer change, point
`-DFETCHCONTENT_SOURCE_DIR_XOPENGLDRV=` / `-DFETCHCONTENT_SOURCE_DIR_UTGLR=` / `-DGL4ES_SOURCE_DIR=` at a local clone;
`meta.json` then records it as `local:<path>`. (Until 2026-10-10 the presets forced the HP2 workspace's sibling
clones.)

A software-only native tree (no GL devices) is handy for bring-up and for parallel work:
`cmake --preset native -B ../build-native-b -DBUILD_XOPENGLDRV=OFF -DBUILD_NOPENGLDRV=OFF`.

## Running the native build

```bash
tools/dev/mkrun-hp2.sh build-native $W/run-native    # run tree (Assets/ is only read)
cd $W/run-native && ./run.sh            # SoftDrv, title level
./run.sh gl Ch1Rictusempra                                   # XOpenGL straight into a level
xvfb-run -a -s "-screen 0 1280x720x24" System/Game -SOFTWARERENDERER -NOMSGBOX -log   # headless
```

`Game.ini`, `User.ini` and `Game.log` land in `System/`; saves in `Save/` (`-SAVESLOT=<n>` → `Save/Slot<n>/`).
New game / continue take the retail shapes `PrivetDr.unr -SAVESLOT=<n>` and `-LOAD=0 -SAVESLOT=<n>`.
`ucc-bin` uses its own `HP.ini`, as retail ucc did. Useful headless checks:
`./ucc-bin Core.SimpleCommandlet`, `./ucc-bin batchexport HGame.u class uc <dir>` (loads the whole HGame closure;
the `[layout]` guard prints any C++/script size mismatch).

## Checks before a commit that touches engine code

- `tools/dev/varargs-census.sh <Pkg>…` — compiles a package with `-Wnon-pod-varargs` enabled (the build
  silences it, and clang turns an `FString` passed through `...` into a runtime trap). Expect 0 hits.
- `python3 tools/dev/analysis/msvcscan.py <files>` — MSVC6 for-scope reads that compile but read an outer,
  uninitialised variable.
- `Source/Engine/Src/EngineLayoutAsserts.cpp` (1,063 offset/size asserts from the shipped `Engine.u`) must hold
  natively and with `-fpack-struct=4`; regenerate with `tools/dev/layout/emit_asserts.py` after any
  `EngineClasses.h` change. Never regenerate `EngineClasses.h` itself (the two layout pads are hand edits).

## The web build

1. `cmake --build --preset wasm-perf` → `build-wasm-perf/SDLLaunch/`. The web boot config is
   `Config/System/web/{Default,DefUser}.ini`, preloaded to `/System/` (see `Config/System/web/README.md`).
2. Headless smoke without assets: `node tools/dev/analysis/smoke-coi-full.mjs build-wasm/SDLLaunch 40` →
   `LoadMap: Entry` then `FailedBrowse` (expected with no packages).
3. In the frontend clone (`$W/ufrontend`, branch `hp2-retail`, which carries both targets; asset paths come from its
   gitignored `.ufront/publish.local.json`). This checkout (`main`) is target `hp2-proto`; the `retail` worktree is
   `hp2-retail-exp` (pass its `--engine-artifact $W/HarryPotter2-retail/build-wasm-perf/SDLLaunch`):
   ```bash
   yarn build --target hp2-proto --engine-artifact $W/HarryPotter2/build-wasm-perf/SDLLaunch   # assembles serve-web/
   yarn serve                       # http://localhost:8080/ with COOP/COEP
   yarn e2e --target hp2-proto      # headless Chromium: boot, SoftDrv, direct map load, config, saves, AutoFOV
   ```
   ⚠ `yarn e2e` reuses ANY server on :8080; when another checkout owns it, run with `UFRONT_E2E_BASE_URL` and a
   Playwright config whose webServer serves this `serve-web/` elsewhere (bootstrap doc §11).
   After changing the web ini, clear the target's `<id>-user-data` store (or the site data): a persisted `Game.ini`
   shadows the preloaded `Default.ini`.
   Configure the web build trees OUTSIDE `yarn` (or pass `-DNODE_EXECUTABLE=$(command -v node)`): configured inside
   `yarn publish-all`, CMake caches yarn's temporary node shim and the next build fails (`/tmp/xfs-*/node: not found`).

## Shared with the other engines of the frontend (2026-10-11, from engine-uc2)

The same commands, settings and log lines in every engine (ufront's UC2 merge; engine-uc2's
`docs/CHEAT-COMMANDS.md` is the reference for the cheats; ported here from engine-ut99, whose code this is):

- **`WHEREAMI`** prints `<x> <y> <z> <yaw> <pitch>` (the pawn's location, one decimal; the view's yaw 0..65535 and
  pitch -32768..32767) on the console, on screen and in the log; **`TELEPORT <x> <y> <z> [<yaw> [<pitch>]]`** puts
  Harry there. Cheats: allowed in a standalone game (HP2 is single-player), refused (logged) on a network client;
  every use logs a `Cheat: ` line. HP2's own `ShowPos` (HPConsole, debug mode) only draws the position on screen.
  `Source/Engine/Src/UnCheats.cpp`, first in `UGameEngine::Exec` (the web's `ue_exec_command` reaches the engine only).
- **`[Engine.GameEngine] FpsLogSeconds=<s>`** (0 = off) / console **`FPSLOG <s>`**: an `FPS:` log line every `<s>`
  seconds (frames, fps, frame ms avg/min/max/p99, the 1% low, the viewport size). `Source/Engine/Src/UnFpsLog.cpp`
  (HP2's `appSeconds()` is an `FTime`: only differences of two are taken).
- **`Init: Language: <ext>`** in the log at start-up: the language this run took (`[Engine.Engine] Language`, read
  after the `-ini:` overrides; the frontend's Language setting; `usa` on retail).
- **The command line holds 4,096 characters** (launcher, UCC, Core's `GCmdLine`): a token that does not fit is
  dropped whole, with a warning once the log is up. It was bounded at 1,023 and cut mid-token silently.
- **`ucc make` saves are deterministic** (`Core/Src/UnObj.cpp`, engine-uc2's S2): stable sorts in `SavePackage`, the
  `.int` files in Windows' (NTFS) order in `CacheDrivers`. (`FindFiles` already matched like Windows here.)
- **Game pads** (`Source/NSDLDrv`, one pad: the first connected): `[NSDLDrv.NSDLClient] UseJoystick` (on by default) is a
  real switch (ufront 2.91, the owner: "on by default, but it can still be disabled"): the game-controller subsystem
  is always up and the viewport reads the key every tick, so the game's own option (`set ini:Engine.Engine.
  ViewportManager UseJoystick …`) closes the pad (every key released) or opens the first one present at once,
  logged `NSDLClient: game pads on|off (UseJoystick)`; ⚠ the `UNSDLClient()` instance constructor used to set
  `UseJoystick`, `ScaleXYZ/RUV` and the dead zones AFTER the ini had filled the object, so `UseJoystick=False` still
  connected the pad and the Scale keys never applied (measured 2026-10-14); the compiled defaults live in
  `StaticConstructor` only now, and the web ini's Scale keys say the 100/100 HP2 has always run with; a pad connected after start opens on `SDL_CONTROLLERDEVICEADDED`
  and closes on `REMOVED`, releasing every pad key (there was no hot-plug, so on the web a pad practically never
  worked); on the web the press that made the browser list the pad, never reported by SDL, is read from
  `navigator.getGamepads()` once at the connect and given to the game (engine-uc2's H-3). Log lines: `NSDLClient:
  game pad connected: <name>` / `disconnected` / `buttons held at the connect given to the game: 0x<mask>`.
  That press is a tap (released at the next tick: SDL never saw it go down, so it never reports its release). The
  sticks and triggers are read from the pad's STATE every tick, not from axis events: SDL's Emscripten driver reports
  no axis values when a pad is opened, so SDL took a stick's first push for its resting point and posted no event for
  it (`SDL_PrivateJoystickAxis`); the state is right (engine-uc2 reads pads the same way). Found by the cross-engine
  E2E (`wasm-shared.spec.js` connect-and-move). (This engine's input time is an `FTime`, so the `FLOAT` time step that
  froze the sticks of engine-ut99 and engine-unreal1 never applied here.)
- **Antialiasing on the web** (ufront 2.92; the launcher's Advanced "Antialiasing" sets XOpenGL's and UTGLR's
  `UseAA`): every GL device draws on **WebGL 2** (`NSDLViewport.cpp`; gl4es's calls run on it unchanged). XOpenGLDrv
  multisamples in framebuffers of its own. OpenGLDrv/UTGLR draws into framebuffer 0, which under
  `-sOFFSCREEN_FRAMEBUFFER` is Emscripten's emulated, single-sample framebuffer: `Source/Game/msaa-backbuffer.js`
  (`ufront_backbuffer_samples(n)`, a `--js-library`, the same as engine-ut99's) swaps it for a multisampled one (RGBA8 +
  DEPTH24_STENCIL8), resolved at the present and before any read of framebuffer 0 (`SHOT`, save thumbnails); UTGLR
  (`10-p/utglr-copy`, branch `hp2`) asks for `NumAASamples` at SetRes when `UseAA` is on, or gives it back. Log
  lines: `AA: 4x multisampled back buffer` / `AA: off`. `-WEBGL=1` gives UTGLR WebGL 1 (no antialiasing), and a
  browser without WebGL 2 falls back to it. Benchmarked (the frontend's opt-in `renderer-benchmark`, steps/2.92):
  WebGL 2 costs nothing (UTGLR: 4% more frames, 4% less CPU a frame).

## Audio (NOpenALDrv)

`Source/NOpenALDrv` is HP2's retail ALAudio ported to openal-soft (OpenAL 1.1; EFX, no EAX), native and web:
compressed sounds are EA-XA ADPCM decoded by `EAXADecoder.{h,cpp}`, music is Ogg Vorbis opened by path
(`../Music/<Song>.ogg`; libogg 1.3.5 + libvorbis 1.3.7 via FetchContent, built without `-fpack-struct` — Init
checks `sizeof(OggVorbis_File)` agrees), dialog carries lip-sync data. Config: `[NOpenALDrv.NOpenALAudioSubsystem]`
with ALAudio's float volumes (`SoundVolume`, `MusicVolume`) plus `OutputRate`/`UseHRTF`/`UseReverb`. Natively
`libopenal.so.1` sits beside `Game` (Windows: `OpenAL32.dll`). The decoder's oracle test (decoded amplitude vs
the lip-sync tables shipped in `AllDialog.uax`):
`cmake --build <native tree> --target eaxa_oracle && <tree>/NOpenALDrv/eaxa_oracle Assets/Sounds` (see the
header of `Source/NOpenALDrv/Test/EAXAOracle.cpp`). Headless runs: `SDL_AUDIODRIVER=dummy` or openal-soft's
`ALSOFT_DRIVERS=null`.

## Windows (MinGW-w64, PLAN Phase F)

```bash
cd Source && cmake --preset windows && cmake --build --preset windows -j"$(nproc)"   # build-windows/Game/Game.exe
tools/dev/mkrun-hp2.sh build-windows $W/run-windows --windows
export WINEPREFIX=$W/.wine-hp2          # keep the prefix inside the workspace
cd $W/run-windows/System && wine Game.exe -SOFTWARERENDERER -NOMSGBOX
```

One self-contained `Game.exe` (`-static-libgcc -static-libstdc++`); its imports must stay within the stock system
DLLs (`i686-w64-mingw32-objdump -p Game.exe | grep "DLL Name"`: ADVAPI32, GDI32, IMM32, KERNEL32, msvcrt, ole32,
OLEAUT32, SETUPAPI, SHELL32, USER32, VERSION, WINMM, WS2_32 — plus `OpenAL32.dll`, copied beside the exe, once
NOpenALDrv is on). The run tree never copies the retail `*.dll`/`*.exe` (a retail `OpenAL32.dll` or `Game.exe`
would shadow ours). MinGW lays bitfields out the MSVC way, so the GCC layout pads in `EngineClasses.h` and
`UnFractal.h` are `#ifndef __MINGW32__`. **Wine's file lookup is case-insensitive: a Windows run proves nothing
about the path/case handling** — test that natively or on the web. `ucc-bin.exe` is not built on MinGW. Stop a
hung prefix with `/usr/lib/x86_64-linux-gnu/wine/wineserver -k`.

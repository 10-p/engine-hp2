# Building HarryPotter2 (native + WebAssembly)

Harry Potter and the Chamber of Secrets (PC, UE1 build 433), built on the engine-ut99 template. This file
covers how to build and run the port; the reasons behind each convention are in the template's BUILD.md
(`/home/ion/engines/engine-ut99/BUILD.md`, read-only) and in `docs/porting/`. Everything is built and run
inside `/home/ion/hp2-ufrontend`.

## Build matrix (CMake presets — `Source/CMakePresets.json`, run from `Source/`)

| Preset | Output | Use |
|--------|--------|-----|
| `native` | `../build-native/Game/Game`, `../build-native/UCC/ucc-bin` | 32-bit Linux oracle, one static binary; every renderer |
| `wasm` | `../build-wasm/SDLLaunch/SDLLaunch.{js,wasm,data}` + `meta.json` | web dev build (-O2 + DWARF) |
| `wasm-debug` | `../build-wasm-debug/…` | -O0, last resort |
| `wasm-perf` | `../build-wasm-perf/SDLLaunch/…` | what the ufrontend clone consumes (`yarn build --target hp2`) |
| `windows` | `../build-windows/Game/Game.exe` | MinGW-w64 (PLAN Phase F) |

```bash
cd Source
cmake --preset native && cmake --build --preset native -j"$(nproc)"
source ~/emsdk/emsdk_env.sh            # sets $EMSDK for the Emscripten toolchain file
cmake --preset wasm-perf && cmake --build --preset wasm-perf -j"$(nproc)"
```

The renderer forks come from the sibling clones in the workspace (`FETCHCONTENT_SOURCE_DIR_XOPENGLDRV` →
`../../XOpenGLDrv`, branch `hp2-v433-compat`; `FETCHCONTENT_SOURCE_DIR_UTGLR` → `../../utglr-copy`, branch
`hp2`; `GL4ES_SOURCE_DIR` → `../../gl4es`). `meta.json` records them as `local:<path>` until they are pushed.

A software-only native tree (no GL devices) is handy for bring-up and for parallel work:
`cmake --preset native -B ../build-native-b -DBUILD_XOPENGLDRV=OFF -DBUILD_NOPENGLDRV=OFF`.

## Running the native build

```bash
tools/dev/mkrun-hp2.sh build-native /home/ion/hp2-ufrontend/run-native    # run tree (Assets/ is only read)
cd /home/ion/hp2-ufrontend/run-native && ./run.sh            # SoftDrv, title level
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
3. In the frontend clone (`/home/ion/hp2-ufrontend/ufrontend`, branch `hp2`; paths come from its gitignored
   `.ufront/publish.local.json`):
   ```bash
   yarn build --target hp2          # assembles serve-web/ (frontend + SDLLaunch.* + manifests + asset symlinks)
   yarn serve                       # http://localhost:8080/ with COOP/COEP
   yarn e2e --target hp2            # headless Chromium: boot, SoftDrv, direct map load, config, saves
   ```
   After changing the web ini, clear the `hp2-user-data` store (or the site data): a persisted `Game.ini`
   shadows the preloaded `Default.ini`.

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
tools/dev/mkrun-hp2.sh build-windows /home/ion/hp2-ufrontend/run-windows --windows
export WINEPREFIX=/home/ion/hp2-ufrontend/.wine-hp2          # keep the prefix inside the workspace
cd /home/ion/hp2-ufrontend/run-windows/System && wine Game.exe -SOFTWARERENDERER -NOMSGBOX
```

One self-contained `Game.exe` (`-static-libgcc -static-libstdc++`); its imports must stay within the stock system
DLLs (`i686-w64-mingw32-objdump -p Game.exe | grep "DLL Name"`: ADVAPI32, GDI32, IMM32, KERNEL32, msvcrt, ole32,
OLEAUT32, SETUPAPI, SHELL32, USER32, VERSION, WINMM, WS2_32 — plus `OpenAL32.dll`, copied beside the exe, once
NOpenALDrv is on). The run tree never copies the retail `*.dll`/`*.exe` (a retail `OpenAL32.dll` or `Game.exe`
would shadow ours). MinGW lays bitfields out the MSVC way, so the GCC layout pads in `EngineClasses.h` and
`UnFractal.h` are `#ifndef __MINGW32__`. **Wine's file lookup is case-insensitive: a Windows run proves nothing
about the path/case handling** — test that natively or on the web. `ucc-bin.exe` is not built on MinGW. Stop a
hung prefix with `/usr/lib/x86_64-linux-gnu/wine/wineserver -k`.

# Building engine-hp2 (native, Windows, WebAssembly, Android)

Everything is built and run inside the workspace `/home/ion/WorkUnreal` (`$W` below): this checkout is
`$W/engine-hp2-retail` (branch `retail`, the retail disc's content), the `main` checkout `$W/engine-hp2`, the frontend
`$W/unreal-frontend-arch-private`. Everything below applies to both branches; the retail values are given here. `source $W/env.sh` first: it puts the workspace's emsdk on the path and sets
`EMSDK`, which the web presets' toolchain file reads.

## Presets (`Source/CMakePresets.json`, run from `Source/`)

| Preset | Output | What it is |
|--------|--------|------------|
| `native` | `build-native/Game/Game`, `build-native/UCC/ucc-bin`, `libopenal.so.1` | 32-bit Linux (`-m32`), one static binary, every renderer |
| `native64` | `build-native64/…` (the same files) | the same for x86_64 (`UE_ALLOW_64BIT`) |
| `wasm` | `build-wasm/SDLLaunch/SDLLaunch.{js,wasm,data}`, `meta.json` | web, `-O2` with DWARF and assertions |
| `wasm-debug` | `build-wasm-debug/SDLLaunch/…` | web at `-O0` |
| `wasm-perf` | `build-wasm-perf/SDLLaunch/…`, `SDLLaunch.js.symbols` | web shipping build (linked `-O3 -g0`, no assertions); what the frontend publishes |
| `windows` | `build-windows/Game/Game.exe`, `OpenAL32.dll`, `build-windows/UCC/ucc.exe` | Windows x86 (MinGW-w64 cross), one self-contained exe, every renderer but NOpenGLESDrv |
| `android-arm64` | `build-android-arm64/Game/libHP2.so`, `_deps/sdl2-build/libSDL2.so`, `libhp2_android_bridge.so` | the game as one shared library for Android arm64 (API 30, shared libc++; needs `ANDROID_NDK_HOME`): the configuration the Gradle project in `Android/` builds into the APK |
| `windows64` | `build-windows64/…` (the same files) | the same for x86_64 |

```bash
cd Source
cmake --preset native && cmake --build --preset native -j"$(nproc)"
cmake --preset wasm-perf && cmake --build --preset wasm-perf -j"$(nproc)"
```

Host packages: the i386 SDL2 and GL development files for `native` (`libsdl2-dev:i386`, `libgl-dev:i386`,
`g++-multilib`), the amd64 ones for `native64`, `mingw-w64` for the Windows presets. The Windows presets fetch SDL2's
and OpenAL Soft's MinGW builds for their architecture (`Source/cmake/SDL2Mingw.cmake`, `OpenALMingw.cmake`); the web
build and native Linux (`UE_BUNDLED_OPENAL`) build the openal-soft fork (`10-p/openal-soft-web`,
`Source/cmake/OpenALSoft.cmake`); NOpenALDrv fetches libogg and libvorbis. No preset needs the game content: the web
link preloads only the tracked web inis (`Config/System/web/`).

The renderer forks are fetched by branch (`Source/CMakeLists.txt`): XOpenGLDrv `10-p/XOpenGLDrv@ufront`,
UTGLR `10-p/utglr-copy@main`, gl4es `10-p/gl4es@ufront`. To build an unpushed change of one,
point `-DFETCHCONTENT_SOURCE_DIR_XOPENGLDRV=`, `-DFETCHCONTENT_SOURCE_DIR_UTGLR=` or `-DGL4ES_SOURCE_DIR=` at a local
clone; `meta.json` then records that dependency as `local:<path>`.

The options this repository defines (`Source/CMakeLists.txt`; the presets set them):

| Option | Default | |
|--------|---------|-|
| `UE_ALLOW_64BIT` | OFF | build x86_64 as a 64-bit engine; OFF demotes an x86_64 compiler to `-m32` |
| `UE_WEB_PERF`, `UE_WEB_DEBUG` | OFF | the web build mode (see the table above) |
| `UE_BUNDLED_OPENAL` | ON | native Linux: build openal-soft and ship `libopenal.so.1` beside the binary |
| `BUILD_XOPENGLDRV`, `BUILD_UTGLR`, `BUILD_SOFTDRV`, `BUILD_NULLDRV` | per preset | the render devices |
| `BUILD_NOPENGLESDRV` | ON (OFF in `windows` and `windows64`) | NOpenGLESDrv, the GLES2 device: native Linux, the web and Android; Windows has no OpenGL ES layer |
| `BUILD_NOPENALDRV`, `BUILD_NULLSOUNDDRV` | per preset | the audio subsystems |
| `BUILD_EDITOR`, `BUILD_STATIC`, `USE_GL4ES` | per preset | the Editor package, one static binary, UTGLR on gl4es |
| `SOFTDRV_MAX_SCREEN` | 4096 (8192 on the web) | SoftDrv's largest viewport dimension |

The Android client (`if(ANDROID AND USE_SDL)` in `Source/CMakeLists.txt`) forces its own set: static, Editor,
XOpenGLDrv, UTGLR through gl4es, NOpenGLESDrv, SoftDrv, NullDrv, NOpenALDrv and the silent audio subsystem, no `ucc-bin`,
`UE_ALLOW_64BIT` (the x86_64 ABI), `ASM=0`, `-fno-delete-null-pointer-checks` and clang's non-POD-varargs leniency;
it defines `UE_ANDROID_APP` for the app's own platform paths.

A software-only native tree (no GL devices) for parallel work:
`cmake --preset native -B ../build-native-soft -DBUILD_XOPENGLDRV=OFF -DBUILD_UTGLR=OFF -DBUILD_NOPENGLESDRV=OFF`.

## Running the native build

```bash
UE_ASSETS_DIR=$W/assets/hp2-retail-exp-assets tools/dev/mkrun-hp2.sh build-native $W/run/hp2-retail-native
cd $W/run/hp2-retail-native && ./run.sh     # SoftDrv, a new game in PrivetDr
./run.sh gl Ch1Rictusempra                  # XOpenGLDrv, straight into a level (null, utglr, gles, soft)
SDL_VIDEODRIVER=dummy System/Game -NULLRENDERER -NOMSGBOX -log    # headless
```

`mkrun-hp2.sh` links the read-only content and copies `System/` (writable), the shipped inis and the binaries.
`Game.ini`, `User.ini` and `Game.log` land in `System/`; saves in `Save/` (`-SAVESLOT=<n>` → `Save/Slot<n>/`). The
boot level is `PrivetDr` (`LocalMap`), so a plain start is a new game; `-LOAD=<n>` continues `Save<n>.usa` when it
exists and otherwise logs `No saved game to continue` and plays the new game. `-DEBUGMODE` turns HP2's debug console
and messages on. `ucc-bin` uses its own `HP.ini`: `./ucc-bin Core.SimpleCommandlet`,
`./ucc-bin batchexport HGame.u class uc <dir>`. SIGTERM, SIGHUP and Ctrl-C ask the main loop to exit cleanly (a
second one is ignored); before the main loop the process ends at once.

`-INPUTTEST=<file>` opens the input test channel (`Source/NSDLDrv/Src/NSDLInputTest.h`): commands appended to the file
drive a virtual SDL game controller that takes the pad's place, press keys, place the pointer, run console commands
and watch Harry and the camera once per input tick. `tools/dev/inputtest/run.sh <run-tree> <scenario> <out-dir>
[<args>]` runs the native build under Xvfb with it and feeds it a scenario (`move.scen`: the move stick's speed against
tilt, its direction and the Walking button, on `GreatHall_G` with `-NULLRENDERER`; `move-retail.scen`: the move stick
and the keys on the retail game's `Entryhall_hub`; `turn.scen`: the camera's turn rate from each stick's X at full and
half tilt and from the Right arrow key; `broom.scen`: the prototype broom's yaw and pitch from the move stick and the
arrow keys in free flight); `rate.py` reads the watch lines.

## Checks before a commit that touches engine code

- The native class layout: `UStruct::Link` stops with a fatal error when a native class's C++ size differs from its
  script size (the `[layoutdiag]` lines before it list the script layout). `Source/Engine/Src/EngineLayoutAsserts.cpp`
  asserts every member offset of `EngineClasses.h` against the retail `Engine.u` at compile time, for the 32-bit
  layout; regenerate it with `tools/dev/layout/emit_asserts.py` from the retail `Core.u`/`Engine.u` after any
  `EngineClasses.h` change (the command is in the file's preamble; `tools/dev/layout/README.md`). `tools/dev/layout/layout-sweep.gdb` compares every script property with its C++
  member in a running engine, on 32-bit and 64-bit builds: expect `shifted=0`.
- `tools/dev/varargs-census.sh <Pkg>…` compiles a package with clang's `-Wnon-pod-varargs` (the build silences it,
  and clang turns an `FString` passed through `...` into a run-time trap). Expect no hit.
- `python3 tools/dev/analysis/msvcscan.py <files>` finds MSVC 6 for-scope reads that compile but read an outer,
  uninitialised variable.

## 64-bit

`native64` and `windows64` run the shipped 32-bit `.u` packages unchanged. The width the code reads is
`PLATFORM_64BITS`, from the compiler (`Core/Inc/Core.h`). What makes it work:

- A script object reference is a 4-byte index into the object table on every width (`UObject::GetScriptObject` /
  `GetScriptRef`; `UStruct::SerializeExpr` converts on load and save), and an `FName` is a 4-byte name-table index,
  so the bytecode and its jump offsets keep their 32-bit shape. Multi-byte bytecode operands are read by copy
  (`ReadScriptOperand`).
- Every header that declares a scripted native class or a script-mirrored struct is `#pragma pack(push,4)`, the
  generated `*Classes.h` and the exporter that writes them included; object properties align to 4; script `int`s that
  stand in for C++ pointers or vtables get their C++ size (`NativePlaceholderSize` in `Core/Src/UnClass.cpp`).
- `PTRINT`/`UPTRINT` wherever a pointer passes through an integer; native registration data waits in pointer-sized
  fields; a None context's zero-fill covers its whole destination and a switch compares whole pointers; the script
  compiler records value sizes in the 32-bit format's sizes (`ScriptFormatStructSize`).
- MinGW keeps `-fpack-struct=4` on i686 only; `windows64` is LLP64 (`long` stays 4 bytes).

`EngineLayoutAsserts.cpp` holds the 32-bit offsets and is skipped on 64-bit; there the fatal size guard and the layout
sweep check the layout. The UTGLR fork rounds its pointers through the engine's `UPTRINT`.

## The web build

1. `cmake --build --preset wasm-perf` → `build-wasm-perf/SDLLaunch/`. The boot config is
   `Config/System/web/{Default,DefUser}.ini`, preloaded to `/System/` (`Config/System/web/README.md`). The ABI shims
   are `vendor/glue/{ue_abi.h,net-shim.js,wasmfs-shim.js}` (ABI version 2, written into `meta.json`, with
   `targetId` `hp2-retail-exp` from `HP2_TARGET_ID` in `Source/CMakeLists.txt`; `main` builds `hp2-proto`).
2. In the frontend (`$W/unreal-frontend-arch-private`; asset paths come from its gitignored
   `.ufront/publish.local.json`):
   ```bash
   yarn build --target hp2-retail-exp --engine-artifact $W/engine-hp2-retail/build-wasm-perf/SDLLaunch
   yarn serve                         # http://localhost:8080/ with COOP/COEP
   yarn e2e --target hp2-retail-exp   # headless Chromium against serve-web/
   ```
   The prototype engine is `--target hp2-proto --engine-artifact $W/engine-hp2/build-wasm-perf/SDLLaunch`.
   `yarn e2e` reuses any server already on :8080; when another checkout owns that port, serve this tree elsewhere
   (`UFRONT_E2E_BASE_URL`, `UFRONT_E2E_SERVE_ROOT`).
3. After changing the web inis, clear the target's `<id>-user-data` store (or the site data): a persisted `Game.ini`
   shadows the preloaded `Default.ini`.
4. Configure the web trees outside `yarn` (or pass `-DNODE_EXECUTABLE=$(command -v node)`): configured inside
   `yarn publish-all`, CMake caches yarn's temporary node shim and the next build fails (`/tmp/xfs-*/node: not found`).

## Commands and log lines shared with the other engines

- **`WHEREAMI`** prints `<x> <y> <z> <yaw> <pitch>` (console, screen and a `Cheat:` log line); **`TELEPORT <x> <y> <z>
  [<yaw> [<pitch>]]`** moves Harry there. Allowed in a standalone game, refused on a network client unless
  `[Engine.GameEngine] AllowNetCheats`. `Source/Engine/Src/UnCheats.cpp`, handled first in `UGameEngine::Exec`.
- **`[Engine.GameEngine] FpsLogSeconds=<s>`** (0 = off) / console **`FPSLOG <s>`**: an `FPS:` line every `<s>` seconds
  (frames, fps, frame ms avg/min/max/p99, the 1% low, the viewport size). `Source/Engine/Src/UnFpsLog.cpp`.
- **`RELAUNCH <arguments>`** (Epic's console command): the game exits, and once it has, its running executable starts
  again with the arguments (Linux and Windows); an Android app closes; on the web it is a fatal error, `Failed to
  launch process. Relaunch: <arguments>`. `Source/Core/Src/UnUnix.cpp`.
- **`Init: Language: <ext>`** at start-up: the language this run took (`[Engine.Engine] Language`, read after the
  `-ini:` overrides).
- **The menu book on a screen that is not 4:3:** while HGame's book is open, the UWindow menus are drawn in the largest
  4:3 box of the screen, centred, and the rest of the screen is black; the pointer is mapped into the box. HPConsole's
  `RenderUWindow` is wrapped natively when the console class loads (`Source/Engine/Src/UnMenuBox.cpp`; the script
  is unchanged). Log line: `Menu book: drawn in the largest 4:3 box of the screen`.
- **`-UFRONTLOG`** also logs the port's diagnostic and repeated lines (`GUfrontLog`): the `[AutoFOV]` lines, the
  `[mouse]` capture hand-overs, NOpenALDrv's `first EA-XA stream` / `first PCM sound` / `first lip-synced line` and
  first reverb zone, the `[WebRTC-LAN]` discovery rounds, and the device information of every render device after
  the process's first (a fullscreen switch re-creates it): the GL strings, XOpenGLDrv's extension list, capability
  lines, VSync state, flushes and shader-compile messages, UTGLR's `GL_EXTENSIONS`, `Device supports` lines and
  buffer and texture limits. The tests pass it; a player's log leaves them out.
  Status lines, warnings, errors and console-command answers are always logged.
- **The command line holds 4,096 characters** (launcher, UCC, Core's `GCmdLine`); a token that does not fit is dropped
  whole, with a warning once the log is up.
- **`ucc make` saves are deterministic**: stable sorts in `SavePackage`, the `.int` files in Windows' order in
  `CacheDrivers`.
- **Fullscreen** (native desktop, `Source/NSDLDrv`). Alt+Enter and `ToggleFullscreen` switch between the window and
  fullscreen (the Enter press does not reach the game), and `[NSDLDrv.NSDLClient] StartupFullscreen` starts in
  fullscreen. With `BorderlessFullscreen` (True, also in `Config/System/Default.ini`) fullscreen is SDL's desktop
  fullscreen: the window covers its display at the display's own resolution, the game renders at that size
  (`NSDLViewport: borderless fullscreen at <w>x<h>`), `FullscreenViewportX/Y` are left as they are, and `SetRes` (the
  video page's resolution) is not applied there (`SetRes <w>x<h> not applied: borderless fullscreen stays at
  <w>x<h>`). With `False` fullscreen is exclusive: the display changes to the mode nearest `FullscreenViewportX/Y`,
  and `SetRes` changes the mode and keeps its size there. Leaving fullscreen gives the window back its size. The web
  and Android builds do not read the key.
- **Game pads** (`Source/NSDLDrv`; one pad, the first connected): `[NSDLDrv.NSDLClient] UseJoystick` (on by default) is
  read every tick, so `set ini:Engine.Engine.ViewportManager UseJoystick False` closes the pad (every key released) and
  `True` opens the first one present; a pad connected later opens on `SDL_CONTROLLERDEVICEADDED`. Sticks and triggers
  are read from the pad's state each tick, and the sticks (the touch move stick on Android too) are given to the
  player once a frame for the frame's own length (`UNSDLViewport::ReadInput`), so a held stick has the same value
  every frame, as a held key does; on the web the press that made the browser list the pad is read from
  `navigator.getGamepads()` and given to the game as a tap. The left stick tilted out of its dead zone (radially;
  each axis keeps its own dead zone, so a push ahead does not turn Harry) but below `[NSDLDrv.NSDLClient] PadWalkTilt`
  (0.5 of the stick's travel; 0 never) walks, as the Walking button does: the stick keeps a bit of its own set in the player's
  `bRun` (the Walking key writes the whole byte; the stick sets its bit again the next tick). The compiled defaults
  (Scale, dead zones) are in `UNSDLClient::StaticConstructor`. Log lines: `NSDLClient: game pad connected: <name>`, `disconnected`, `buttons held
  at the connect given to the game: 0x<mask>`, `game pads on|off (UseJoystick)`.
- **Render device** (`Source/Game/Src/Launch.cpp`). `-XOPENGLRENDERER`, `-UTGLRRENDERER`, `-NOPENGLESRENDERER`,
  `-SOFTWARERENDERER` and `-NULLRENDERER` select a device this build contains (an absent one is logged and ignored).
  Without a switch the persisted `GameRenderDevice` is kept when it is a hardware default or the user's marked pick
  (`[Engine.Engine] ufrontUserRenderDevice`); else XOpenGL, then UTGLR; SoftDrv when the build has neither. A switch
  whose device is still the game device after `Engine->Init` is persisted with the marker. The launcher writes the
  choice to `GameRenderDevice`, `WindowedRenderDevice` and `RenderDevice`.
- **Software fallback** (`Source/NSDLDrv/Src/NSDLViewport.cpp`). A render device that cannot start (a window or GL
  context the driver cannot give, no WebGL context in the browser, or a failed Init) gives way to SoftDrv for the rest
  of the run, in a window without GL: `Render device <device> cannot start: falling back to
  SoftDrv.SoftwareRenderDevice for this run`, once. `ToggleFullscreen` keeps SoftDrv. Nothing is written to the ini, so
  the next start tries the chosen device again.
- **NOpenGLESDrv** (`Source/NOpenGLESDrv`, `-NOPENGLESRENDERER`, never a default): the GLES2 device on WebGL 2 (WebGL 1
  with `-WEBGL=1` or without WebGL 2), on Android's GLES 2 context and natively on the desktop GL context, with HP2's
  render face: `DrawTriangles` (indexed lists, fans, plain lists; `MaxVertices()` 256), the 5-argument
  `DrawComplexSurface` (a Lumos surface is straight alpha at opacity 1 - cAlpha/255, with the depth writes Render
  chose), `PF_Highlighted|PF_Translucent` as straight alpha with the vertex alpha, alpha-palette tiles premultiplied,
  palette alpha kept, `PF_NoSmooth` taken per draw. Options in `[NOpenGLESDrv.NOpenGLESRenderDevice]` (`NoFiltering`,
  `Overbright`, `UseVAO`, `UseBGRA`, `MaxAnisotropy`, `NumAASamples`, `SwapInterval`, and URenderDevice's
  `DetailTextures`, `HighDetailActors`, `VolumetricLighting`, `ShinySurfaces`, `Coronas`), with compiled defaults
  for an ini without the section. Log lines: `Got OpenGL <version>`, `NOpenGLES: default framebuffer depth <n> bits`,
  `GLES2: anisotropic filtering <n>x`.
- **Antialiasing on the web** (`UseAA` and `NumAASamples` of XOpenGL and UTGLR, NOpenGLES's `NumAASamples`; the
  launcher's Advanced switch): every GL device draws on WebGL 2. XOpenGLDrv multisamples in framebuffers of its own;
  NOpenGLESDrv and UTGLR draw into framebuffer 0, which `Source/Game/msaa-backbuffer.js` (`ufront_backbuffer_samples(n)`)
  replaces with a multisampled RGBA8 + DEPTH24_STENCIL8 one, resolved at the present and before any read of
  framebuffer 0 (`SHOT`, save thumbnails). Log lines: `AA: 4x multisampled back buffer` / `AA: off`, and NOpenGLES's
  `GLES2: antialiasing 4x (multisampled back buffer)` / `GLES2: antialiasing off`. `-WEBGL=1`, or a browser without
  WebGL 2, gives NOpenGLESDrv and UTGLR WebGL 1 without antialiasing. Natively and on Android the context is made
  before the device starts, and NOpenGLES only logs `NumAASamples`.

## Audio (NOpenALDrv)

`Source/NOpenALDrv` is HP2's retail ALAudio ported to openal-soft (OpenAL 1.1, EFX, no EAX), natively and on the web.
Compressed sounds are EA-XA ADPCM, decoded by `EAXADecoder.{h,cpp}`; music is Ogg Vorbis opened by path
(`../Music/<Song>.ogg`; libogg 1.3.5 and libvorbis 1.3.7, built without `-fpack-struct`, and Init checks that
`sizeof(OggVorbis_File)` agrees); dialog carries lip-sync data. Config: `[NOpenALDrv.NOpenALAudioSubsystem]` with
ALAudio's float volumes (`SoundVolume`, `MusicVolume`) plus `OutputRate`, `UseHRTF`, `UseReverb`. The decoder's
oracle compares the decoded amplitude with the lip-sync tables shipped in `AllDialog.uax`:
`cmake --build <native tree> --target eaxa_oracle && <tree>/NOpenALDrv/eaxa_oracle <assets>/Sounds` (the header of
`Source/NOpenALDrv/Test/EAXAOracle.cpp`). Headless: `SDL_AUDIODRIVER=dummy` or openal-soft's `ALSOFT_DRIVERS=null`.

## Android

`Android/` is a Gradle/NDK project (Android Gradle plugin 9.4.0, Gradle 9.6.0 through the wrapper, its daemon on
an installed JDK 17) whose `externalNativeBuild` points at `Source/CMakeLists.txt` (targets `Game`, the game as
`libHP2.so`, and `hp2_android_bridge`), built for `armeabi-v7a`, `arm64-v8a` and `x86_64` in one APK (x86
Chromebooks and the emulator run the last). It needs platform 36 (`compileSdk`; `targetSdk` 34, `minSdk` 24), build-tools
36.0.0, NDK 28.2.13676358 and CMake 4.1.2 or newer. Ubuntu's packages install the SDK into `/usr/lib/android-sdk`;
`local.properties` names it and the distribution's CMake:

```bash
sudo apt install openjdk-17-jdk cmake ninja-build adb google-android-cmdline-tools-19.0-installer \
  google-android-platform-36-installer google-android-build-tools-36.0.0-installer google-android-ndk-r28c-installer
printf 'sdk.dir=/usr/lib/android-sdk\ncmake.dir=/usr\n' > Android/local.properties
```

```bash
cd Android
./gradlew :app:assembleRelease      # -> app/build/outputs/apk/release/app-release.apk (assembleDebug: debuggable)
#   -Psdl2Dir=<path>     an SDL2 checkout (release-2.32.x); else one is cloned into Android/build/SDL2
#   -PnativeDebug        the engine at the variant's own CMAKE_BUILD_TYPE (every variant is RelWithDebInfo otherwise)
#   -PxopenglDir=<path>  a local XOpenGLDrv clone instead of the fetched branch (FETCHCONTENT_SOURCE_DIR_XOPENGLDRV)
adb install -r app/build/outputs/apk/release/app-release.apk
```

- Signing: every variant is signed with the key the Gradle property `ueAndroidSigning` names, a properties file kept
  outside the repository (`storeFile`, `storePassword`, `keyAlias`, `keyPassword`), set in `~/.gradle/gradle.properties`
  or as `UE_ANDROID_SIGNING`. One key signs every release, so each APK installs over the last; without the property
  the build uses the debug key.
- The two branches build two apps that install side by side, from `HP2_TARGET_ID` in `Source/CMakeLists.txt`
  (`Android/app/build.gradle` reads it): `main` is `com.hp2.android`, "HP2 Prototype", game folder `HP2`;
  `retail` is `com.hp2.android.retail`, "HP2 Retail", game folder `HP2-Retail`. The Java package (and so the JNI
  names) is `com.hp2.android` on both.
- SDL2 is one checkout for both halves: the C library the engine's CMake builds (`FETCHCONTENT_SOURCE_DIR_SDL2`,
  shared, `libSDL2.so`) and the Java `SDLActivity` compiled from `android-project/`.
- openal-soft (OpenSL ES, no C++20 modules, no RTKit), libogg and libvorbis are linked statically into `libHP2.so`.
- The engine runs on a thread of its own with an 8 MB stack (`SDL_main`, `Source/Game/Src/Launch.cpp`); the log
  is on logcat (tag `HP2`, with stdout and stderr as `HP2-stdio` and the bridge as `HP2Bridge`) and in the game
  folder's `System/Game.log`.
- The inis: `tools/dev/sync-android-ini.py` writes `Config/System/android/{Default,DefUser}.ini` from the web
  pair (`--check` verifies them; the deltas are `NetworkDevice=IpDrv.TcpNetDriver` and XOpenGLDrv `UseVSync=On`);
  the APK carries them as assets, and the game installs them into the folder's `System/` before each start when
  they differ. The engine derives `Game.ini` and `User.ini` from them when those are missing.

The engine reads its game folder (the asset tree: `System/`, `Maps/`, `Textures/`, `Sounds/`, `Music/`, `Save/`)
by path; the bridge (`Android/app/src/main/cpp/android_bridge.cpp`) sets `HP2_ANDROID_BASEDIR`, `HOME` and the
working directory before SDL starts `SDL_main`, and the launcher (`MainActivity`) chooses which folder:

- Internal storage (`/data/data/<applicationId>/files/<folder>`): fast, deleted with the app, no permission.
  "Choose the HP2 folder" opens Android's folder picker, which gives the app that folder only, and "Copy to
  internal storage" copies it in (only what is new or changed: a stopped copy resumes); with no folder chosen yet
  it opens the picker first. "Copy settings and saves" writes the internal copy's `System/Game.ini`,
  `System/User.ini` and `Save/` back to the chosen folder.
- A game folder in place: `Android/data/<applicationId>/files/<folder>` on shared storage or an SD card needs no
  permission; `/sdcard/<folder>` or `<folder>` at the top of an SD card needs All files access (the storage
  permission before Android 11), which the launcher asks for, with the copy as the alternative, only when the game
  is started from there. Shared storage is a FUSE mount and the engine's case-insensitive file lookup is slow
  there; the internal copy is the fast choice.
- The launcher's resolution scale (0.1 to 1) sizes SDL's surface buffer (`SurfaceHolder.setFixedSize`), which
  Android scales to the screen; the engine sees the scaled size as its window. SDL's surface view (`FrameSurface`)
  scales every touch and mouse position from the screen to that buffer before SDL reads it, so a tap lands where it
  is drawn at every scale.
- The launcher's renderer selection, at the bottom of its page (the game's choice, then XOpenGL, UTGLR, NOpenGLES and
  Software; Null for debugging), adds that render device's `-*RENDERER` switch after any command line the launcher was started
  with. The game keeps the device it last started with, also under "The game's choice": a device whose GPU draws the
  default wrong is switched once. UTGLR draws through gl4es on an OpenGL ES 2 context.
- gl4es, UTGLR's GL layer, starts without a precompiled-shader archive (`LIBGL_NOPSA`: nothing is written into the
  home folder), and on the Android emulator (`ro.kernel.qemu` 1, or `ro.hardware` goldfish or ranchu) with
  `LIBGL_NOBGRA`, so gl4es swizzles BGRA uploads itself (`OpenGLDrv: Android emulator detected -> LIBGL_NOBGRA=1` in
  the log). `Source/Game/Src/Launch.cpp` sets both before the render device starts.

Controls: a pad through NSDLDrv's game-controller path (`UseJoystick`, the pad layout of `DefUser.ini`: the look
stick turns and tilts the camera as the mouse does, the move stick moves Harry and its X strafes); touch
as the mouse in the menus (the book, the cut-scene console and the vendor bar: tap to click, the cursor at the
finger); in play an on-screen overlay (a move stick on the left half, drag to look on the right, CAST, JUMP and
MENU buttons) that drives the engine through `Source/NSDLDrv/Src/NSDLAndroid.cpp` on the mouse and pad keys
`DefUser.ini` binds. The move stick tilts fully as the launcher's "Touch move stick" settings say: full speed at a
drag of 12 dp from where the finger is held (4 to 60), a dead zone of 3 dp for a resting finger and one of 3 dp for
each axis, so a push ahead does not strafe (each 0 to 10, always below full speed), saved with the launcher's other settings and
handed to the game when it starts (`GameActivity.EXTRA_MOVE_FULL_TILT_DP`, `EXTRA_MOVE_DEAD_ZONE_DP`,
`EXTRA_MOVE_AXIS_DEAD_ZONE_DP`); past full tilt the held point follows the finger. It never walks. A real pad's
button, or a stick, trigger or hat pushed half way (Android's input events,
`GameActivity.dispatchKeyEvent`/`dispatchGenericMotionEvent`), hides the overlay; a touch on the screen shows it again
(in play that touch does nothing else; in a menu it is still the menu's mouse click). The system's Back
key is Escape (it opens and closes the book). The prototype's title level
is play, not a menu: its Start is the spell cursor's target (drag on the right half moves the cursor, CAST
clicks), as with a mouse. XOpenGLDrv draws without clip planes on the ES profile, as on the web.

Gyro aiming is the launcher's "Gyro aiming" switch (off by default) and its horizontal and vertical sensitivities (0.1
to 5, 1 by default), saved with the other settings and handed to the game when it starts (`GyroAim.EXTRA_GYRO_AIM`,
`EXTRA_GYRO_YAW_SCALE`, `EXTRA_GYRO_PITCH_SCALE`). A sensitivity is view degrees per device degree: at 1 the camera
turns as far as the device. `GyroAim.java` reads the gyroscope and gravity while the game is in front, in either
landscape: tilting the device's top edge towards the player looks up; left and right is the turn about the world's
vertical ("player space": turning the device, and twisting it like a wheel once it is tilted back), never more than
the device's own turn and twist. HP2's camera turns only with the mouse's axes, so in play `NSDLAndroid.cpp` adds the
turn to `aMouseX` and `aMouseY` in whole rotation units of the camera (65536 a turn, the rest carried to the next
tick), past ReadInput's 20 / DeltaTime, PlayerInput's `MouseSensitivity` and field of view and the camera's
`CurrentSet.fRotSpeed` (none where the camera takes no mouse input), and against `bInvertMouse`: the mouse's settings
do not change it. A narrower field of view slows it as it slows the mouse: the turn is times `DesiredFOV / DefaultFOV`
(the duel camera's 50 degrees, a level's FOV change; 1 at the player's own field of view, at any aspect ratio, since
AutoFOV widens only the drawn view). PlayerInput's two-tick average of the mouse keeps the total. While a menu is up,
or over a load (the game not ticking for half a second), the turn is dropped. There is no smoothing, acceleration or
dead zone of the gyro's own. With `-UFRONTLOG` the log has the totals once the gyro rests (`Gyro: <yaw> degrees right
and <pitch> up in play in all, <n> yaw and <n> pitch units (zoom <z>)`: the degrees before the zoom, the units after
it, the zoom of the last turn).

In the background (the home screen, the recent apps, another app on top) the game is suspended. SDL's activity
pauses on `onStop`, and its event pump then blocks the game thread until the app returns, so nothing ticks;
NOpenALDrv pauses its OpenAL device (`alcDevicePauseSOFT`, ALC_SOFT_pause_device) on SDL's
`SDL_APP_DIDENTERBACKGROUND`, just before the block, so openal-soft's mixer and the OpenSL ES output stop and no
sound plays, and resumes it on `SDL_APP_WILLENTERFOREGROUND`; the overlay stops redrawing while it cannot be seen.
Back in the foreground SDL restores the GL context on the new surface and the game goes on where it was
(`NOpenALDrv: output paused (the app is in the background)` / `output resumed` in the log).

A command line can be passed from adb; the launcher's countdown then starts the game without a touch:

```bash
adb shell am start -n com.hp2.android/.MainActivity --es com.hp2.android.ARGS 'Ch1Rictusempra -SAVESLOT=1'
adb logcat -s HP2 HP2-stdio HP2Bridge SDL
```

`GameActivity` is not exported; launch `MainActivity`. The same native configuration without the APK is the
`android-arm64` preset (`ANDROID_NDK_HOME` set; pass `-DFETCHCONTENT_SOURCE_DIR_SDL2=<checkout>` to reuse the
Gradle project's SDL2).

## The retail content (this branch)

The content is the retail disc's installed tree, `$W/assets/hp2-retail-exp-assets`
(`tools/dev/retail/assemble-assets.sh <iso> <work-dir> <out-dir> <unshield>` builds one from the disc image; the
layout is in `docs/porting/retail-assets.md`). Both ini sets boot `LocalMap=PrivetDr.unr` with `Language=usa` (the US
dialog recording and the `.usa` text files; every other key falls back to `.int`). What this branch adds to `main`:

- Command line: `GAMESTATE=<token>` overrides the player's `CurrentGameState` when the level's actors are screened.
- Console: `SHOWCOLL 1|0` (collision cylinders) and `SHOWPATHS 1|0` (interpolation and patrol paths, hidden path
  markers included); `SETVOLUMES SOUND=<f> MUSIC=<f>`, `MUTESOUNDS` / `UNMUTESOUNDS` (HGame sends them while a
  cut-scene is skipped) and `SOUNDSOURCESTATES`; `PLAYINGSOUNDSTATS` lists each voice's priority. The Delete key's
  special pause works only with `-DEBUGMODE`.
- Saves: `Save0.bmp` is copied from `System/Thumbnails/<Level>.bmp` (else `Default.bmp`); every persistent-actor record
  carries its actor's class, so an actor the save holds but the map lacks is respawned.
- Audio: NOpenALDrv follows the retail ALAudio (no occlusion, an ambient pass every 0.2 s with at most three voices per
  ambient sound, music stopped on a map change).
- `tools/dev/retail/`: `layout-diff.py` (script layouts, prototype vs retail packages), `func-diff.py` (exported
  functions of two DLL builds), `ghidra/` (headless Ghidra scripts and `match.py`, which pairs two function dumps) and
  `ucdis.py` (a bytecode disassembler for the script-stripped retail packages, with `natives.txt`). Each header says
  how to run it.

## Windows (MinGW-w64)

```bash
cd Source && cmake --preset windows && cmake --build --preset windows -j"$(nproc)"     # or windows64
UE_ASSETS_DIR=$W/assets/hp2-retail-exp-assets tools/dev/mkrun-hp2.sh build-windows $W/run/hp2-retail-windows --windows
export WINEPREFIX=$W/.wine-hp2          # keep the prefix inside the workspace
cd $W/run/hp2-retail-windows/System && wine Game.exe -SOFTWARERENDERER -NOMSGBOX
```

One self-contained `Game.exe` (`-static-libgcc -static-libstdc++`, winpthread linked in). Its imports must stay within
the system DLLs (`<triple>-objdump -p Game.exe | grep "DLL Name"`: ADVAPI32, GDI32, IMM32, KERNEL32, msvcrt, ole32,
OLEAUT32, SETUPAPI, SHELL32, USER32, VERSION, WINMM, WS2_32) plus `OpenAL32.dll`, copied beside the exe. The run tree
never copies the retail `*.dll`/`*.exe` (a retail `OpenAL32.dll` or `Game.exe` would shadow ours). MinGW lays
bitfields out the MSVC way, so the GCC layout pads in `EngineClasses.h` and `UnFractal.h` are `#ifndef __MINGW32__`.
At 64 bits a WinSock `SOCKET` is 8 bytes: `InternetLink`'s script `int Socket` holds it truncated and
`AInternetLink::GetSocket()` reads it back sign-extended (a WinSock handle has 32 significant bits).
Wine's file lookup is case-insensitive, so a Windows run says nothing about the path and case handling; test that
natively or on the web. Stop a hung prefix with `wineserver -k`.

`ucc.exe` is the console tool, the retail UCC's name, with the system DLLs only (no `OpenAL32.dll`): it reads its
arguments and its own `HP.ini` as `ucc-bin` does on Linux and writes `ucc.log` (`wine ucc.exe help`), and the console's
Ctrl-C (a SIGINT to the wine process) asks it to exit cleanly. `mkrun-hp2.sh --windows` copies it beside `Game.exe`.

## CI

`.github/workflows/build.yml` builds `native` and `native64` (Linux), `windows` and `windows64` (MinGW-w64) and `wasm`
on every push and pull request and uploads the binaries; a push to `main` replaces the rolling `latest` release.

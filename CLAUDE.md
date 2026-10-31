# engine-hp2 — agent guide

The private engine repository of **Harry Potter and the Chamber of Secrets (PC, 2002)**, **Unreal Engine 1 build 433**
(`Source/Core/Inc/UnObjVer.h`: `ENGINE_VERSION 433`, `PACKAGE_FILE_VERSION` set at run time), ported to Linux, Windows
(MinGW-w64) and the web. It holds engine source only and produces the WebAssembly artifact (and native binaries). The
frontend (`/home/ion/WorkUnreal/unreal-frontend-arch-private`, targets `hp2-proto` and `hp2-retail-exp`, published
with the other games but unlisted) consumes the artifact and owns the host page, the UI, the build assembly and the
browser glue behind the thin ABI shims vendored in `vendor/glue/` (`UE_ABI_VERSION 2`).

The build skeleton, folder layout, presets, renderers (XOpenGLDrv for WebGL 2, OpenGLDrv/UTGLR on gl4es, the in-tree
NOpenGLESDrv and SoftDrv), helper drivers (NSDLDrv, NOpenALDrv, NullDrv, SoundDrv, WebRTCDrv) and web inis follow `engine-ut99`
(`/home/ion/WorkUnreal/engine-ut99`); this file records where HP2 differs. Build and run: `BUILD.md`.

## The workspace and the two branches

| Thing | Path | Role |
|-------|------|------|
| This repository | `/home/ion/WorkUnreal/engine-hp2` (branch `main`) | the prototype engine, target `hp2-proto` |
| Its retail checkout | `/home/ion/WorkUnreal/engine-hp2-retail` (branch `retail`) | the retail engine, target `hp2-retail-exp` |
| The frontend | `/home/ion/WorkUnreal/unreal-frontend-arch-private` (branch `main`) | `targets/hp2-proto/`, `targets/hp2-retail-exp/` |
| The Android app | `Android/` | the Gradle/NDK project: `libHP2.so` from `Source/CMakeLists.txt`, the launcher and the touch overlay (`BUILD.md`, Android); one app per branch, from `HP2_TARGET_ID` |
| Renderer forks | `10-p/XOpenGLDrv@ufront`, `10-p/utglr-copy@main`, `10-p/gl4es@ufront` | fetched by CMake by branch; point `FETCHCONTENT_SOURCE_DIR_XOPENGLDRV` / `FETCHCONTENT_SOURCE_DIR_UTGLR` / `GL4ES_SOURCE_DIR` at a local clone to build an unpushed change |
| Game content | `/home/ion/WorkUnreal/assets/hp2-proto-assets`, `…/hp2-retail-exp-assets` | never in git; read-only images with a writable overlay (`assets/mount-assets.sh` after a reboot) |
| The baseline | `/home/ion/WorkUnreal/epic-sources/hp2-ue1-433-proto` (and `ut99-v400`, the three-way merge base) | the drop this engine is a modification of; see `epic-sources/README.md` |

**`retail` holds retail-specific changes only.** A general engine change goes to `main`, and `retail` then takes it
by a merge (`git fetch origin && git merge origin/main` on `retail`), never by a rebase: it is a pushed branch. Never
commit a general change to `retail`. The
frontend's `publish-all` refuses a `retail` checkout that does not contain `origin/main` (`publish.engine.baseBranch`).
Values that differ per branch: `HP2_TARGET_ID` (`Source/CMakeLists.txt`: `hp2-proto` here, `hp2-retail-exp` on retail;
meta.json's `targetId` and the WebRTC peer prefix) and the content the inis and `EngineLayoutAsserts.cpp` describe.

Features are listed in the frontend's `/home/ion/WorkUnreal/unreal-frontend-arch-private/docs/14-change-inventory.md`;
a new feature adds its line there. The baseline for "what we changed" is the Epic/KnowWonder drop in
`~/WorkUnreal/epic-sources/` (see its README). The records of the port (`docs/porting/**`) are history, not
instructions.

## Layout

```
Source/        every source folder of the drop (C++ packages and UnrealScript Classes/) plus the CMake build;
               the Windows-only originals (WinDrv, Window, D3DDrv, UnrealEd, Launch, FrontEnd, DebuggerLaunch,
               PackageTool, CutCompiler, Setup, Gesture*, EdSrc, Tools, ALAudio) are kept for reference, not built
Config/System/ the shipped Default.ini and DefUser.ini; web/ is the web boot set (preloaded to /System);
               android/ is generated from web/ by tools/dev/sync-android-ini.py (the APK installs it)
Thirdparty/    the closed and vendored libraries the original build linked (no target links them)
vendor/glue/   ue_abi.h, net-shim.js, wasmfs-shim.js from @ufront/glue (ABI version 2)
tools/         developer tools (each script's header says how to run it; tools/dev/layout/README.md)
docs/          SOURCE-LAYOUT.md (drop → repository mapping) and the porting records
```

## How HP2 differs from the template

- **Build 433, 436-era interfaces.** `DECLARE_CLASS` takes the package name. `URenderDevice` (`Engine/Inc/UnRenDev.h`)
  has a 5-argument `DrawComplexSurface(…, DWORD PolyFlags, BYTE cAlpha)`, pure `MaxVertices()` and `DrawTriangles(…)`;
  every render driver (the forks, NOpenGLESDrv, NullDrv, SoftDrv) implements that face. `appSeconds()` returns an `FTime`
  (32.32 fixed point): take differences, and pass `.GetFloat()` to a `%f`.
- **`UAudioSubsystem` (`Engine/Inc/UnAudio.h`) is HP2's own:** flagged, faded and prioritised `PlaySound`,
  `PlayMusic(FString Song, FLOAT FadeIn)` opening Ogg Vorbis files from `Music/` by path (no `.umx`), lip-sync data.
  Compressed sounds are EA-XA ADPCM (15-byte frames, 28 samples); plain sounds are 22050 Hz mono PCM; dialog is
  `AllDialog.uax`, loaded on demand.
- **No IpDrv, UWeb, IpServer or UBrowser in the drop**; HP2 is single-player. IpDrv comes from the template, built
  without the master/update-server commandlets; WebRTCDrv is the web's net driver, and the frontend keeps P2P off.
- **Closed binaries** (`Thirdparty/`: `coda`, `MpegLib`, `s3tc`, `ENCVAG`) are linked on no target: the EA-XA decoder
  is in-tree, the S3TC encoder is editor-only and every DXT1 texture has P8 mips.
- **Static linking uses the template's `UNREAL_STATIC` registry**; HP2's own `__STATIC_LINK` scheme stays undefined.
- **Layout under GCC/clang:** `ABrush→AMover` and `ANavigationPoint→APlayerStart` need a 3-byte pad in `EngineClasses.h`
  (`#ifndef __MINGW32__`: MinGW lays bitfields out the MSVC way), `UIceTexture` a `MoveIcePad`, the Fire textures
  `GCC_ALIGN(4)`. A native class whose C++ size differs from its script size is a fatal error at load.
- **64-bit:** script object references and `FName`s are 4-byte indices, script-mirrored headers are
  `#pragma pack(push,4)`, script `int`s that stand in for C++ pointers get their C++ size in `UStruct::Link`
  (`NativePlaceholderSize`). `BUILD.md` lists the mechanisms.
- **Content paths are Windows paths** (`Cutscenes\<file>.int`, `..\music\<Song>.ogg`, `"..\\Save"`): the Unix file
  manager folds `\` to `/` and resolves case per component; the web glue does not fold case under WasmFS, so this is
  engine work.
- **Line endings:** the drop was CRLF, the repository is LF; diff against the drop with `--strip-trailing-cr`.
- **Android** is the SDL client as a shared library (`UE_ANDROID_CLIENT` in CMake, `UE_ANDROID_APP` in the code
  for the app's own paths: the game folder from `HP2_ANDROID_BASEDIR`, the log on logcat); `__ANDROID__` guards
  what every Android build shares. Touch stays a mouse for the menus (the player's `bShowMenu` or the viewport's
  `bShowWindowsMouse`, as `UGameEngine::Tick` releases the pointer); the overlay's input enters through
  `Source/NSDLDrv/Src/NSDLAndroid.cpp`. The Android inis are generated (`tools/dev/sync-android-ini.py`), never
  edited by hand. The retail app differs from the prototype one only through `HP2_TARGET_ID`.

## Rules

- Comments explain the code as it is, in one or two lines; keep the rare load-bearing note. No stage markers, work
  ids, fix stories, report sources, owner quotes, dates, commit hashes, cross-references to other files' comments or
  other repositories, glyphs or all-caps emphasis. A platform statement lives inside the `#ifdef` it belongs to. A
  comment that contradicts its code is a bug. Epic's, KnowWonder's and the upstream forks' comments are left alone.
  Log text is not a comment: keep its wording (the frontend's E2E matches some lines).
- Docs describe the current state in present tense: no phases, history, dates as narrative, owner attributions or
  measurements as proof. Rewrite a doc from the code; records (`docs/porting/**`) stay as they are. Every path, flag,
  preset and script a doc names must exist.
- Names follow behaviour. Shared CMake options take the `UE_` prefix; engine-specific ones keep theirs. Do not rename
  ini sections or keys players may have saved, the `WebRTCDrv` package, `UWebRTCNetDriver`, the `NetworkDevice=`
  value, exported ABI symbols, or log lines the frontend matches.
- Commit subjects name the feature (`fix(core): …`, `feat: …`, `build: …`), never a stage number or a date.
- Do not change the ABI (`vendor/glue/`) by hand: it is vendored from `@ufront/glue`, and the engines and the glue
  move together.
- Do not bake heavy glue (FetchFS, IDB, WebRTC logic) into the wasm: only the thin shims in `vendor/glue/`.
- Do not commit game content or build trees (gitignored).
- Do not build the other engines from here, or this one from the other repositories.
- Do not write, build or run anything outside `/home/ion/WorkUnreal` (run trees, wine prefix, publish output and
  screenshots included).
- Do not use `git commit -a` / `git add -A` while another agent works in this tree: commit by explicit path.
- Platform keys: `#if _MSC_VER` is the MSVC compiler, `#ifdef PLATFORM_WIN32` Windows the OS, `#if __GNUG__` the GNU
  family (MinGW included), `PLATFORM_64BITS` the pointer width. Never add `#ifdef WIN32`/`_WIN32`/`_WINDOWS` to
  engine code.
- POSIX-only calls (`fork`, `waitpid`, `sigaction`, `mkdir` modes, …) live in `Core/Src/UnUnix.cpp` behind
  `#ifndef PLATFORM_WIN32`, with the Win32 spelling in the `#else` or in `UnMinGW.cpp`. New file I/O is binary
  (`"rb"`/`"wb"`); path literals use `PATH_SEPARATOR` or `/`.
- `EngineClasses.h` is generated; its hand edits are the layout pads (`bColoredPad`, `bTwoWayPad`), the
  `PTRINT` members (`AStatLog.Context`, `AStatLogFile.LogAr`; `AInternetLink.PrivateResolveInfo` in `IpDrvClasses.h`)
  and the unconditional `#pragma pack`. Regenerate
  `EngineLayoutAsserts.cpp` with `tools/dev/layout/emit_asserts.py` after any change to it, and run
  `tools/dev/layout/layout-sweep.gdb` on a 32-bit and a 64-bit build.
- Do not subtract pointers that may point into different buffers (GCC compiles it as an exact division); use a byte
  difference and check it.

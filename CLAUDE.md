# HarryPotter2 (engine-hp2) — agent guide

**What this is:** the **private** engine repo for **Harry Potter and the Chamber of Secrets (PC, 2002)**,
**Unreal Engine 1 build 433** (`Source/Core/Inc/UnObjVer.h`: `ENGINE_VERSION 433`, `PACKAGE_FILE_VERSION`
runtime-set), web port. It holds engine source only and produces a **WASM artifact** (+ native binaries).
It is **not** the product on its own — ufrontend (`10-p/unreal-frontend-arch-private`, branch `main` since 2026-10-10,
targets `hp2-proto` + `hp2-retail-exp`, published with the other four games but unlisted) consumes this engine's WASM artifact, owns the host page / UI / build assembly, and owns the
browser glue behind the thin **ABI shim** vendored at `vendor/glue/` (`ABI_VERSION 1`).

Templated on **`/home/ion/engines/engine-ut99`** (UT99 v400 → web, base import maximqaxd/ut99dc): the same
build system (CMake + gcc/emcc/MinGW), folder organisation, build targets, renderers (XOpenGLDrv for
WebGL2, OpenGLDrv/UTGLR + gl4es for WebGL1, the in-tree SoftDrv), helper drivers (NSDLDrv, NOpenALDrv,
NullDrv, SoundDrv null), web-friendly inis and the WebRTC net driver. Read that repo's `CLAUDE.md` and
`BUILD.md` for the reasoning behind every convention; this file records only where HP2 differs.

## Start here
`$W` = the workspace that holds every repo side by side: **`/home/ion/WorkUnreal`** since 2026-10-10, where this repo is
cloned twice beside the other engines — `engine-hp2` (`main`) and `engine-hp2-retail` (`retail`), the same arrangement
as `engine-ut99` / `engine-ut99-436-compat`. LOG/PLAN entries and older reports use `~/WorkHP2` and
`/home/ion/hp2-ufrontend` (the HP2 port's own workspaces before that).

**★ `retail` is for retail-SPECIFIC changes ONLY** (the UT99 v436 rule, applied here on 2026-10-10). A general engine
change goes to `main`, and `retail` is then rebased on top (`git fetch origin && git rebase --onto origin/main
<old-base>`); never commit a general change directly to `retail`, or the next fix to one silently misses the other.
ufrontend's `publish-all` enforces it: `hp2-retail-exp` declares `publish.engine.baseBranch: "main"` and a
`retail` checkout that does not contain `origin/main` is refused.

0. On a NEW machine: the workspace's `bin/setup.sh` (WorkUnreal) — `docs/porting/BOOTSTRAP-NEW-MACHINE.md` is the
   HP2 port's own earlier layout, kept for its history. Branches: `main`/`prototype` = the pre-release snapshot assets; `retail` = the retail disc.
1. **`docs/porting/PLAN.md`** — the work queue: phases A–G, the decisions already made, acceptance
   lines, the progress checklist. Read it before touching anything.
2. `docs/porting/analysis/00-critique.md`, then the analysis report for the phase you are in.
3. The template's `CLAUDE.md` + `BUILD.md` (`/home/ion/engines/engine-ut99`) for the *why* of every convention.
4. Build, run and test: **`BUILD.md`** (native + web + E2E). Three-way-merge helper: `tools/dev/merge3.sh <outdir>`;
   `docs/porting/LOG.md` is the dated progress log (newest last).

## Map of the world
| Thing | Path | Role |
|-------|------|------|
| This engine repo | `$W/HarryPotter2` (branch `main`) | HP2 build-433 source → WASM/native artifacts; the PROTOTYPE engine (`hp2-proto`) |
| Its `retail` worktree | `$W/HarryPotter2-retail` (`git worktree add ../HarryPotter2-retail retail`) | the RETAIL engine (`hp2-retail-exp`), built side by side |
| The frontend clone (branch `hp2-retail`) | `$W/ufrontend` | consumes the artifacts; `targets/hp2-proto/` + `targets/hp2-retail-exp/` descriptors; storage prefixes = the ids (DBs `hp2-proto-user-data`, `hp2-retail-exp-user-data`) |
| Renderer forks (branches `hp2-v433-compat` / `hp2`) | `$W/XOpenGLDrv`, `$W/utglr-copy`, `$W/gl4es` | fetched by CMake; edited here, never in `/home/ion/engines/*` |
| Game assets (never in git) | `Assets/` (prototype) · `$W/assets-retail` (retail, from the ISO) | frozen per target, game assets only: `$W/asset-folders/{hp2-proto,hp2-retail-exp}` (README inside, `<id>.sha256` beside) |
| The template engine (READ ONLY) | `/home/ion/engines/engine-ut99` | the pattern every port decision mirrors |
| Epic reference sources (READ ONLY) | `/home/ion/epic-sources/ut99-v400` | the v400 base HP2's 433 tree diverged from; the 3-way-merge base |

⚠ **`/home/ion/ufrontend`, `/home/ion/engines/*`, `/home/ion/epic-sources` are another agent's working
copies. Read them; never write, build in, or `git` in them.** (They exist on the original box only; on another machine
clone them OUTSIDE the workspace if you need the "why".) Every repo this port needs is cloned fresh under `$W`.

## Layout (the clean-up, first commit)
```
Source/        every source folder of the original drop (C++ packages + UnrealScript Classes/) + our
               CMake build; Windows-only originals (WinDrv, Window, D3DDrv, UnrealEd, Launch, FrontEnd,
               DebuggerLaunch, PackageTool, CutCompiler, Setup, Gesture*, EdSrc, Tools) are kept for
               reference and not built
Config/System/ Default.ini + DefUser.ini (shipped) · web/ = the web boot set (preloaded into MEMFS)
Thirdparty/    the closed / vendored libraries the original build linked (OpenAL 1.0 SDK, Vorbis SDK
               libs, EAAudioCodec `coda`, DirectX 3/5/7 headers, s3tc, ENCVAG, MpegLib)
Assets/        game content, gitignored (Assets/README.md)
vendor/glue/   ue_abi.h + webrtc-shim.js + wasmfs-shim.js, vendored from @ufront/glue at ABI_VERSION 1
docs/          SOURCE-LAYOUT.md (raw-drop → repo mapping), porting notes as they land
```

## Load-bearing HP2 facts (how it differs from the UT99 template)
- **Build 433, 436-era interfaces.** `DECLARE_CLASS` takes the package name; natives register through
  `UnCoreNative.h`/`UnEngineNative.h` lookup tables (`GNativeLookupFuncs`), not v400's `GNatives[]` only.
- **`URenderDevice` (Engine/Inc/UnRenDev.h) is NOT v400's:** `DrawComplexSurface(…, DWORD PolyFlags, BYTE
  cAlpha)`, pure `MaxVertices()` + `DrawTriangles(Frame, Info, Pts, NumPts, Indices, NumIndices, PolyFlags,
  Span)`, non-pure `DrawGouraudPolygon`, `GetOsSurface`. Every render driver (XOpenGL, UTGLR, NullDrv,
  the in-tree SoftDrv) must implement that face — gated in the forks on `ENGINE_VERSION==433`.
- **`UAudioSubsystem` (Engine/Inc/UnAudio.h) is HP2's own:** `PlaySound(…, INT Flags, FLOAT FadeDuration,
  FLOAT Priority)`, `PlayMusic(FString Song, FLOAT FadeIn)` opens **Ogg Vorbis** files from `Music/` by
  path (there is no `.umx`, and music never goes through `Paths=`), `FLipSyncData`, `ModifySound`,
  flag-driven stop/fade. The shipped driver is `ALAudio.ALAudioSubsystem` (OpenAL 1.0, EAX compiled out,
  vorbisfile, the closed `coda` library). **Compressed sounds are EA-XA ADPCM** (`sx -eaxa_blk` at
  import, 15-byte frames / 28 samples — 2,994 of 3,289 sounds); plain sounds are RIFF WAVE PCM 22050 Hz
  mono 16-bit; dialog is `AllDialog.uax` (97 MB) loaded on demand. Not MPEG Layer II — `MpegLib` has no
  runtime caller.
- **No `IpDrv`, `UWeb`, `IpServer`, `UBrowser` in the drop** — the retail game shipped no multiplayer.
  `Default.ini` still names `IpDrv.TcpNetDriver`; the port brings IpDrv (from the template, adapted to
  433) and the engine-local `WebRTCDrv` for P2P.
- **Closed binaries:** `EAAudioCodec` (`coda`, only `.lib`), `MpegLib.lib`, `s3tc.lib`, `ENCVAG.LIB`
  (PS2). Worked around, never decompiled, linked on no target: s3tc's encode path is editor-only and the
  9 DXT1 textures all carry P8 twins (no runtime decode), ENCVAG is `HAVE_VAG 0`, `coda` is initialised
  from `UnVcWin32.cpp` only and its one job (EA-XA decode) gets an open in-tree decoder in Phase E
  (`tools/dev/analysis/xadec.py` is the reference).
- **Static linking is the template's registry, not HP2's `__STATIC_LINK`.** HP2 carries a Loki/PS2-era
  `__STATIC_LINK` scheme (`GNativeLookupFuncs`, `UnCoreNative.cpp`, `AUTO_INITIALIZE_REGISTRANTS`) whose
  native table is missing 29 natives; it stays undefined and its macro branches are deleted.
- **Layout under GCC/clang:** two script classes shift invisibly to the size guard — `ABrush→AMover` and
  `ANavigationPoint→APlayerStart` (trailing bitfield runs). Both get a 3-byte pad in the generated
  `EngineClasses.h` under `#ifndef __MINGW32__` (MinGW lays bitfields out the MSVC way and the pad would
  overshoot there). Never regenerate that header without re-applying the pads.
- **Paths in content are Windows paths.** `HGame.u` opens `Cutscenes\<file>.int` (the dir is
  `System/CUTSCENES`), music is `..\music\<Song>.ogg`, the save dir literal is `"..\\Save"`. The Unix file
  manager folds `\`→`/` and resolves case per component; the web glue does NOT fold case for direct opens
  under `-sWASMFS`, so this is engine work, not frontend work.
- **`AudioMutex` has no users** — left unbuilt.
- **Line endings:** the drop was CRLF; the repo is LF (second commit). Diff against
  `/home/ion/epic-sources/ut99-v400` with `--strip-trailing-cr`.

## Do NOT
- Do NOT cherry-pick the Dreamcast/PSX2 device layers (KOSDrv/PVRDrv/AICADrv, `__PSX2_EE__`, the VU0
  vector asm in `UnMath.h`, `FFileManagerPSX2*`). Pull only what the web port genuinely needs.
- Do NOT bake heavy glue (FetchFS/IDB/WebRTC logic) into the WASM — only the thin shims in `vendor/glue`.
- Do NOT change the ABI (`vendor/glue/ue_abi.h`, `ABI_VERSION 1`).
- Do NOT commit game assets or build trees — gitignored.
- Do NOT build the other engines from here, or this one from the other repos: this game is standalone.
- Do NOT write, build or run anything outside the workspace `$W` (run trees, wine prefix, publish output and
  screenshots included) — the owner's rule.
- Do NOT use `git commit -a` / `git add -A` while an agent works in this tree: commit by explicit path.
- Do NOT confuse the platform keys: `#if _MSC_VER` = the MSVC compiler, `#ifdef PLATFORM_WIN32` = Windows the OS,
  `#if __GNUG__` = the GNU family (MinGW included). Never add `#ifdef WIN32`/`_WIN32`/`_WINDOWS` in engine code.
- Do NOT put POSIX-only calls (`fork`, `waitpid`, `sigaction`, `mkdir` modes, …) outside `Core/Src/UnUnix.cpp`
  (behind `#ifndef PLATFORM_WIN32`, Win32 spelling in the `#else` or `UnMinGW.cpp`). New file I/O is binary
  (`"rb"`/`"wb"`); path literals use `PATH_SEPARATOR` or `/`.
- Do NOT hand-edit `EngineClasses.h` beyond the two layout pads (`bColoredPad`, `bTwoWayPad`); regenerate
  `EngineLayoutAsserts.cpp` with `tools/dev/layout/emit_asserts.py` after any change.
- Do NOT subtract pointers that may point into different buffers (GCC compiles it as an exact division — the
  `MeshRn` segfault); use a byte difference and check it.

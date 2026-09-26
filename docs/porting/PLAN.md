# HP2 web port — THE PLAN (read this first, then grind)

**Goal:** Harry Potter and the Chamber of Secrets (PC, Unreal Engine 1 build 433) running in a browser
through the ufrontend shell, on the exact template of the UT99 port (`/home/ion/engines/engine-ut99`).
Native Linux and Windows builds ride along as the compat oracle and parity work. **A web build running is
the finish line**; everything else is ordered by how much it de-risks that.

This is the durable work queue. Revision 2 (2026-09-26) folds in the eleven analysis reports and the
critique in `docs/porting/analysis/` (`00-critique.md` first: it resolved 25 contradictions between the
reports by reading the code, and every "C#", "M#-#", "X#" reference below points into it). Each phase
lists its steps, the decisions already made (do not relitigate them), how to verify, and what to commit.
Work top to bottom; parallelise inside a phase where the steps name independent packages.

---

## 0. State of the world (2026-09-26)

Done and committed (repo `/home/ion/hp2-ufrontend/HarryPotter2`, branch `main`):

| Commit | What |
|--------|------|
| `754be39` | Raw 2002 drop restructured into `Source/ Config/System/ Thirdparty/ Assets/(ignored) vendor/glue/` — mapping in `docs/SOURCE-LAYOUT.md` |
| `200241e` | CRLF → LF for every text file under `Source/` and `Config/` |
| `e47a3c7` | CMake skeleton (`Source/CMakeLists.txt`, `CMakePresets.json`, `cmake/`, per-package `CMakeLists.txt`), the template's portable packages copied **UNADAPTED** (`NSDLDrv NullDrv SoundDrv NOpenALDrv IpDrv WebRTCDrv XOpenGLDrv OpenGLDrv Game UCC`), `Core/Src/UnStaticExports.cpp UnMinGW.cpp`, `Core/Inc/UnStaticExports.h`, `Editor/Src/NullEdHook.cpp`, `tools/` |
| `306b4f0`, `c73e7e7` | analysis report snapshots |
| (this commit) | all eleven reports complete + `00-critique.md`; the readers' scripts and data in `tools/dev/analysis/`; this plan revised; the four P0 CMake list edits (§3.1); `Source/CMakeLists.txt` web audio OFF until Phase E |

Workspace (all fresh clones — **never touch `/home/ion/ufrontend`, `/home/ion/engines/*`,
`/home/ion/epic-sources`**; another agent works there):

```
/home/ion/hp2-ufrontend/
  HarryPotter2/      this engine repo (git, main)
  ufrontend/         clone of 10-p/unreal-frontend-arch-private, branch hp2, `yarn install` done
  XOpenGLDrv/        clone of 10-p/XOpenGLDrv, branch hp2-v433-compat (== ut99-v400-compat today, 87f6e89)
  utglr-copy/        clone of 10-p/utglr-copy, branch hp2 (== main today, e60807b)
  gl4es/             clone of 10-p/gl4es, branch hp2 (== fix/cubemap-bind-active-unit, 98441ed)
  .analysis-snapshots/  stale; delete once this commit lands
```
The CMake presets point FetchContent at these sibling clones (`FETCHCONTENT_SOURCE_DIR_XOPENGLDRV`,
`FETCHCONTENT_SOURCE_DIR_UTGLR`, `GL4ES_SOURCE_DIR`), so nothing has to be pushed for a build to work.
(The forks briefly lived inside `ufrontend/`; they were moved on 2026-09-26. Never `git add` them there.)

Toolchain on this box: gcc 15.2 (+ `gcc-multilib`, `libsdl2-dev:i386`, `libopenal-dev:i386`,
`libgl1-mesa-dev:i386`), cmake 4.2, node 22, yarn 4.17 (corepack), emsdk 5.0.7 at `/home/ion/emsdk`
(`source ~/emsdk/emsdk_env.sh`; on a fresh emsdk run `embuilder build sdl2` once), `i686-w64-mingw32-gcc`
13.2, wine. 32 cores, 31 GB RAM, ~50 GB free disk.

**Analysis reports** (`docs/porting/analysis/`, all complete): `00-critique.md` (read first),
`template-core.md`, `template-engine.md`, `template-render-fire-softdrv-editor.md`, `template-drivers.md`
(its §11 second pass overrides §0–10 where they differ), `template-launcher-ucc.md`, `renderer-forks.md`,
`hp2-core-engine-specifics.md`, `hp2-game-boot-and-content.md`, `ufrontend-target-contract.md`,
`web-layer-template.md`, `windows-mingw-template.md`. `00-run1-summaries.md` is the killed first run's
summaries — partly stale; a current report wins where they disagree. **The readers' tools and data live in
`tools/dev/analysis/`**: `tcore-private/syn/UnTemplate.fixed.h` (the resolved `UnTemplate.h`, take it
wholesale), `tcore-private/cc/probe.sh` (the `-fsyntax-only` probe), `xadec.py` + `eaxa_test.py` (the EA-XA
decoder and its oracle), `upkg.py`/`uaxdump.py`/`updeps.py`/`closure.py`/`compare.py`/`layout.py` (package
parsers, boot closure, layout checks), `hp2-target.json` + `hp2-exclude.json` (draft descriptor),
`hp2-file-manifest.json`/`hp2-dependency-manifest.json` (dry-run outputs), `varargs.txt` +
`hp2-ce-nonpod*.txt` + `hp2-ce-forscope.txt` + `h-editor-forscope.txt` (the scanner censuses),
`smoke-coi-full.mjs` (the COOP/COEP-aware web smoke), `utglr_gates.txt`/`xogl_ids.txt` (fork gate lists).

---

## 1. Ground rules (the owner's, verbatim in spirit)

1. **Template fidelity.** Every build-system, layout, driver and ini decision mirrors `engine-ut99`. When
   HP2 differs (433 interfaces), adapt the template's code to HP2 — never the reverse. Read the template's
   `CLAUDE.md`, `BUILD.md` and the comments in its sources; they explain *why* each thing is as it is.
2. **Closer to `engine-ut99` than to Epic.** `/home/ion/epic-sources/ut99-v400` is the common ancestor
   and the *base* of every three-way merge, nothing more. **No Dreamcast/PSVita/KOS deltas** (already
   absent from our CMake); **no PSX2 deltas** (HP2's own `__PSX2_EE__` blocks go away too).
3. **Standalone.** This repo never builds the other engines; they never build it. Separate clones,
   separate branches (`hp2`, `hp2-v433-compat`) in ufrontend/XOpenGLDrv/utglr-copy/gl4es. Merging
   back is a later, explicit decision. Never push anywhere unless the owner asks.
4. **Closed binaries are worked around, never decompiled**: `coda` (EAAudioCodec — an EA-XA ADPCM decoder
   with an open replacement), `MpegLib.lib` (no runtime caller), `s3tc.lib` (no runtime decode needed: the
   9 DXT1 textures all carry P8 twins), `ENCVAG.LIB` (PS2), the DirectX libs, the OpenAL 1.0 SDK libs.
   Their folders stay in `Thirdparty/`; nothing links them on any target.
5. **Assets never in git** (`Assets/` is ignored). Inis live in `Config/System/`; the web set in
   `Config/System/web/`.
6. **Commit small, commit often**, subject `Pkg: what — why` (see `git log`); end every commit message
   with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>` (or the model doing the work). Update
   `BUILD.md`/`CLAUDE.md`/this plan in the same commit as the change they describe.
7. **Verify, don't assume.** Every phase has an acceptance line. A log milestone in `Game.log` is the
   evidence, not "it compiled". The first native boot log is committed as the fixture every later regex is
   derived from (M3-5).

---

## 2. Decisions already made (do not reopen)

| Topic | Decision | Why / evidence |
|-------|----------|----------------|
| Engine repo folder / name | `/home/ion/hp2-ufrontend/HarryPotter2`, project `HP2` | owner's folder; `publish.engine.repo` = `HarryPotter2`, `enginesRoot` = `/home/ion/hp2-ufrontend` |
| Launcher | `Source/Game/Src/Launch.cpp` → executable **`Game`** (`Game.exe` on Windows); web artifact **`SDLLaunch.{js,wasm,data}`** | matches HP2's `Game.exe`; the frontend hardcodes `SDLLaunch.js` |
| `GModule` / log / ini names | `GModule = "Game"` on every platform → `Game.log` natively and `/System/Game.log` on the web; `appInit(TEXT("Game"))` → `Game.ini`/`User.ini`; `THIS_PACKAGE = "Game"` so `LocalizeError` resolves against the retail `Game.int`. UCC keeps retail `appInit(TEXT("HP"))` → its own `HP.ini` | C9, C22 |
| Original Win32 sources | kept in `Source/` (Launch, WinDrv, Window, D3DDrv, UnrealEd, FrontEnd, …), **not built** | reference for HP2 boot semantics |
| TCHAR | **ANSI** (`char`) on every target incl. Windows. Consequences: `wcscmp` → `appStrcmp` at `Core/Src/UnClass.cpp:1439` and `Editor/Src/UnEdCam.cpp:2313,2345,2377,2442`; retail UTF-16 `*_pa.usa` caches are not importable | C1 (retail was `/D UNICODE`; every shipped text file is 8-bit) |
| Static single binary | ut99dc's `UNREAL_STATIC` + `UnStaticExports` registry + `--whole-archive`, with the template's `IMPLEMENT_CLASS`/`IMPLEMENT_PACKAGE`/`IMPLEMENT_FUNCTION` (+`STATIC_EXPORT`) and `THIS_PACKAGE`. HP2's `__STATIC_LINK`/`GNativeLookupFuncs` scheme stays **undefined and is deleted from the macros**; `Core/Src/UnCoreNative.cpp` is dropped from the build (empty TU) | its native table is missing 29 natives and its macros are ill-formed under GCC (template-engine §7); C14 |
| `DECLARE_CLASS` arity | **strict 4-arg** `DECLARE_CLASS(TClass,TSuper,TFlags,TPackage)` everywhere: HP2's headers and SoftDrv already use it; every copied driver gets the 4th argument per site; the forks gain a greppable `==433` rung. No variadic macro | M1-4 |
| `GSecondsPerCycle` / time | stays HP2's `FLOAT`; `appSeconds()/appSecondsSlow()/appProcessSeconds()` return **`FTime`**, computed in double from `SDL_GetPerformanceCounter()` with a start-counter base | C7; hp2-core §g |
| `__UNIX__` / `__LINUX__` | **not defined** (the template's `UnGnuG.h` defines neither); re-key `Core.h:55` `SUPPORTS_PRAGMA_PACK` → `_MSC_VER \|\| __ICC \|\| __GNUC__` and `UnMisc.cpp:1629` → `#if _MSC_VER`; `PATH_SEPARATOR`/`LINE_TERMINATOR` unconditional `/`, `\n` | C13 |
| `FFileStream` | Core keeps **link stubs** (`Read/Create/Destroy` return 0/1 in `UnUnix.cpp`); the audio driver streams itself (driver-owned `FAudioStream`, no thread, pumped from `Update()`); the `Destroy()` busy-wait is `#if _MSC_VER` | C4 |
| `GetGlobalTime` | returns **0 on every GCC target** (template/Epic `FFileManagerLinux` behaviour). Fixes both consumers at once: `appInit`'s "regenerate the inis when Default.ini is newer" and the persistent-actor cache mtime check. `appInit` code stays otherwise unchanged | C5 |
| Path normalisation | one Core component: `FFileManagerLinux::FixPath()` — `\`→`/` then per-component case-insensitive resolution against `readdir` when the exact path misses (cached) — used by every entry point; plus the template's readdir-based `appFindFileNoCase` for package lookups. **The glue does NOT fold case for direct opens under `-sWASMFS`** | M2-2, M3-2, S7 |
| Renderers | XOpenGLDrv (WebGL2, web default), OpenGLDrv/UTGLR + gl4es (WebGL1), SoftDrv (`-SOFTWARERENDERER`), NullDrv (`-NULLRENDERER`). **No NOpenGLESDrv** (switch + preference row removed from the launcher) | owner's list |
| SoftDrv merge base | **the template's nine files** + HP2's 125-line 433 delta re-applied (`DrawComplexSurface(…,PolyFlags,cAlpha)` with the six `Surface.PolyFlags`→`PolyFlags` swaps, `MaxVertices()=256`, `DrawTriangles` three-loop body, 4-arg macro, `Render.h` include); `VolumetricLightingMeshes` becomes a SoftDrv-local config bool; mesh fog gated on `PF_RenderFog` | C11; render report §3.3–3.4 |
| 433 render face | forks get `#if ENGINE_VERSION==433` alongside `==400`: 5-arg `DrawComplexSurface`, `INT MaxVertices()` **= 256 on every device** (the mesh batcher sizes its buffers once), `DrawTriangles(Frame, Info, Pts, NumPts, _WORD* Indices, NumIndices, PolyFlags, Span)` with three input shapes (indexed list / `Indices==NULL && NumIndices==NumPts` fan / plain list), `DrawGouraudPolygon` non-pure forwarder (drivers must NOT declare it), `GetOsSurface` stub. `cAlpha` = transparency (255 = invisible) only on Lumos surfaces; `Light.W` = per-vertex opacity; `PF_Highlighted\|PF_Translucent` = straight alpha. Retail `Source/D3DDrv/Src/Direct3D7.cpp` is the behavioural spec | renderer-forks §2–3, C23 |
| Layout | port the template's `[layout]` size guard recording in `UStruct(ENativeConstructor…)` **only** and checking in `UStruct::Link` (keep HP2's `PropertiesAlign`); add `ABrush::bColoredPad[3]` **and** `ANavigationPoint::bTwoWayPad[3]`, both `#ifndef __MINGW32__`; Fire `GCC_ALIGN(4)` on `SparkType/FX_Heat/DropType`; generate the per-property `static_assert` file from the shipped `.u` (template-engine Appendix A) | C6, C12; template-engine §10.4 |
| Editor | stripped `NullEdHook` Editor linked **ON on every target**: `Entry.unr` (loaded at boot) imports `Editor.B_PlyrOn`, and `Editor.u` imports the native-only `Editor.TransBuffer`. `Editor/Src/UMasterCommandlet.cpp` is out (includes the unbuilt `Setup/Inc/Setup.h`) | C2 |
| Audio | **EA-XA ADPCM**, not MPEG Layer II: `sx -eaxa_blk` at import, 15-byte frames / 28 mono samples, 2,994 of 3,289 sounds; plain sounds are RIFF WAVE PCM 22050 Hz mono 16-bit; music = `Music/*.ogg` opened **by path** (`../Music/<Song>.ogg`, never through `Paths=`); dialog = `AllDialog.uax` (97.4 MB) loaded by `DynamicLoadObject` on the first spoken line. Phase order: `SoundDrv.NullAudioSubsystem` (template-drivers §4 class body) first; then `NOpenALDrv` = HP2's `ALAudio` ported to openal-soft (EFX, no EAX — ALAudio already compiles EAX out) + FetchContent `xiph/ogg v1.3.5` + `xiph/vorbis v1.3.7` built **without** `-fpack-struct=4` and included under `#pragma pack(push,8)` + the in-tree EA-XA decoder (`tools/dev/analysis/xadec.py` is the reference; `eaxa_test.py` the oracle: decoded amplitude vs the shipped lip-sync tables, r ≥ 0.996) | C3, M5-1..M5-3; template-drivers §5, §11 |
| `AudioMutex` | zero users, never shipped — **left unbuilt, nothing to port** | C24 |
| Networking | IpDrv ported from the template (433-adapted, its two commandlets dropped, no Dreamcast blocks); `WebRTCDrv` ported (`WEBRTC_PEER_PREFIX "hp2"`, `"HP2 Server"`); **no `IpDrv.u`** in Phase B (TcpNetDriver binds without it; `ucc make` can build it later as a Phase F nicety); `ServerActors.Empty()` on **all** platforms (the retail ini names IpServer/UWeb which do not exist); `DownloadManagers=Engine.ChannelDownload`. Whether hp2 exposes P2P multiplayer at all is owner question O4 | template-launcher-ucc §3.5; C7 in drivers |
| Mouse / gestures | **no gesture input path**: `CompareGesture` has no script caller; spells are hold/release AltFire with the `SpellCursor`. Ordinary pointer lock + relative deltas, absolute cursor when `bShowWindowsMouse` | C17 |
| Frontend target | `targets/hp2/` in the ufrontend clone; `storage.dbPrefix = "hp2"`, `keyPrefix = "hp2"` → IndexedDB `hp2-user-data`, `hp2-mods`, `hp2-network-cache`; **package cache is OPFS `pkg/hp2/`** (not IndexedDB); localStorage keys `hp2_*`; `network.peerPrefix = "hp2"`; `travel` = contract §1.5 fallback (one `Engine.GameInfo` "Story" mode + `GameSpeed=1.3` toggle) unless O1 picks the generic change; `userFiles` = `Save/*.usa`, `Save/*.bmp`, `Save/Slot1..6/*.usa\|bmp` (never `Save/cache/`); `AllDialog.uax` **not** in `precache.json` (menu stays ≈141 MB); `logPath '/System/Game.log'`; published at the ROOT of its own domain with no portal — **Phase G**, not Phase D | C8, C10, C20, C25, M3-10 |
| Web audio during bring-up | `BUILD_NOPENALDRV` **OFF on web until Phase E** (the copied driver is the v400 one and cannot compile against `UnAudio.h`); the launcher's web `AudioDevice` override is guarded on `HP2_HAS_NOPENALDRV` → `SoundDrv.NullAudioSubsystem`; web `Default.ini` `AudioDevice=SoundDrv.NullAudioSubsystem` | M3-1 |
| Music path | `Paths=` keeps **exactly the retail six lines** with `../Save/*.usa` last (`appInit` sets `Paths[5]` by index and rewrites the **last** `Paths=` of `Game.ini` on every boot). Never add a 7th line; the `../Music/*.umx` line stays as a placeholder | C19; web-layer §9.2 |
| Line endings | LF everywhere in `Source/`, `Config/` | template is LF; diffs against epic need `--strip-trailing-cr` |
| "Main menu" | HP2's retail main menu was a Win32 dialog; `Startup.unr` is an attract fly-through that creates and immediately closes the `FEBook`. Acceptance B is defined on log lines (§4) — Esc opens `FEInGamePage` | M2-1; game-boot fact 1–2 |

---

## 3. Phase A — Core, Engine, Render, Fire, SoftDrv, Editor compile natively (Linux, 32-bit)

**Method: three-way merge, base = epic v400, ours = HP2 433, theirs = engine-ut99**, driven by the per-file
strategy tables the reports produced. `tools/dev/merge3.sh <outdir>` re-runs the dry run (119 untouched,
49 clean, 6 take-theirs, 41 conflicting files). The reports classify every hunk; use them, not the raw dry run.

### 3.1 CMake list edits (one commit; each removal stops the build otherwise — M1-3, C14)
- `Render/CMakeLists.txt`: drop `Src/UnTest.cpp`. `Engine/CMakeLists.txt`: drop `Src/palette.cpp` (`__declspec` export, MSVC-only caller). `Editor/CMakeLists.txt`: drop `Src/UMasterCommandlet.cpp`. `Core/CMakeLists.txt`: drop `Src/UnCoreNative.cpp`. (Applied in the revision-2 commit — verify.)
- Keep out: `AudioMutex.cpp`, `UnPSX2.cpp`, `UnVcWin32.cpp`, `winstuff.cpp`, `EdHook.cpp`, `TTFontImport.cpp`, `UPS2ConvertCommandlet.cpp`, `UPSX2MusicCommandlet.cpp`.
- Delete the duplicate `Render/Src/UnSpan.h` and `Render/Src/Amd3d.h` (the `Inc/` copies win).

### 3.2 Core (template-core §9.3 — its 13 boxes are the commit plan; the header boxes 1–4 are ONE commit, M1-1)
1. **Platform header + object model, atomic** (nothing compiles in between): `Inc/UnGnuG.h` = template file + 433 re-adds (`FTime appSeconds()/appSecondsSlow()`, `FLOAT GSecondsPerCycle`, `DEFINED_appCycles/appSeconds`; `DEFAULT_ALIGNMENT` 8; drop PSX2/`OWORD`/`GCC_MOVE_ALIGN`/`__Context`/`#error`/`typedef size_t`/`#define HANDLE`; keep the template's `PLATFORM_WIN32` blocks, `floorf`-based `appRound/appFloor`, `stdint` typedefs, `SIZE_T=size_t`, `IMPLEMENT_PACKAGE_PLATFORM`+`STATIC_EXPORT`); `Inc/UnUnix.h` = template; `Inc/Core.h` 3WAY + `SUPPORTS_PRAGMA_PACK` on `__GNUC__` + delete the `__STATIC_LINK` include + the `TCHARU` `PLATFORM_WIN32` branch (W8); `Inc/UnFile.h` 3WAY (keep HP2's `FTime` decls; the template's inline `appMalloc` needs `GetMallocError` from `Core.cpp`); `Inc/UnObjBas.h` (4-arg `DECLARE_CLASS` with **no-parens** `new(X)TClass` placement-new; template `IMPLEMENT_CLASS`/`IMPLEMENT_PACKAGE`; delete the `__STATIC_LINK` branches + `AUTO_INITIALIZE_REGISTRANTS`; move `ConstructObject` ×2 to `UnClass.h`); `Inc/UnScript.h` (template `IMPLEMENT_FUNCTION`+`STATIC_EXPORT`, unconditional `P_GET_SKIP_OFFSET` memcpy; **keep HP2's `P_FINISH` `EX_DebugInfo` step and `FFrame::Parent`**); `Inc/UnStack.h` (HP2 ctor + template `virtual ~FFrame()`; HP2 opcode numbers); `Inc/UnTemplate.h` = `tools/dev/analysis/tcore-private/syn/UnTemplate.fixed.h` (433's `TRefArray→TArray→FArray` needs using-declarations — ~65 dependent-name sites; `TTransArray::Insert` → `TArray<T>::Insert`; `FString::Mid` DWORD fix). Gate: the header syntax probe (`tools/dev/analysis/tcore-private/cc/probe.sh`) → 0 errors with `g++ -m32` and with `clang++ --target=wasm32-unknown-emscripten -fpack-struct=4`.
2. The other clean/near-clean headers: `FFileManagerGeneric.h` (drop the in-class qualifier; `GetGlobalTime`→0), `FFileManagerLinux.h` (+ W6 `S_IRUSR|S_IWUSR|S_IXUSR` mkdir modes, `<sys/stat.h>`; + **`FixPath()`** and flag-honouring `FindFiles` — M2-2, M3 in launcher §3.5), `FMallocAnsi.h`, `FOutputDeviceFile.h` (keep HP2's `appUserDir()` log path; add the stdout mirror), `FOutputDeviceAnsiError.h` (take theirs), `UnCache.h`, `UnCoreNet.h`, `UnClass.h`, `UnMem.h` (delete the PSX2 `Align=16`).
3. `Inc/UnMath.h`: **keep HP2's**; delete the PS2 `FVector` (`W` member!) and every ASM/ASMLINUX/PSX2 transform branch; add `static_assert(sizeof(FVector)==12)`.
4. `Src/UnUnix.cpp` = template file + the "HP2 433 additions" block (the complete link-symbol list, M1-2): `FTime appSeconds()/appSecondsSlow()/appProcessSeconds()` (rebased counter), `appUserDir()` = `""`, `IsOSVer2kOrXP()` = true, HP2's 3-arg `appCreateProc` + `appGetProcReturnCode` (Unix bodies; Win32 bodies in Phase F), `FFileStream::Read/Create/Destroy(INT)` stubs, `appGetDllHandle` from the static registry, `appRequestExit` web branch, `appBaseDir()` = `SDL_GetBasePath()`. Verify: `nm -C Game | grep ' U '` lists none of these.
5. Small Src files: `Core.cpp`, `UnGUID.cpp` (the wild-write fix), `UnProp.cpp` (+ delete the PSX2 struct-offset block) — 3WAY clean; `UnAnsi.cpp` (`appStrchr` template body), `UnName.cpp` (keep HP2's hardcoded-name block; forward-declare `AllocateNameEntry`), `UnMath.cpp` (hoist `DWORD i` in `SetupTable`; nothing else from the template).
6. `UnClass.cpp` + `UnCorSc.cpp`: layout guard (`ENativeConstructor` ctor + after `Link`'s `PropertiesAlign` line), native-bool `BitMask` restore (`c506ba7` — HP2's `UClient` bools are adjacent `BITFIELD`s), `wcscmp`→`appStrcmp`, `Clamp<INT>` in `execArrayElement`; delete PSX2 blocks.
7. `UnMisc.cpp`: 9 of 11 template hunks merge clean; `appApplyIniOverrides()` **after** HP2's "gk" Language block; `appIsGlobalOptionToken`; `appUnwindf` critical-error latch; `[appThrowf]` echo; the HP2 blockers: `"default.ini"` → `"Default.ini"` and write `Language` only on success (S9); `"..\\Save"` → `TEXT("..") PATH_SEPARATOR TEXT("Save")` and the `..\\Textures\\Palettes.utx` probe; `debugf("%s", GSys->SaveSlotPath)` → `*GSys->SaveSlotPath` (S1 — a runtime trap on the web); `FFileStream::Destroy` wait `#if _MSC_VER`; MSVC6 for-scope leaks; `itoa`/`p`/`j`.
8. `UnObj.cpp` + `UnLinker.cpp`: the six exceptions-off web NULL sites (`#ifdef __EMSCRIPTEN__`), the **iterative** `FArchiveTagUsed` (worklist) **keeping HP2's `RefObj`/`GetReferencing()`** with both `DrainWorklist()` calls, the `CPF_Config` repair in `StaticConstructObject` (template-core §5.11 #10 — without it the GL devices start with every option off on the web, M4-5), the memtag cast; the template's `UnLinker.h` hunks by hand into HP2's split `UnLinker.cpp` (`OpenFailed` names the file ×2, `VerifyImport` NULL bail-outs ×2).
9. Gates: `cmake --build --preset native --target Core` clean; `nm -C libCore.a | grep GStaticExport | sort | uniq -d` empty.

### 3.3 Engine (template-engine §12 "Phase 1" then "Phase 2"; 42 template rows drop in clean, 13 need hand-porting, 40 are Dreamcast and never ported)
- Compile: `Inc/UnRender.h:129-256` delete the asm branches; `Inc/UnModel.h:154` hoist `INT i` (a header — blocks every TU); `Inc/UnActor.h:61`; `Inc/UnCamera.h:323` `PUSH_HIT` `decltype`; `UnLevAct.cpp` forward-declare `CompareHits`; `UnActor.cpp:366` expand `DOREP(PlayerPawn,TargetViewRotation)`; `UnLevel.cpp:1232`; the for-scope hoists (template sites + HP2-only G2–G9); `UnAudio.cpp:576-609` `CompressXA` → `#if _MSC_VER … #else return false;` (fails on all three targets otherwise: `hProcess` used outside its guard); `UnTex.cpp:12,747` `#ifdef WIN32` → `#if _MSC_VER` (MinGW defines `WIN32` and would link the closed `s3tc.lib`).
- Boot correctness: `EngineClasses.h` pads L1 (`bColoredPad`) + L2 (`bTwoWayPad`) `#ifndef __MINGW32__`; the generated `Src/EngineLayoutAsserts.cpp` (Appendix A `emit_asserts.py`); `UnLevTic.cpp:655,686,703` split the three `PriorityActors[n] = PriorityList + n++` (C++17 sequencing); `UnGame.cpp:1760-1762` `Tick` NULL-check order; `UnCamera.cpp:370-397` move the `#endif` so `RenDev->Exit()` runs on GCC; `UnCamera.cpp:534-542` screenshot row loop; exceptions-off NULL returns D3/D4/D9/D10/D11; tick floor `0.005f → 0.001f` (keep HP2's `0.10f` ceiling); every `MakeDirectory(SaveSlotPath, 0)` → `, 1` (12 calls); the savegame `FURL` fixes + HP2's `LOADGAME`/`-LOAD=` path parse (fix once in `FURL::FURL`); `appIsGlobalOptionToken` in `UGameEngine::Init`.
- Web contract (can land in Phase D): `FLUSHCONFIG` at the top of `UEngine::Exec`; the SDL whole-game mouse capture block; `Received` log in `UnDownload.cpp:176`.
- **Never port:** `UnTrace.cpp` `Use[FrontFirst]` removal (a template regression), every `PLATFORM_DREAMCAST`/`LOW_MEMORY` block, `LEGEND`, `HAVE_VAG 1`. AutoFOV is optional Phase-B+ polish (HP2 is third-person; its HUD scales from `Canvas.SizeX`, so AutoFOVUI does not apply — template-engine Q3).

### 3.4 Render, Fire (render report §1.7, §2.6)
- Render: `UnLight.cpp` — delete `#if _MSC_VER` at `:661` and the whole GCC `Merge` copy (`:847-1044`): the GCC copy has **no `bDarkLight` branch** (dark lights would brighten; `Adv1Willow` has 199) (M2-7); drop the `#if !__PSX2_EE__` lines at 546/548, 582/595; `UnRender.cpp` — `HackRaster[MAX_RASTER_Y]` with `#define MAX_RASTER_Y 8192` + `EndY = Min<INT>(EndY, MAX_RASTER_Y-1)` in `SetupRaster` (heap overrun above 2880-px-tall viewports, M2-10); delete the PSX2 halves and the asm of `Pipe()`; the `DrawWorld` `GetClass()` guard; 29 for-scope hoists (§1.5). `UnParticleRn.cpp` has never met GCC — expect template-deduction/narrowing errors.
- Fire: `Inc/UnFractal.h:19` `#if !_MSC_VER && !defined(__MINGW32__)`; `GCC_ALIGN(4)` on `SparkType`(:174), `FX_Heat`(:179), `DropType`(:292) — **mandatory**, `VERIFY_CLASS_OFFSET` aborts at the first `HPParticle.u` FireTexture otherwise; `UnFractal.cpp` hoists (`INT t` before 175, `DWORD Y` at 305/430, `INT Flash` before 1319/1338).

### 3.5 SoftDrv (render report §3.4 — exact edits)
`cp` the template's nine `Src/` files + `WEB-NOTES.md`; then: `SoftDrvPrivate.h` `#include "Render.h"`, 4-arg macro, replace the two v400 signatures with HP2's three (`DrawComplexSurface(…,DWORD PolyFlags,BYTE cAlpha)`, `INT MaxVertices()`, `DrawTriangles(…)`), add `BITFIELD VolumetricLightingMeshes` + its `UBoolProperty` in `SoftDrv.cpp`; `DrawSurf.cpp` signature + the six `Surface.PolyFlags`→`PolyFlags` inside `DrawComplexSurface` (load-bearing for Lumos); `DrawPoly.cpp` rename `DrawGouraudPolygon`→`DrawTriangles`, insert `MaxVertices(){return 256;}`, `GMeshFog = … && (PolyFlags & PF_RenderFog)` (particles never write `.Fog`), HP2's `PF_Highlighted|PF_Translucent`-masked block and the three draw loops. Verify: no `DrawGouraudPolygon` symbol from SoftDrv; `nm` shows the three 433 members.

### 3.6 Editor (render report §4.5 list — the current CMake list minus `UMasterCommandlet.cpp`)
Template fixes ED1–ED16 (`UnParams.cpp` duplicate-symbol guard under `UNREAL_STATIC` (W2), `PUSH_HIT`, `Min<INT>`), two new (`__declspec` globals + a fourth `wcscmp`, both `UnEdCam.cpp`), 72 for-scope sites (`tools/dev/analysis/h-editor-forscope.txt`).

### 3.7 Gates for Phase A
- Before merging Engine/Render/Fire/Editor: run the `-fsyntax-only` probe over them with the resolved Core headers and attach the error census to `docs/porting/LOG.md` (M1-6).
- One clang pass with `-Wnon-pod-varargs` **enabled** (host clang, `--target=i686-linux-gnu`): fix every hit on a boot/gameplay path before the first wasm link (M1-8; `tools/dev/analysis/hp2-ce-nonpod*.txt` is the starting census). Native gcc does not diagnose these.
- Run `tools/dev/analysis/compare.py` over `Core.u`, `Fire.u`, `Editor.u` as well as `Engine.u` before the first boot (M1-5).

**Acceptance A:** `cmake --preset native && cmake --build --preset native -j"$(nproc)" -DBUILD_GAME=OFF -DBUILD_XOPENGLDRV=OFF -DBUILD_NOPENGLDRV=OFF -DBUILD_NOPENALDRV=OFF` builds `Core Engine Render Fire SoftDrv Editor` (temporarily comment the driver `add_subdirectory` lines if they are not yet adapted; restore before B). Then `BUILD.md` gets its first (native) section.

---

## 4. Phase B — Drivers + launcher: the native oracle boots to the title map

1. **NullDrv**: 4-arg macro, 5-arg `DrawComplexSurface`, `MaxVertices()=256`, empty `DrawTriangles`, include fix (drivers §3, renderer-forks §9).
2. **SoundDrv**: the complete class body in template-drivers §4 (every 433 pure virtual; `PlaySound → -1`, `PlayMusic → 0`, `StopMusic → 0`; leave `USound::Audio` NULL). With null audio dialog timing works but lip-sync does not — not a bug to chase (M2-8); `-NOSOUND` is the zero-code fallback.
3. **NSDLDrv**: 4-arg macros (`Inc/NSDLDrv.h:26,109`), `FTime` at `NSDLClient.cpp:248` and `NSDLViewport.cpp:1154-1155`, `NoFractalAnim=0; ParticleDensity=1` (HP2's two extra `UClient` config keys), `bWindowsMouseAvailable=!Capture` in `SetMouseCapture`, `GetColorDepths/GetCurrentColorDepth/GetCurrentRenderDevice` execs, `SetRes` windowed/web-ignore, `LocalizeError("Failed3D")` literal; keep **every** `__EMSCRIPTEN__` site (N1–N21) intact; optionally forward the 433 `UViewport::TryRenderDevice` to the client. The GL-vs-software choice keys on the device name containing `OPENGL`/`XOPENGL` — no logic change.
4. **Game launcher** — the complete delta is template-launcher-ucc §2.6.2; the file is still byte-identical to the template. Keep everything template; change: delete Dreamcast blocks, `SimpleCopy`, the `~/.utconf` swap, the `NOPENGLESRENDERER` row/preference, the `IpServer.UdpServerUplink` line; bound the argv join (`appStrncat`); `GModule = "Game"`; `appInit(TEXT("Game"))`; chdir to the exe dir (or Core's template `appBaseDir`); web driver block with the `HP2_HAS_NOPENALDRV` guard; native `NormalizeSubsystem` lists (viewport `NSDLDrv.NSDLClient`; audio `NOpenALDrv…` then `SoundDrv.NullAudioSubsystem`; net `IpDrv.TcpNetDriver`); **`ServerActors.Empty()` on all platforms**; FirstRun read-then-stamp 433 with a log line, no dialogs, ignore `Reconfig`/`ForceSoftware`; `FTime` rewrite of the load-time, `MainLoop`, pacing ring and `ue_exec_command` printf; `CleanUpOnExit` calls `Engine->Audio->SetViewport(NULL)` before `appPreExit`; `HandleError` skips the SDL message box under `-NOMSGBOX`/dummy video. Optional: SoftDrv fallback when no HW renderer is compiled. Accepted-but-no-op: `-NOFRONTEND`. HP2 argv contracts the frontend must produce: `PrivetDr.unr -SAVESLOT=N` (new game), `-LOAD=0 -SAVESLOT=N` (continue).
   **Native ini-normalisation table** (M2-4; the retail `Default.ini` is what the native boot reads): `ViewportManager→NSDLDrv.NSDLClient`; `AudioDevice→NOpenALDrv…` iff `HP2_HAS_NOPENALDRV` else `SoundDrv.NullAudioSubsystem` (fatal at boot otherwise: `InitAudio` loads it `LOAD_NoFail`); `GameRenderDevice` **and** `WindowedRenderDevice` from the switch table; `ServerActors.Empty()`; `DownloadManagers=Engine.ChannelDownload`.
5. **IpDrv**: 4-arg macros, drop `UMasterServerCommandlet.cpp`/`UUpdateServerCommandlet.cpp`, delete Dreamcast blocks, `UnNetDrv.h`/`UnConn.h`/`UnNet.h`/`UnBunch.h`/`UnChan.h` 433 diffs (small). No `IpDrv.u`.
6. **UCC** (not blocking Acceptance B — M2-11): replace `UCC.cpp` with the template's + three edits (`appInit(TEXT("HP"))`, `GModule` basename via `appStrncpy`, bounded join); gate `add_subdirectory(UCC)` with `NOT MINGW` until someone needs it. Acceptance §3.7 of the launcher report: `ucc-bin help`; `ucc-bin Core.SimpleCommandlet` → the 9 golden lines; `batchexport HGame.u class uc <scratch>` loads HGame + imports with no `[layout]`/`Can't find file` (the fastest headless load check — use it **before** booting the client).
7. **Run tree** `tools/dev/mkrun-hp2.sh <build-dir> <run-dir>` (from `mkrun-ut99.sh`; M2-3): symlink `Assets/{Maps,Textures,Sounds,Music,Fonts,Help,SystemLocalized}`; **copy** `Assets/System` **excluding `*.dll *.exe`** (retail `OpenAL32.dll`/`Game.exe` would shadow ours on Windows, M6-2), then copy `Config/System/{Default,DefUser}.ini` in; `mkdir Save`; copy `build-*/Game/Game` (+ `libopenal.so.1` when NOpenALDrv is on) **last**; `run.sh` from `run-template.sh` (`-SOFTWARERENDERER` default). Never write into `Assets/`. Until `FixPath` lands, `ln -s CUTSCENES System/Cutscenes` is a stopgap.
8. Boot: `cd run/System && ./Game -SOFTWARERENDERER -log` (`Xvfb :99 -screen 0 1280x720x24 & DISPLAY=:99 …`). Chase `Game.log` one failure at a time (`Can't find file for package`, `Can't bind to native class`, `[layout]`). `tools/dev/shot.sh` screenshots the X display.
9. Captures once it boots (M3-5, M3-3, M4-2): commit the log as `docs/porting/logs/native-first-boot.log` (the fixture for every E2E/telemetry regex); measure 32-bit RSS at the menu and on `Ch6WizardCard`, `Entryhall_hub`, `Adv6Goyle` (`/proc/<pid>/status`) — if > ~1.2 GB test `-LAZY` before Phase D; take SoftDrv reference screenshots (menu, `Ch1Rictusempra` start, a Lumos surface, a particle effect).

**Acceptance B (M2-1 wording):** `Game.log` shows `Bringing Level startup.MyLevel up for play` (package
spelling follows the URL — settle O6 from this log) + `ScriptLog: Creating root window`, no `Critical:`,
**zero `[layout]` warnings** (needs L1+L2 pads), on `-SOFTWARERENDERER` and `-NULLRENDERER`; Esc opens
`FEInGamePage`; `open Ch1Rictusempra` (self-contained; `PrivetDr` also needs `AllDialog.uax` + the cut-scene
ints — a good end-to-end path test, and a first death without a save drops to a black `Entry`, M2-9) loads
and ticks 30 s; `Game.ini`/`User.ini`/`Game.log` appear in `System/` for both `./Game` and an absolute
`argv[0]`; `-SAVESLOT=2 PrivetDr.unr` logs `Save Slot Path: ../Save/Slot2` and creates `Save/Slot2/`.
Commit + `BUILD.md` native section.

---

## 5. Phase C — Hardware renderers on the 433 face (native first; renderer-forks §10.4–10.6)

1. **XOpenGLDrv** (`hp2-v433-compat`): add `XOGL_UE1_V4XX`/`XOGL_HP2` in `Inc/XOpenGLDrv.h`; X1 `#include "Render.h"` on 433; X8 4-arg `DECLARE_CLASS`; mechanical `ENGINE_VERSION==400` → `XOGL_UE1_V4XX` at ≈25 sites (§4.1 list, `tools/dev/analysis/xogl_ids.txt`); X5 alpha-blend flags → synthetic `PF_XOGL_AlphaBlend = 0x20000000`; C1 5-arg `DrawComplexSurface` with Lumos opacity; C2 `MaxVertices()=256` + `DrawTriangles` (indexed/fan/list; zero normals for particles); C3 `HP2AdjustPolyFlags` from `DrawTriangles`/`DrawGouraudPolygon`/`DrawTile`; C4 `LINE_PreTransformed`; C5 alpha-palette tiles → `PF_Highlighted`; C7 two-sided depth bias after first look. Regression: the template's v400 build symbol set unchanged.
2. **OpenGLDrv/UTGLR** (`hp2`): `UTGLR_ENGINE_UT99DC` (already defined by our CMake **and** `UTGLR_ENGINE_HP2`) forces v400 types at `OpenGL.h:362,640`, `OpenGL.cpp:3703` — split into platform (`UT99DC`) + derived `UTGLR_ENGINE_V400`; then H1–H7 under `UTGLR_ENGINE_HP2` reusing upstream's v436 `FTime`/4-arg code and the Rune/227 `PF_AlphaBlend` plumbing (`UTGLR_HAS_ALPHABLEND`). Regression as above.
3. **gl4es**: no change; decide the ref (O5).
4. Native GL check: `./run.sh gl` (XOpenGL) and `-UTGLRRENDERER`: first-light list §10.6 (HUD/menu tiles, alpha-palette icons, meshes/particles/blob shadows/fog, a Lumos surface fade, a fading actor with straight alpha, save-thumbnail colours, no `Unknown texture format`/GL errors). Diff against the Phase-B SoftDrv screenshots; optionally capture retail D3D7 under wine once for the Lumos quirk (M4-3, O9).

**Acceptance C:** both hardware renderers render the title map and `Ch1Rictusempra` natively without GL errors in the log; screenshots committed under `docs/porting/reference/`.

---

## 6. Phase D — The web build and the frontend target (THE GOAL; web-layer §15 W0–W4 + contract §12)

**W0 workspace:** forks beside the repo (done); `source ~/emsdk/emsdk_env.sh`; `embuilder build sdl2` once.
**W1 link:**
1. `Config/System/web/Default.ini` per web-layer §9.2 — derived from HP2's retail file, not the template's: six `Paths=` retail order, `../Save/*.usa` last; `LocalMap=startup.unr` (disk spelling, C21); no `CdPath`; `[Engine.Engine]` `ViewportManager=NSDLDrv.NSDLClient`, `GameRenderDevice=XOpenGLDrv.XOpenGLRenderDevice`, `WindowedRenderDevice`+`RenderDevice=SoftDrv.SoftwareRenderDevice`, `AudioDevice=SoundDrv.NullAudioSubsystem` (until E), `NetworkDevice=WebRTCDrv.WebRTCNetDriver`, `Console=HGame.HPConsole`, `FrameRateCap=120`; `[FirstRun] FirstRun=0` only; no `ServerActors`; `[NSDLDrv.NSDLClient]` template section + `NoFractalAnim=False`, `ParticleDensity=1` (Brightness 0.4 vs 0.5: verify visually); `[XOpenGLDrv…]`, `[OpenGLDrv…]`, `[NullDrv…]` template sections verbatim; `[SoftDrv…]` HP2's keys + `BilinearFiltering`/`VolumetricLightingMeshes`; `[WebRTCDrv.WebRTCNetDriver]` template block + `DownloadManagers=Engine.ChannelDownload`; `[IpDrv.TcpNetDriver]`; `[ALAudio.ALAudioSubsystem] Channels=32` (appInit sizes the stream table from it); `[Engine.GameReplicationInfo] ServerName=HP2 Server`; `[Engine.GameInfo]` + `bWorldLog/bLocalLog=False`; keep `[Editor.EditorEngine] EditPackages`; `[HGame.baseConsole] bDebugMode` = O3. **Omit** every UT99/Windows section (§14). `DefUser.ini` = HP2 retail (review `Escape=quit`). Replace `README-ut99.md`. Optional `Pickup.int` stub (missing from the drop; cosmetic).
2. Web audio off (done in CMake; mirror in the launcher guard) — M3-1.
3. `Launch.cpp` web paths (already template; after the Phase-B delta only the web-specific removals remain) — `emit-artifact-meta.mjs`: `targetId hp2`, `provenance.repo HarryPotter2`, `toolchain.base` text, `buildFlags` += `-sOFFSCREEN_FRAMEBUFFER`, drop the `libxmp` dep line (done), add ogg/vorbis deps in E.
4. `WebRTCDrv`: 4-arg macros, `FTime` at `WebRTCDrv.cpp:399-402`, `WEBRTC_PEER_PREFIX "hp2"`, `"HP2 Server"`, WebRTCLan `LanNow()`; keep `WebRTCLan` compiled but do not invest (no server browser in HP2).
5. **Milestone W1:** `cmake --preset wasm && cmake --build --preset wasm -j32` → `build-wasm/SDLLaunch/{SDLLaunch.js,.wasm,.data,meta.json}`; `offscreen-depth24.sh` reports 3 rewrites; `grep -c _webrtc_deferred_open SDLLaunch.js` = 0.
**W2 smoke:** `tools/web-smoke.mjs` → the COOP/COEP-aware `tools/dev/analysis/smoke-coi-full.mjs` version (the template's fails on a threaded build); GLog→stdout mirror, `[appThrowf]` echo, `appRequestExit` web branch, the `appInit` fixes (§3.2 #7). **Milestone W2:** `Name subsystem initialized` … `LoadMap: Entry` → `FailedBrowse` (no assets), no `Bound to UWeb.so`.
**W3 loader robustness:** every exceptions-off NULL site (L1–L4, L6–L8, L10; E1 startup-map fallback — decide Entry vs `DefaultLocalMap`, E3, E6, E7, E9, E10), `FLUSHCONFIG`, `appFindPackageFile` per-path case-insensitive retry, the `"..\\music\\"` and every other backslash literal → `/` (F2).
**W4 frontend** (`/home/ion/hp2-ufrontend/ufrontend`, branch `hp2`; contract §12 in order):
6. `.ufront/publish.local.json`: `{"out":"/home/ion/publish-hp2","assets":{"hp2":"/home/ion/hp2-ufrontend/HarryPotter2/Assets"},"enginesRoot":"/home/ion/hp2-ufrontend","emsdk":"/home/ion/emsdk"}` (+ `siteUrl`/`telemetryEndpoint` after G).
7. `targets/hp2/target.json` from `tools/dev/analysis/hp2-target.json` (contract §1.3 skeleton): `assetMarkers.fingerprint` = sha256 of `System/Core.u` (61,685 B, `e0812a19…56e8` — see game-boot §6.4), storage/network/mods (`.ogg` → `Music`)/`fetchStubs`/`manifestExclude` (from `hp2-exclude.json`: `*.dll *.exe *.bat *.dsp`, `System/{Default,DefUser,Game,User}.ini`, `*.usa`, `EditorRes/**`, `Maps/{Oldfiles,Studies,templates}/**`, `Logs/**`)/`userFiles`/`defaults` (`map: "PrivetDr"`)/`videoDrivers` (no NOpenGLES)/`frameRateCap` (`System/Game.ini`)/`capabilities` (decide `indexedDb` required — saves depend on it)/`travel` (§1.5 fallback)/`conversions []`/`notices`+`telemetry` (TO-VERIFY against the Phase-B log, then the first web log); `README.md`; `branding/{theme.css,logo.svg,help.md,portal-notes.json}` (contract §2; the 12 theme tokens).
8. Manifests: the contract §3 commands (measured: 460 files / 476 MB served, 714/714 references resolved, precache 231 files / 131.7 MB **without** `AllDialog.uax`); `yarn deps`; `precache.json`.
9. `test/skeleton.test.mjs:76` add `hp2` (+ telemetry `CAPTURED` fixtures from real lines); the other target-enumerating suites (§4.4); `.ufront/gates/no-engine-specifics.sh` learns `hp2`; `packages/e2e/lib/engines/hp2.profile.js` (`logPath '/System/Game.log'`) + `hp2-wasm-boot.spec.js` (M3-8: without them `yarn e2e --target hp2` passes vacuously); `docs/06`/`docs/07`/`STATE.md`/step record per the gates.
10. `yarn build --target hp2 --out serve-web-hp2` → `yarn serve --root serve-web-hp2` → `http://localhost:8080/`; then the E2E boot spec. Key-capture check: HP2 hard-codes F4/F6/F7/F9/F11, Tab, PageUp/Down, Insert — the page must `preventDefault` while the canvas has focus (X8).
11. `AllDialog.uax`: background-fetch after the menu (frontend change) or accept the first-line stall in v1 (M3-7).

**Acceptance D (Milestone W4):** headless Chromium: `Created WebGL 2.0 context on the worker` → `Bringing startup.MyLevel up for play` → `Possessed PlayerPawn`, no `Aborted`/`[appThrowf]`; then `-UTGLRRENDERER`, `-SOFTWARERENDERER`, `-NULLRENDERER`; a map loads from the Play panel; a key binding changed in-game survives a reload; `yarn test` green. `BUILD.md` web section. Root publish/portal/Azure are **Phase G** (M3-10).

---

## 7. Phase E — Audio (`NOpenALDrv` for the 433 `UAudioSubsystem`; template-drivers §5.6, §11.3–11.4)

1. Decoder first: port `tools/dev/analysis/xadec.py` (EA-XA ADPCM, 15-byte frames, 28 samples, coefficient/shift nibbles) to C++ with `eaxa_test.py`'s oracle as a unit test (decoded amplitude vs the shipped lip-sync tables). Stream XA in ~32 KB chunks; `Sound->Data.Unload()` after the last stream ends (dialog would otherwise accumulate ~450 MB decoded, M5-6).
2. `Source/NOpenALDrv/NOpenALDrv.cpp` for HP2 = `Source/ALAudio/Src/ALAudioSubsystem.cpp` ported per the line-level replacement table (§11.3): OpenAL 1.1 (openal-soft via `cmake/OpenALSoft.cmake`, EFX; **no EAX**), driver-owned `FAudioStream` pumped from `Update()` (no thread), `ResolveMusic()` tries `<Song>.ogg` then `<Song>` case-insensitively (`Music/Peeves_chase_v2` has no extension, M5-4), libvorbis via FetchContent (`xiph/ogg v1.3.5`, `xiph/vorbis v1.3.7`) built without `-fpack-struct=4` and included under `#pragma pack(push,8)` (`SUPPORTS_PRAGMA_PACK` on `__GNUC__`); the template's web bits (`Module.alsoftConfig`, `UseHRTF`, `OutputRate`, features chosen at run time). Config section keeps ALAudio's names and **float** volumes (`SoundVolume=0.9`, `MusicVolume=0.5`, `Channels`, `ReverseStereo`, `DopplerFactor`, `RollOff`, `UsePrecache`, `Use3dHardware=False`) + `OutputRate/UseHRTF/UseReverb` (M5-5).
3. Flip `BUILD_NOPENALDRV` back ON on web; web ini `AudioDevice=NOpenALDrv.NOpenALAudioSubsystem` + section; `target.json` `audioBuffer`/`audioHrtf`; `meta.json` deps (ogg, vorbis; no libxmp). Native: `libopenal.so.1` beside `Game`.

**Acceptance E:** menu music and spell SFX audible natively and on the web; a spoken line plays with lip-sync; stereo placement sign verified (O13); web heap stays within the Phase-B budget.

---

## 8. Phase F — Windows (MinGW-w64) parity (windows-mingw §20.4 in order)

`cmake --preset windows && cmake --build --preset windows` → `build-windows/Game/Game.exe` + `OpenAL32.dll`; import allow-list only stock system DLLs + `OpenAL32.dll`; run tree via `mkrun-hp2.sh --windows` (skips retail `*.dll *.exe`); wine + Xvfb smoke with its own prefix (`~/.wine-hp2`, display `:97`): `Game.log` `Bound to Engine`, `Bringing Level … up for play`, zero `[layout]`, no `Critical:`. Win32 bodies for the 433 `UnUnix.cpp` additions (`appCreateProc` via `CreateProcessA`). **Wine is case-insensitive: never use it as evidence that the path/case fixes work** (M6-4). No Windows UCC, no CI until the web build runs.

---

## 9. Phase G — Publish at the domain root + the Azure domain (contract §6, §10)

- Root publish: **Track A** (target `publish.folder "hp2"`, upload `<out>/hp2/` as the site — zero ufrontend code) works today; **Track B** (`publish.folder "."`, `publish.portal:false`, ~13 generic edits in `packages/build/src/publish/*` + `packages/portal`) is the clean one — owner question O2.
- Azure **Option A**: second storage account `hp2webassets` (`$web`) + second Front Door endpoint `hp2games` on the existing `unrealfd` profile, origin group `hp2-storage-og`, routes `hp2-route` (reusing the `coiheaders` rule set, zstd/brotli policy) and `hp2-telemetry-route` → the existing relay; broker `ALLOWED_ORIGINS` += the HP2 origin (`ID_PREFIXES` += `hp2` only if O4 enables P2P); the silence alert scoped per host; `deploy-ufront.py --site hp2` with `requiredFiles` instead of `requiredFolders`; `apply-outputs.sh` writes `siteUrl`/`telemetryEndpoint` into **this** clone's `publish.local.json`. Custom domain = AFD-managed cert + CNAME + `_dnsauth` TXT (owner's DNS). Clone `unreal-frontend-azure-infra` into `/home/ion/hp2-ufrontend/` before editing. **Do not run `bin/infra-deploy.sh` or upload without the owner's explicit go-ahead** — it creates billable resources and publishes content.

---

## 10. How to grind (the loop, and the parallelism)

- One phase at a time. Inside Phase A steps 3.3–3.6 (Engine / Render+Fire / SoftDrv / Editor) and Phase B steps 1–3 (NullDrv / SoundDrv / NSDLDrv) and Phase C steps 1–2 (the two forks) are **independent packages**: fan them out to parallel agents, each owning ONE directory, each committing its own files. The main thread owns Core, the launcher and every `CMakeLists.txt`. Under a usage limit, run agents **one at a time** (the owner's rule).
- The compile-fix loop: `cmake --build --preset native --target <Pkg> -j32 2>&1 | grep -E "error" | head -40`, fix, repeat. Prefer the template's spelling of a fix (search `/home/ion/engines/engine-ut99/Source` for the same error's context) over inventing one; prefer the report's file:line over re-deriving.
- When a closed library blocks: null it, note it in `docs/porting/BLOCKERS.md` with the symbol names, move on (known: `coda`/EA-XA → own decoder; `s3tc` → stubs; `MpegLib` → no caller; CJK/Thai native fonts → not for English, log as a gap).
- After every green milestone: commit, append a dated line to `docs/porting/LOG.md` (what landed, what was measured), tick the box in §12.
- Never `git` in `/home/ion/ufrontend`, `/home/ion/engines/*`, `/home/ion/epic-sources`. Never push anywhere unless the owner asks (the fork branches included — record `local:` provenance in `meta.json` until then, M4-4).

## 11. Open questions

Owner decisions (ask once, record the answer in §2):
- **O1** `travel` block: contract §1.5 data fallback (default) or the generic test/PlayPanel change (§4.3)?
- **O2** Root publish: Track A now / Track B later? New endpoint on the existing Front Door profile (Option A, recommended)?
- **O3** Ship `[HGame.baseConsole] bDebugMode=true` (debug keys, cheats, level list) in the published web ini? Default: `true` for bring-up, `false` at publish.
- **O4** Expose WebRTC multiplayer for hp2 at all (HP2 has no replicated gameplay)? Default: build the driver, hide the panel, no broker prefix.
- **O5** gl4es ref: keep the template pin `fix/cubemap-bind-active-unit` (no HP2 change needed) or push/retag `hp2`?

Verify on the first boots (do not decide in advance):
- **O6** ~~`Startup.MyLevel` vs `startup.MyLevel`~~ — **answered: `startup.MyLevel`** (the level name follows the file, `Bringing Level startup.MyLevel up for play (0)...`; `docs/porting/logs/native-first-boot.log`).
- **O7** `SetString("Core.System","paths",…)` replaces the **last** `Paths=` line? (read the written `Game.ini`).
- **O8** With `GetGlobalTime`→0, do hub persistent-actor caches survive a web reload?
- **O9** Retail D3D7 `PF_LumosAffected` quirk: blob shadows visible or not? (wine + retail `Game.exe`).
- **O10** ~~Resident heap~~ — **native answer: no `-LAZY`**: RSS 168 MB (menu), 185 MB `Ch6WizardCard`, 189 MB `Ch1Rictusempra`, 199 MB `Entryhall_hub`, 204 MB `Adv6Goyle` (SoftDrv, null audio). Re-measure the wasm heap in Phase D.
- **O11** HUD scale at 21:9 (HP2 sizes from `Canvas.SizeX`).
- **O12** Menu cursor double-move when NSDL emits both `MousePosition` and `IK_MouseX/Y` in absolute mode?
- **O13** Stereo placement sign after the openal-soft coordinate conversion.
- **O14** Do the shipped `.u` files contain `EX_DebugInfo` tokens?
- **O15** `UEngineTextureFactory` (declared package `Editor`, implemented in Engine) — any duplicate registration in the static binary? (`obj list class=EngineTextureFactory`).
- **O16** Does HP2 ever push hubs (`HubStackLevel`/`?pop` → extra `Game<i>.usa`)?

## 12. Progress checklist

- [x] Repo clean-up, git init, LF normalisation
- [x] Reference clones on their own branches beside the repo; `yarn install` in the frontend clone
- [x] CMake skeleton + template drivers copied + `tools/dev/merge3.sh`
- [x] Analysis reports + critique complete and committed; readers' tools in `tools/dev/analysis/`
- [x] P0 CMake list edits (UnTest, palette, UMasterCommandlet, UnCoreNative); web audio OFF until E; libxmp meta line dropped
- [x] A: Core (atomic header commit → UnUnix → Src files → UnMisc → UnObj/UnLinker) · Engine · Render+Fire · SoftDrv (template base) · Editor · syntax probe + varargs census · `compare.py` on all four `.u`
- [x] B: NullDrv · SoundDrv · NSDLDrv · launcher delta + native ini table · IpDrv · run tree · **boots to `startup.MyLevel` (SoftDrv, NullDrv), zero `[layout]`** · first-boot log committed · RSS measured · SoftDrv screenshots · UCC smoke
- [x] C: XOpenGLDrv hp2-v433-compat (`a5ec7d5..5b376cc`) · UTGLR hp2 (`6ad2b62..48b2681`) · native GL `Ch1Rictusempra` renders on both (Mesa llvmpipe, no GL errors); C7/H6 two-sided depth bias still open
- [x] D: web inis · W1 link · W2 smoke · W3 loader sweep (L10 `fd6910c`) · `targets/hp2` + manifests + tests (ufrontend `22aa692..42012c7`) · **web boot to the title map on XOpenGL** (`yarn e2e --target hp2`: boot, SoftDrv, direct map load, config persistence, save/load — all green; UTGLR/gl4es + NullDrv boot too) · BUILD.md web section
- [ ] E: EA-XA decoder + oracle · NOpenALDrv from ALAudio · vorbis · web audio ON
- [ ] F: Windows `Game.exe` under wine
- [ ] G: root publish track chosen · Azure Option A prepared (deploy on go-ahead)

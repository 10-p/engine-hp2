# HP2 web port — THE PLAN (read this first, then grind)

**Goal:** Harry Potter and the Chamber of Secrets (PC, Unreal Engine 1 build 433) running in a browser
through the ufrontend shell, on the exact template of the UT99 port (`/home/ion/engines/engine-ut99`).
Native Linux and Windows builds ride along as the compat oracle and parity work. **A web build running is
the finish line**; everything else is ordered by how much it de-risks that.

This document is the durable work queue. It was written after the repo clean-up, the reference cloning
and a three-way-merge dry run, while a parallel analysis pass was still writing its reports into
`docs/porting/analysis/`. Each phase lists its steps, the decisions already made (do not relitigate
them), how to verify, and what to commit. Work top to bottom; parallelise inside a phase where the steps
name independent packages.

---

## 0. State of the world (2026-09-25)

Done and committed (repo `/home/ion/hp2-ufrontend/HarryPotter2`, branch `main`):

| Commit | What |
|--------|------|
| `754be39` | Raw 2002 drop restructured into `Source/ Config/System/ Thirdparty/ Assets/(ignored) vendor/glue/` — mapping in `docs/SOURCE-LAYOUT.md` |
| `…` | docs (CLAUDE.md, README, SOURCE-LAYOUT) |
| `200241e` | CRLF → LF for every text file under `Source/` and `Config/` |
| (this commit) | CMake skeleton (`Source/CMakeLists.txt`, `CMakePresets.json`, `cmake/`, per-package `CMakeLists.txt` for Core/Engine/Render/Fire/SoftDrv/Editor), the template's portable drivers copied UNADAPTED (`NSDLDrv NullDrv SoundDrv NOpenALDrv IpDrv WebRTCDrv XOpenGLDrv OpenGLDrv Game UCC`), `Core/Src/UnStaticExports.cpp UnMinGW.cpp`, `Core/Inc/UnStaticExports.h`, `Editor/Src/NullEdHook.cpp`, `tools/`, this plan |

Workspace (all fresh clones — **never touch `/home/ion/ufrontend`, `/home/ion/engines/*`,
`/home/ion/epic-sources`**; another agent works there):

```
/home/ion/hp2-ufrontend/
  HarryPotter2/      this engine repo (git, main)
  ufrontend/         clone of 10-p/unreal-frontend-arch-private, branch hp2, `yarn install` done
  XOpenGLDrv/        clone of 10-p/XOpenGLDrv, branch hp2-v433-compat (== ut99-v400-compat today)
  utglr-copy/        clone of 10-p/utglr-copy, branch hp2 (== main today)
  gl4es/             clone of 10-p/gl4es, branch hp2 (== fix/cubemap-bind-active-unit today)
```
The CMake presets point FetchContent at these sibling clones (`FETCHCONTENT_SOURCE_DIR_XOPENGLDRV`,
`FETCHCONTENT_SOURCE_DIR_UTGLR`, `GL4ES_SOURCE_DIR`), so nothing has to be pushed for a build to work.

Toolchain on this box: gcc 15.2 (+ `gcc-multilib`, `libsdl2-dev:i386`, `libopenal-dev:i386`,
`libgl1-mesa-dev:i386`), cmake 4.2, node 22, yarn 4.17 (corepack), emsdk 5.0.7 at `/home/ion/emsdk`
(`source ~/emsdk/emsdk_env.sh`), `i686-w64-mingw32-gcc` 13.2. 32 cores, 31 GB RAM, ~50 GB free disk.

**Analysis reports** (parallel readers; may still be landing when you start): `docs/porting/analysis/`
— `template-core.md`, `template-engine.md`, `template-render-fire-softdrv-editor.md`,
`template-launcher-ucc.md`, `template-drivers.md`, `renderer-forks.md`, `hp2-core-engine-specifics.md`,
`hp2-game-boot-and-content.md`, `ufrontend-target-contract.md`, `web-layer-template.md`,
`windows-mingw-template.md`, and `00-critique.md` (contradictions + spot-checks). **Step 0 of the grind:
read the critique, then the report for the phase you are in; commit the reports if they are untracked.**
If a report is missing, the workflow script that produced them is at
`/home/ion/.claude/projects/-home-ion-hp2-ufrontend-HarryPotter2/*/workflows/scripts/hp2-port-analysis-*.js`
(readers are plain prompts; re-run the missing one as an Agent).

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
   back is a later, explicit decision.
4. **Closed binaries are worked around, never decompiled**: `coda` (EAAudioCodec), `MpegLib.lib`,
   `s3tc.lib`, `ENCVAG.LIB`, the DirectX libs, the OpenAL 1.0 SDK libs. Null them out or replace with
   open equivalents (openal-soft, libvorbis, a Layer-II decoder — §7). Their folders stay in `Thirdparty/`.
5. **Assets never in git** (`Assets/` is ignored). Inis live in `Config/System/`; the web set in
   `Config/System/web/`.
6. **Commit small, commit often**, conventional-ish subject `Pkg: what — why` (see `git log`); end every
   commit message with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>` (or the model doing the
   work). Update `BUILD.md`/`CLAUDE.md`/this plan in the same commit as the change they describe.
7. **Verify, don't assume.** Every phase has an acceptance line. A log milestone in `Game.log`
   (`Assets/…/System/Game.log` of the run tree) is the evidence, not "it compiled".

---

## 2. Decisions already made (do not reopen)

| Topic | Decision | Why |
|-------|----------|-----|
| Engine repo folder / name | `/home/ion/hp2-ufrontend/HarryPotter2`, project `HP2` | owner's folder; `publish.engine.repo` = `HarryPotter2`, `enginesRoot` = `/home/ion/hp2-ufrontend` |
| Launcher | `Source/Game/Src/Launch.cpp` → executable **`Game`** (`Game.exe` on Windows, the retail name); web artifact **`SDLLaunch.{js,wasm,data}`** (frozen cross-engine loader name) | matches HP2's `Game.exe`; the frontend hardcodes `SDLLaunch.js` |
| Original Win32 sources | kept in `Source/` (Launch, WinDrv, Window, D3DDrv, UnrealEd, FrontEnd, …), **not built** | reference for HP2 boot semantics; fidelity to the drop |
| TCHAR | **ANSI** (`char`), as the template and as HP2's own `UnGnuG.h` already assume (`TCHAR_TO_ANSI(str) str`). The MSVC build was `UNICODE`; we are not MSVC | UE1 packages serialise both; .int files are ASCII; wasm `wchar_t` is 4 bytes |
| Static single binary | ut99dc's `UNREAL_STATIC` + `UnStaticExports` registry + `--whole-archive`, with the *dynamic-style* `IMPLEMENT_CLASS` (autoclass export + `STATIC_EXPORT`). HP2's own `__STATIC_LINK` branch of `IMPLEMENT_CLASS` (Loki/PS2 scheme, `GetPrivateStaticClass##TClass`) stays **undefined** | proven with the web build and every driver of the template |
| Renderers | XOpenGLDrv (WebGL2, web default), OpenGLDrv/UTGLR + gl4es (WebGL1), HP2's own SoftDrv (`-SOFTWARERENDERER`), NullDrv (`-NULLRENDERER`). **No NOpenGLESDrv** | owner's list; NOpenGLES is not in it |
| 433 render face | forks get `#if ENGINE_VERSION==433` gates beside the existing `==400` ones: `DrawComplexSurface(…, DWORD PolyFlags, BYTE cAlpha)`, `INT MaxVertices()`, `DrawTriangles(…)`, non-pure `DrawGouraudPolygon`, `GetOsSurface` | `Source/Engine/Inc/UnRenDev.h` |
| Audio | first `SoundDrv.NullAudioSubsystem` (stubs every pure virtual of HP2's `UAudioSubsystem`), then `NOpenALDrv` = HP2's `ALAudio` ported to OpenAL 1.1 / openal-soft (the `10-p/openal-soft-web` fork, `cmake/OpenALSoft.cmake`) + libvorbis for `Music/*.ogg` + an open MPEG-1 Layer II decoder for `SF_Compressed` sounds if that is what they are (§7 decides from bytes) | HP2's `UAudioSubsystem` is not v400's; the template's `NOpenALDrv` is a *pattern* (web backend, HRTF, `Module.alsoftConfig`), not a drop-in |
| Networking | IpDrv ported from the template (433-adapted); `WebRTCDrv` ported for P2P; `IpDrv.u` script classes only if `ucc make` compiles them cleanly, otherwise TcpNetDriver alone | HP2 shipped no IpDrv; `Default.ini` still names `IpDrv.TcpNetDriver` |
| Editor | stripped `NullEdHook` Editor linked **ON** (template rule 2.39) unless the content analysis proves HP2 maps reference no Editor classes | runtime dependency in UE1 |
| Frontend target | `targets/hp2/` in the ufrontend clone; `storage.dbPrefix = "hp2"`, `keyPrefix = "hp2"` → IndexedDB `hp2-user-data`, `hp2-package-cache`, … ; `network.peerPrefix = "hp2"`; `publish.folder` = root (no portal, no selector) | owner: prefix `hp2-user-data`; "another azure domain that loads the frontend straight away" |
| Line endings | LF everywhere in `Source/`, `Config/` | template is LF; diffs against epic need `--strip-trailing-cr` |
| Music path | `Assets/Music/*.ogg` (HP2 streams Ogg files by name; there is no `.umx`) — `Paths=` may need `../Music/*.ogg` for FetchFS/manifest purposes; verify how `PlayMusic` opens the file | `Source/Engine/Inc/UnAudio.h` `PlayMusic(FString Song, …)` |

---

## 3. Phase A — Core, Engine, Render, Fire, SoftDrv, Editor compile natively (Linux, 32-bit)

**Method: three-way merge, base = epic v400, ours = HP2 433, theirs = engine-ut99.** The dry run
(`tools/dev/merge3.sh <outdir>`; results were in the session scratchpad, re-run it) classified every
file under `Core Engine Render Fire SoftDrv Editor`/`{Inc,Src}`:

| Class | Count | Action |
|-------|-------|--------|
| `THEIRS-UNCHANGED` | 119 | nothing to do (ut99dc did not touch it) |
| `CLEAN` | 49 | copy the merged file in, review the diff once, build |
| `OURS-UNCHANGED (take theirs)` | 6 | copy the template's file: `Core/Inc/FOutputDeviceAnsiError.h UnUnix.h`, `Engine/Src/UnMover.cpp UnURL.cpp`, `SoftDrv/Src/DrawTile.cpp SoftDrv.cpp` |
| `CONFLICTS` | 41 | resolve by hand, file by file (guidance below) |
| `HP2-ONLY` | 62 | keep HP2's file; port the template's *generic* fixes by analogy where the file has the same role |
| ut99dc-only | copied already (`UnStaticExports.*`, `UnMinGW.cpp`, `NullEdHook.cpp`); NOT copied: `UnFilePool.cpp` (Dreamcast), `Core/Src/UnLinker.h` (HP2 has `Core/Inc/UnLinker.h`), `Render/Src/RenderPrivate.h` (HP2's is `Render/Inc/Render.h`), `Fire/Src/FractalPrivate.h` (HP2's is `Fire/Inc/UnFractal.h`) — port their hunks (Dreamcast bits excluded; the `GCC_ALIGN(4)` on trailing `BYTE` members in FractalPrivate.h and the `!defined(__MINGW32__)` `__int64` guard matter) |

**Conflict list (dry run), hardest first:** `Core/Inc/UnGnuG.h`(12) `Core/Inc/UnTemplate.h`(9)
`Engine/Src/UnAudio.cpp`(8) `Engine/Src/UnGame.cpp`(7) `Editor/Src/UnMeshLP.cpp`(7)
`Render/Src/UnMeshRnLOD.h`(6) `Core/Inc/UnScript.h`(6) `Render/Src/UnRender.cpp`(5)
`Engine/Inc/EngineClasses.h`(5) `Core/Src/UnUnix.cpp`(5) `Core/Inc/UnMath.h`(5) `Render/Src/UnLight.cpp`(4)
`Core/Src/UnObj.cpp`(4) `Core/Src/UnCorSc.cpp`(4) `Engine/Src/UnPhysic.cpp`(3) `Core/Src/UnClass.cpp`(3)
`Core/Inc/UnObjBas.h`(3) + 24 files with 1–2 hunks.

Guidance per hard file (from reading the hunks):
- **`UnGnuG.h`** — do not merge; **take the template's file and re-add HP2's 433 bits**: `GCC_ALIGN`/`GCC_MOVE_ALIGN`,
  `OWORD` (drop the `mode(TI)` typedef unless used), `FTime`-returning `appSeconds()` / `appSecondsSlow()`
  and `FLOAT GSecondsPerCycle` (HP2's `Core.h` defines `FTime`; Engine uses it — check every caller before
  deciding the return type), the `__PSX2_EE__` branches deleted. Keep the template's `appRound`/`appFloor`
  (`floorf` versions — they fixed the software-renderer streaks), `appCycles()`/`appSeconds()` out-of-line
  (SDL performance counter in `UnUnix.cpp`), `PATH_MAX`, `stdint` typedefs, `SIZE_T = size_t`,
  `IMPLEMENT_PACKAGE_PLATFORM` with `STATIC_EXPORT`.
- **`UnObjBas.h`** — keep HP2's 4-arg `DECLARE_CLASS(TClass,TSuperClass,TStaticFlags,TPackage)` and
  apply the template's two fixes inside it: placement-new **without** parentheses in `InternalConstructor`
  (`new(X)TClass` — value-init zeroes the UObject header under modern C++), and `IMPLEMENT_CLASS` →
  the autoclass export + `STATIC_EXPORT( TClass, autoclass##TClass )`. Delete the `__STATIC_LINK` branch.
- **`UnScript.h`, `UnCorSc.cpp`, `UnClass.cpp`** — HP2 already has the unaligned-safe `appMemcpy` reads
  under `__PSX2_EE__`; the template has them under `PLATFORM_DREAMCAST`. Take the plain x86 paths (ours
  `#else`), drop both guards; keep HP2's `P_FINISH` with the `EX_DebugInfo` step (433 debugger opcode —
  the shipped `.u` files may contain it) and HP2's `Clamp<INT>` fix from the template where it applies.
  Add the template's **layout guard** (`GLayoutStructs` in `UStruct::UStruct(EStaticConstructor…)` and the
  check in `UStruct::Link`) — HP2's `Link` aligns to `PropertiesAlign`, keep that.
- **`UnTemplate.h`** — the conflicts are `this->` qualifications for dependent names in `TTransArray`/`TMap`
  (GCC two-phase lookup) landing on HP2's different surrounding code: apply the `this->` idea to HP2's text.
- **`UnMath.h`** — delete every `__PSX2_EE__` block (the 16-byte `FVector` with `W`, the VU0 asm); keep
  HP2's `GMath` tables (`SqrtManTbl`, `DivSqrtManTbl`, … + `Power()` approximations); the template's
  Dreamcast/LOW_MEMORY branches are not wanted. Fix the `ASMLINUX` transform paths to plain C (`#if ASM` off).
- **`UnUnix.cpp`** — HP2's is the Loki-era Linux core (fork/exec, the full signal table, `stricmp`…);
  the template's is ut99dc's SDL-based rewrite (`appSeconds` via `SDL_GetPerformanceCounter`, `appCycles`,
  `SDL_GetPlatform`, `appGetDllHandle` from the static registry, `appBaseDir()=getcwd`). **Take the
  template's file and re-add HP2's 433 additions** (`FTime`, `AudioMutex` if still needed, any `app*`
  HP2's Engine calls that ut99dc lacks — the linker will tell you).
- **`UnObj.cpp`** — take the template's iterative `FArchiveTagUsed` (worklist; needed on the web's stack),
  HP2's `FPushMemTag`/`RefObj` extras can go unless something calls `GetReferencing()`; take the
  template's constructor-runs-for-every-class comment/behaviour.
- **`UnMisc.cpp`** — `FArray::Realloc/Remove` moved in HP2 (where? `UnTemplate.h`?) — resolve by keeping
  one definition; `GMem.Init(65536, TEXT("GMem"))` is HP2's signature, keep it.
- **`UnName.cpp`** — HP2 has `FName::HardcodedNames[]`; the template forward-declares
  `AllocateNameEntry`. Keep HP2's, add the template's declaration if it uses it.
- **`EngineClasses.h`** (generated from `Engine/Classes/*.uc`; **must match `Assets/System/Engine.u`, never
  regenerate it**) — only take the template's *packing* edits by analogy (`GCC_PACK`/`GCC_ALIGN` on trailing
  bitfields such as `ABrush::bColoredPad`) if the layout guard reports a mismatch; otherwise leave it.
- **`UnAudio.cpp`, `UnGame.cpp`, `UnPhysic.cpp`, `UnLevTic.cpp`, `UnPath.cpp`, `UnActCol.cpp`** — HP2's
  433 code is authoritative; the template hunks are GCC portability (`#if ASM`, varargs, `Clamp<INT>`,
  loop-variable scoping), the 'Received' log line (UnGame.cpp), FrameRateCap (UnGame.cpp/UnEngine.cpp),
  AutoFOV (find where the template put it — `grep -rn AutoFOV /home/ion/engines/engine-ut99/Source`), the
  exception-free `NULL` returns. Port each by hand into HP2's text.
- **Render `UnRender.cpp`/`UnLight.cpp`/`UnMeshRnLOD.h`/`UnMeshRn.cpp`/`UnSprite.cpp`** — HP2's files are
  heavily 433 (skeletal meshes, particles). Template hunks are ASM-off paths, `DivSqrtApprox` helpers from
  `RenderPrivate.h` (HP2: `Render/Inc/Render.h`), and the `2.16 AutoFOV` if it lives here. Apply by hand.
- **SoftDrv `SoftDrvPrivate.h`/`DrawPoly.cpp`** — HP2's 433 versions; take the template's
  `SOFTDRV_MAX_SCREEN` table bound, `ASM=0` fallbacks, `GIsMMX` force-off. Add the 433 face to nothing —
  HP2's SoftDrv already implements it.
- **Editor `UnMeshLP.cpp`/`UnScrCom.cpp`/`UnEdCsg.cpp`** — template compile fixes (`PUSH_HIT`
  `sizeof→decltype`, `Min<INT>`, `UnParams` duplicate-symbol guard under `UNREAL_STATIC`).

HP2-only hazards to handle in this phase (details: `analysis/hp2-core-engine-specifics.md`):
- `Core/Inc/UnCoreNative.h` `DECLARE_NATIVE_TYPE` uses MSVC token-pasting (`##type::*##type##Native`) —
  rewrite as standard `##` pasting; `GNativeLookupFuncs[32]` + `IMPLEMENT_NATIVE_HANDLER` must work in the
  static binary (each package's table registered at load — check `UnCoreNative.cpp`/`UnEngineNative.cpp`).
- `Core/Src/AudioMutex.cpp` (Win32 `CRITICAL_SECTION` re-typed by hand): port to `pthread_mutex` /
  `SDL_mutex`, or stub if no caller survives (`grep -rn AudioMutex Source/Engine Source/ALAudio`).
- `Core/Src/UnPSX2.cpp`, `FFileManagerPSX2*.h`: not built, leave in place.
- `Core/Inc/UnStat.h` / `Engine/Inc/UnStats.h`: `STATS 1` stat tables — make sure they compile with `ASM=0`.
- `Core/Src/UnVcWin32.cpp` is MSVC-only (inits `coda` and vorbis allocators) — never compiled here.
- `Engine/Src/UnTex.cpp` calls `S3TCencode/S3TCgetEncodeSize` (closed `s3tc.lib`): guard the **encode**
  path out (editor import); decide on the **decode** path after checking whether any `Assets/Textures/*.utx`
  carries compressed mips (`analysis/windows-mingw-template.md` §s3tc) — if yes, replace `S3TCdecode` with an
  open DXT1 decoder (tiny; write it).
- `Engine/Inc/UnEngineWin.h`, `Engine/Src/winstuff.cpp`: never included/compiled on GCC targets — make sure
  no GCC-compiled file includes `UnEngineWin.h` (the launcher rewrite in Phase B replaces it).

Steps:
1. `tools/dev/merge3.sh /tmp/merge-hp2` → apply `CLEAN` + `take theirs` into `Source/` (`cp`), commit
   ("Core/Engine/…: apply the template's clean three-way merges"). Re-run the dry run any time the tree changes.
2. Resolve `Core` conflicts (list above), then `cmake --preset native && cmake --build --preset native --target Core -j32`
   until Core links. Commit per file group.
3. Engine → Render → Fire → SoftDrv → Editor, same loop (each is `--target <Pkg>`). These five can be worked
   **in parallel by separate agents** once Core's headers are stable (they only depend on Core/Engine headers).
4. Run the layout guard early: as soon as `Game` boots (Phase B), `[layout] native X: C++ sizeof=… but
   script layout=…` warnings in the log name every reflected class whose C++ size disagrees with
   `Engine.u`/`Core.u`. Fix with packing attributes, not by editing the `.u`.

**Acceptance A:** `cmake --preset native && cmake --build --preset native -j"$(nproc)"` builds the static
libraries `Core Engine Render Fire SoftDrv Editor` (the drivers may still be off at this point:
`-DBUILD_GAME=OFF -DBUILD_XOPENGLDRV=OFF -DBUILD_NOPENGLDRV=OFF -DBUILD_NOPENALDRV=OFF` and skip
`NSDLDrv/NullDrv/SoundDrv/IpDrv` by temporarily commenting their `add_subdirectory` — restore before B).

---

## 4. Phase B — Drivers + launcher: the native oracle boots to the main menu

Adapt the copied template drivers to HP2's 433 interfaces (`analysis/template-drivers.md` has the
member-by-member list):

1. **NullDrv** (`Source/NullDrv/Src/NullDrv.cpp`): add `MaxVertices()`, `DrawTriangles(…)`,
   `DrawComplexSurface(…, DWORD PolyFlags, BYTE cAlpha)`, `GetOsSurface`; 4-arg `DECLARE_CLASS(…,NullDrv)`.
2. **SoundDrv** (`Source/SoundDrv/Null.cpp`): stub **every** pure virtual of HP2's `UAudioSubsystem`
   (`Source/Engine/Inc/UnAudio.h`: `Update(FSceneNode*)`, `RegisterSound`, `UnregisterSound(USound*,INT)`,
   `PlaySound(…,Flags,FadeDuration,Priority)`, `StopSound` ×2, `PlayMusic`, `StopMusic`, `StopAllMusic`,
   `CleanUp`, `PostRender`, `ModifySound`, …). Return sensible values (`PlaySound` → a fake handle ≥ 1).
3. **NSDLDrv** (`NSDLClient.cpp`, `NSDLViewport.cpp`, `NSDLDrv.h`): diff HP2 `Engine/Inc/UnCamera.h` vs
   epic (33 lines) and `UnIn.h`/`Input.uc` (EInputKey) and adapt; the GL-vs-software surface choice keys on
   the device name containing `OPENGL`; HP2's mouse-gesture spell casting needs raw relative mouse — make
   sure `IK_MouseX/Y` deltas reach `UInput` as in the template.
4. **Game launcher** (`Source/Game/Src/Launch.cpp`, from the template's `UnrealTournament/Src/Launch.cpp`):
   keep every template feature — the renderer switch table (`-SOFTWARERENDERER -NULLRENDERER
   -XOPENGLRENDERER -UTGLRRENDERER`), `-ini` / `-ini:<File>:[Section]:Key=` seeding, `-userini`, ini
   normalisation (ViewportManager→`NSDLDrv.NSDLClient`, AudioDevice→`NOpenALDrv…` when built else
   `SoundDrv.NullAudioSubsystem`, NetworkDevice→`WebRTCDrv…` on Emscripten), `FrameRateCap`, AutoFOV,
   `-log`, GLog→stdout, chdir to `System`, the web main loop; **add HP2's boot semantics** from
   `Source/Launch/Src/Launch.cpp` + `Engine/Inc/UnEngineWin.h` (`analysis/template-launcher-ucc.md`):
   `GPackage = "Game"` → `Game.ini`/`User.ini` derived from `Default.ini`/`DefUser.ini`; `[FirstRun]`
   `FirstRun` stamped to `ENGINE_VERSION` with **no** config dialogs; `Language` from ini; `-SAVESLOT=`,
   `-NOFRONTEND`, `-EXEC=` honoured; splash waits dropped (headless equivalent: nothing). Rename the
   template's `UT_*` defines (`Game/CMakeLists.txt` already says `HP2_WEB_PERF`, `HP2_HAS_NOPENALDRV`,
   `HP2_ASSETS_DIR`, `--target=hp2`).
5. **UCC** (`Source/UCC/Src/UCC.cpp`, `CMakeLists.txt` already without UWeb): apply the template's
   changes (static whole-archive link, `GIsClient` off, `__LINUX__` file-manager select).
6. **IpDrv**: 433-adapt (`UnNetDrv.h`/`UnConn.h`/`UnNet.h`/`UnBunch.h`/`UnChan.h` diffs vs epic are small);
   drop the Dreamcast `pthread` block in its CMake; the `WebRTCLan.h` include stays (header-only, one-way).
   `IpDrv.u`: try `ucc make` with the v400 `IpDrv/Classes` after `Game` boots; if it fails, ship without
   (TcpNetDriver does not need the script classes).
7. **Run tree**: `tools/dev/mkrun-hp2.sh <build-dir> <run-dir>` (adapt `mkrun-ut99.sh`): symlink
   `Assets/{Maps,Textures,Sounds,Music,Fonts,Help,SystemLocalized}`, copy `Assets/System` (the engine writes
   `Game.ini`/`Game.log` there), copy `Config/System/{Default,DefUser}.ini` in, copy the `Game` binary (+
   `libopenal.so.1` when NOpenALDrv is on). `run.sh` adapted from `run-template.sh` (`-SOFTWARERENDERER` default).
8. Boot: `cd run/System && ./Game -SOFTWARERENDERER -log` (Xvfb: `Xvfb :99 -screen 0 1280x720x24 & DISPLAY=:99 …`).
   Chase `Game.log` to `Bringing Level Startup.MyLevel up for play` and the HPConsole menu; fix load failures
   one at a time (`Can't find file for package`, `Can't bind to native class`, `[layout]` warnings).
   `tools/dev/shot.sh` takes a screenshot of the X display.

**Acceptance B:** `Game.log` reaches the main menu on `-SOFTWARERENDERER` and on `-NULLRENDERER` with zero
`[layout]` warnings and no `Critical:`; `open Ch1Rictusempra` (or another map named in
`analysis/hp2-game-boot-and-content.md`) loads and ticks for 30 s. Commit + write `BUILD.md` (native section).

---

## 5. Phase C — Hardware renderers on the 433 face (native first)

1. **XOpenGLDrv** (`/home/ion/hp2-ufrontend/XOpenGLDrv`, branch `hp2-v433-compat`; build glue
   `Source/XOpenGLDrv/CMakeLists.txt` already fetches it): add `#if ENGINE_VERSION==433` alongside `==400`
   for every gated site (`analysis/renderer-forks.md` lists them), implement `MaxVertices()`,
   `DrawTriangles(Frame, Info, Pts, NumPts, Indices, NumIndices, PolyFlags, Span)` (index-list version of
   `DrawGouraudPolygon`/its `DrawGouraudTriangles` buffer path), `DrawComplexSurface` with `PolyFlags/cAlpha`
   (alpha-modulated surfaces — find what Render passes for `cAlpha`), `GetOsSurface` stub. Commit on the
   fork branch; the engine builds from the sibling clone.
2. **OpenGLDrv/UTGLR** (`/home/ion/hp2-ufrontend/utglr-copy`, branch `hp2`): same set, gated on a new
   `UTGLR_ENGINE_HP2` define (added in `Source/OpenGLDrv/CMakeLists.txt` beside `UTGLR_ENGINE_UT99DC`).
3. **gl4es**: expected unchanged; bump only if UTGLR needs a GL feature the fork lacks.
4. Native GL check: `./run.sh gl` (XOpenGL) and `-UTGLRRENDERER` show the menu and a map; screenshot both.

**Acceptance C:** both hardware renderers render the menu and a map natively without GL errors in the log.

---

## 6. Phase D — The web build and the frontend target (THE GOAL)

Engine side (`analysis/web-layer-template.md` is the checklist; the CMake `if(EMSCRIPTEN)` block and
`Source/Game/CMakeLists.txt` link flags are already the template's):
1. `source ~/emsdk/emsdk_env.sh && cd Source && cmake --preset wasm && cmake --build --preset wasm -j32`.
   Fix what clang rejects (non-POD varargs, `-fpack-struct=4` warnings-as-errors, `__asm`, `__int64`).
2. Port the **exception-free load-failure `NULL` sites** the template patched in `UnObj.cpp`/`UnLinker*`/
   `UnGame.cpp`/`UnLevel.cpp` (`grep -rn "__EMSCRIPTEN__" /home/ion/engines/engine-ut99/Source/Core
   /home/ion/engines/engine-ut99/Source/Engine`) into HP2's equivalents (HP2 split the linker into
   `Core/Src/UnLinker.cpp`).
3. `Launch.cpp` web paths (main-loop inversion via `emscripten_sleep`, `ue_exec_command`, `appRequestExit`,
   status `EM_ASM` lines, the `Received '<pkg>'` log line the host persists on); `NSDLViewport.cpp` web
   context creation on the worker (`OFFSCREENCANVASES_TO_PTHREAD`, `emscripten_webgl_commit_frame`).
4. **`WebRTCDrv`**: 433-adapt (`UNetDriver`/`UNetConnection` diffs); `WebRTCLan` can stay compiled but is
   inert without a server browser.
5. **`Config/System/web/Default.ini` + `DefUser.ini`**: HP2's `Default.ini` with `ViewportManager=NSDLDrv.NSDLClient`,
   `GameRenderDevice=XOpenGLDrv.XOpenGLRenderDevice`, `AudioDevice=SoundDrv.NullAudioSubsystem` (→ NOpenALDrv
   in Phase E), `NetworkDevice=WebRTCDrv.WebRTCNetDriver`, `[WebRTCDrv.WebRTCNetDriver]` section (copy the
   template's values), `[Engine.Engine] FrameRateCap=120`, `[ufront.AutoFOV]`, `[NSDLDrv.NSDLClient]`
   1280×720 windowed, `[XOpenGLDrv.XOpenGLRenderDevice]` + `[OpenGLDrv.OpenGLRenderDevice]` +
   `[NullDrv.NullRenderDevice]` + `[SoftDrv.SoftwareRenderDevice]` sections, `Paths=` incl. `../Music/*.ogg`
   if the music loader resolves through `GSys->Paths`. Drop the GameSpy/master-server `ServerActors`.
   Write `Config/System/web/README.md`.
6. `Source/Game/cmake/emit-artifact-meta.mjs` → `targetId: "hp2"`, `provenance.repo: "HarryPotter2"`;
   `offscreen-depth24.sh` unchanged; `tools/web-smoke.mjs` adapted (asset root, expected log milestones).
7. `cmake --preset wasm-perf && cmake --build --preset wasm-perf` → `build-wasm-perf/SDLLaunch/SDLLaunch.{js,wasm,data}` + `meta.json`.

Frontend side (`/home/ion/hp2-ufrontend/ufrontend`, branch `hp2`; `analysis/ufrontend-target-contract.md`):
8. `targets/hp2/target.json` (id `hp2`, `engineFamily` UE1, `abiVersion` 1, `publish` block with
   `engine.repo "HarryPotter2"`, `engine.branch "main"`, `presetDir "Source"`, `folder` for a ROOT publish,
   `assetMarkers.fingerprint` = sha256 of `Assets/System/Core.u` (61,685 bytes), `require`
   `["System/HGame.u","Maps/startup.unr"]`, `storage {dbPrefix:"hp2", keyPrefix:"hp2", cacheBudgetMb, memfsBudgetMb}`,
   `network` (playerName in `System/User.ini [DefaultPlayer] Name`; serverName — HP2 has
   `Engine.GameReplicationInfo`? verify in `Source/Engine/Classes`, else the `WebRTCDrv.WebRTCNetDriver`
   section like unreal1), `mods.extToFolder` (`.unr Maps .utx Textures .uax Sounds .ogg Music .u/.int/.ini System .usa Save`),
   `fetchStubs`, `manifestExclude` (`*.dll *.exe *.bat *.dsp System/Game.ini System/User.ini System/Default.ini
   System/DefUser.ini *.usa Logs/** Maps/Oldfiles/** Maps/Studies/** Maps/templates/** EditorRes/**`),
   `userFiles ["Save/*.usa"]`, `defaults` (a directly loadable map + empty game), `videoDrivers` (XOpenGL,
   UTGLR, Software, Null), `frameRateCap`, `conversions []` (unless the web search in the content report
   found real ones), `travel` (single-player: modes/toggles/presets/mutators minimal or empty — check what
   the frontend/tests tolerate), `capabilities`, `notices`/`telemetry` (mark TO-VERIFY, then verify against a
   captured log — the skeleton test asserts them against real lines).
9. `targets/hp2/branding/{theme.css,logo.svg,help.md,portal-notes.json}`, `README.md`.
10. Manifests: `node packages/glue/bin/generate-manifest.cjs /home/ion/hp2-ufrontend/HarryPotter2/Assets
    targets/hp2/file-manifest.json --exclude …` (the exclude list from target.json, recipe in `docs/06`),
    `yarn deps`, `precache.json` = the boot working set (Core, Engine, Editor, Fire, UWindow, UnrealShare,
    HGame, HPModels, HPParticle, HProps, HPSounds, startup.unr + its textures/sounds — sizes in the content report).
11. `test/skeleton.test.mjs`: add `hp2` to `TARGETS` and telemetry fixtures; `yarn test` green; the
    gates in `.ufront/gates` (doc-sync, state-updated, no-engine-specifics) — update `docs/migration/STATE.md`
    + a `docs/migration/steps/hp2-*.md` note as the gates demand.
12. `.ufront/publish.local.json`: `{"out":"/home/ion/hp2-ufrontend/publish","assets":{"hp2":"/home/ion/hp2-ufrontend/HarryPotter2/Assets"},"enginesRoot":"/home/ion/hp2-ufrontend"}`.
13. `yarn build --target hp2 --out serve-web-hp2` (points at `build-wasm-perf/SDLLaunch` — check how the
    build resolves the artifact dir; drop it in `targets/hp2/engine-artifact/` if that is the contract) then
    `yarn serve --root serve-web-hp2` and open `http://localhost:8080/`. Then the boot E2E adapted from the
    UE1 boot spec in `packages/e2e` (headless Chromium with GPU guard), or `tools/web-smoke.mjs`.
14. **Root publish, no selector**: make `yarn publish-all` able to publish one target at `<out>/` with no
    portal `index.html` (a `publish.folder: "."`/`--no-portal` path in `packages/build/src/publish/*`,
    `packages/portal`) — the contract report names the exact files.

**Acceptance D:** the wasm build boots in headless Chromium to the HP2 main menu on XOpenGL, loads a map
from the Play panel, and `yarn test` is green in the ufrontend clone. `BUILD.md` web section written.

---

## 7. Phase E — Audio (`NOpenALDrv` for the 433 `UAudioSubsystem`)

1. Establish the sound format from bytes (`analysis/template-drivers.md`): `USound` = `FileType`,
   `CoreFlags` (`SF_Compressed|SF_Streaming` set at import for compressed sounds), `raw_*` fields, `Data`,
   `LipSyncData`. Compressed data is most likely **MPEG-1 Layer II** (the `Tools/Audio/MP2 encoder/TooLame`
   and `Thirdparty/MPeg/MpegLib.lib` say so); plain sounds are RIFF WAVE. Music is `Music/*.ogg`.
2. Write `Source/NOpenALDrv/NOpenALDrv.cpp` for HP2 by porting `Source/ALAudio/Src/ALAudioSubsystem.cpp`
   (HP2's own, already the right interface) to OpenAL 1.1 (openal-soft via `cmake/OpenALSoft.cmake`, EFX
   reverb instead of EAX), libvorbis (`FetchContent` xiph/ogg + xiph/vorbis, static, works under emcc) for
   streaming music, and an open Layer-II decoder for compressed sounds (candidates: libmpg123 via
   FetchContent, or a small permissive decoder; the owner said "whatever ffmpeg implemented for it" — pick
   the smallest thing that decodes the actual bytes). Keep the template's web bits: `Module.alsoftConfig`,
   `UseHRTF`, `OutputRate`, no EAX symbols linked (features chosen at run time).
3. `Config/System/web/Default.ini` `AudioDevice=NOpenALDrv.NOpenALAudioSubsystem`; `[NOpenALDrv.NOpenALAudioSubsystem]`
   section; target.json `audioBuffer`/`audioHrtf` blocks.

**Acceptance E:** menu music and spell SFX audible natively and on the web; lip-synced dialog plays.

---

## 8. Phase F — Windows (MinGW-w64) parity

`cmake --preset windows && cmake --build --preset windows` → `build-windows/Game/Game.exe`, self-contained
(`objdump -p` shows only system DLLs + `OpenAL32.dll` beside it). Rekey `_MSC_VER`/`__GNUG__`-meaning-OS
guards onto `PLATFORM_WIN32`; `-fpack-struct=4`; check `ABrush::bColoredPad`-style pads for 433. Verify
with `wine` + `Xvfb` (`analysis/windows-mingw-template.md`). Lower priority than D/E.

---

## 9. Phase G — The Azure domain

Prepare only: a second storage static website + Front Door endpoint on the existing profile (COOP/COEP
headers, zstd `Content-Encoding`), reusing the PeerJS broker and telemetry relay
(`/home/ion/unreal-frontend-azure-infra/config.json` is the shape; clone that repo into
`/home/ion/hp2-ufrontend/` before editing). **Do not run `bin/infra-deploy.sh` or upload without the
owner's explicit go-ahead** — it creates billable resources and publishes content.

---

## 10. How to grind (the loop, and the parallelism)

- One phase at a time; inside Phase A steps 3 (Engine/Render/Fire/SoftDrv/Editor) and Phase B steps 1–3
  (NullDrv/SoundDrv/NSDLDrv) and Phase C steps 1–2 (the two forks) are **independent packages**: fan them
  out to parallel agents, each owning ONE directory, each committing its own files. The main thread owns
  Core, the launcher and every `CMakeLists.txt`.
- The compile-fix loop: `cmake --build --preset native --target <Pkg> -j32 2>&1 | grep -E "error|Error" | head -40`,
  fix, repeat. Prefer the template's spelling of a fix (search `/home/ion/engines/engine-ut99/Source` for
  the same error's context) over inventing one.
- When a closed library blocks: null it (`#if 0`-style stub returning failure, or a replacement), note it
  in `docs/porting/BLOCKERS.md` with the symbol names, move on.
- After every green milestone: commit, append a dated line to `docs/porting/LOG.md` (what landed, what was
  measured), and tick the box below.
- Never `git` in `/home/ion/ufrontend`, `/home/ion/engines/*`, `/home/ion/epic-sources`. Never push
  anywhere unless the owner asks.

## 11. Progress checklist

- [x] Repo clean-up, git init, LF normalisation
- [x] Reference clones on their own branches; `yarn install` in the frontend clone
- [x] CMake skeleton + template drivers copied + `tools/dev/merge3.sh`
- [ ] Analysis reports committed (`docs/porting/analysis/*.md`), critique read
- [ ] A: Core compiles · Engine · Render · Fire · SoftDrv · Editor
- [ ] B: NullDrv · SoundDrv · NSDLDrv · Game launcher · UCC · IpDrv · run tree · **boots to menu (SoftDrv, NullDrv)**
- [ ] C: XOpenGLDrv hp2-v433-compat · UTGLR hp2 · native GL menu + map
- [ ] D: wasm build links · NULL-sites · web inis · WebRTCDrv · meta.json · `targets/hp2` · manifests · tests · **web boot to menu** · map load · root publish
- [ ] E: NOpenALDrv (OpenAL Soft + vorbis + Layer II) native + web
- [ ] F: Windows Game.exe under wine
- [ ] G: Azure domain prepared (deploy on go-ahead)

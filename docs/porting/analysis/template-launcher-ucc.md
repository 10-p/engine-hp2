# Template analysis: launcher and UCC (key: template-launcher-ucc)

Scope: (1) the UT99 web-port SDL launcher (engine-ut99/Source/UnrealTournament) feature-by-feature, stock vs ufront additions;
(2) HP2's original Win32 launcher (Source/Launch + Engine/Inc/UnEngineWin.h + Source/FrontEnd) and the spec for the new SDL
launcher `Source/Game/Src/Launch.cpp` (exe `Game`, web artifact `SDLLaunch.js`); (3) UCC template vs HP2 UCC, and
whether `ucc make` can build v400 IpDrv.u against HP2 Engine.u.

Status: complete (see the last line). Re-run 2026-09-26 (Opus): §1 re-verified against the sources (errata marked inline), §2-§4 new, §0 filled last.

## 0. Summary / action checklist

**Bottom line.** `Source/Game/Src/Launch.cpp` is still a byte-identical copy of the UT99 template; the port is a
*delta* (§2.6.2), not a rewrite. Most "launcher features" actually live in Core/Engine (§1.1 table) and are owned
by those analysts; the launcher itself needs ~15 small edits. HP2's Win32 boot (SafeDisc, splashes, WFrontEnd slot
picker, D3D detection, CD check) is **dropped**; its *semantics* survive as argv contracts (`PrivetDr.unr
-SAVESLOT=N`, `-LOAD=0 -SAVESLOT=N`) that the web frontend must produce (§2.5). UCC: take the template's `UCC.cpp`
with 3 edits (§3.3). `IpDrv.u` is **not needed** (TcpNetDriver binds without it) but *can* be compiled from the v400
classes against HP2's Engine.u — the script API is fully present; the risks are in the tool (§3.5 M1-M8).

**Launcher (`Source/Game/Src/Launch.cpp`) — ACTION checklist** (details §2.6.2)
- [ ] Delete Dreamcast blocks, `SimpleCopy`, `~/.utconf` swap.
- [ ] Remove `NOPENGLESRENDERER` row + preference; optional SoftDrv fallback when no HW renderer is compiled (Phase B).
- [ ] Bound the argv join (`appStrncat`); `GModule` = basename(argv[0]) via `appStrncpy` (fixes 32-byte overflow + log name).
- [ ] `appInit( TEXT("Game"), … )`; keep `GIsClient=1; GIsGuarded=0`.
- [ ] Native chdir to the exe dir (or Core adopts template `appBaseDir`) — the exe lives in `System/`.
- [ ] Web driver block: NSDLDrv; audio = NOpenALDrv if `HP2_HAS_NOPENALDRV` else `SoundDrv.NullAudioSubsystem`; WebRTCDrv; CaptureMouse; bWorldLog/bLocalLog; **delete** the IpServer line.
- [ ] Native `NormalizeSubsystem` lists: viewport NSDLDrv; audio NOpenALDrv→SoundDrv; net TcpNetDriver.
- [ ] `ServerActors.Empty()` on **all** platforms (HP2 ini names IpServer/UWeb which do not exist).
- [ ] FirstRun: read-then-stamp 433 with a log line; no dialogs; ignore Reconfig/ForceSoftware; optional RunCount++.
- [ ] FTime rewrite of `InitEngine` load time, `MainLoop`, pacing ring, `ue_exec_command` printf (§1.8, §2.2.3).
- [ ] `CleanUpOnExit(ErrorLevel, Engine)`: `Engine->Audio->SetViewport(NULL)` before `appPreExit` (HP2 retail).
- [ ] `HandleError`: skip the SDL message box when headless (`-NOMSGBOX` / dummy video driver).
- [ ] Write `Config/System/web/{Default,DefUser}.ini` before the first wasm link (`LINK_DEPENDS`).
- [ ] `emit-artifact-meta.mjs`: HP2 provenance strings.

**Owned elsewhere but launcher-blocking** (hand-offs)
- Core: `appApplyIniOverrides` (call **after** the gk Language block), `appIsGlobalOptionToken` (+ Engine `UnGame.cpp:180-185`), `appUserDir()` = `""` on Unix, `FFileManagerLinux` from the template (mkdir mode, GetGlobalTime=0), `FindFiles` flag handling (§3.5 M3), `appRequestExit` web branch, GLog→stdout echo, layout guard, SavePath `..\Save` literal (A5), `Save Slot Path` FString-varargs trap (§2.6.4 item 4).
- Engine: AutoFOV port, web-mouse block, web startup-map fallback, `Received` debugf in `UDownload::DownloadDone`.
- Frontend: slot picker → argv; `userFiles` per-slot globs; `conversions: []`; restart to change slot.

**UCC — ACTION checklist** (§3)
- [ ] Replace `Source/UCC/Src/UCC.cpp` with the template file; edits: `appInit(TEXT("HP"))` (retail, Q-UCC1), GModule basename, bounded join.
- [ ] Keep `Source/UCC/CMakeLists.txt` (already template minus UWeb); decide MinGW gating (Q-UCC2).
- [ ] Acceptance §3.7: `help`, `Core.SimpleCommandlet` golden output, `batchexport HGame.u` as a headless load check.
- [ ] Optional: `IpDrv.u` via `make -NOHEADER` in a scratch tree with a dedicated ini (M3-M6).

## 1. UT99 template launcher (engine-ut99/Source/UnrealTournament)

### 1.1 Files and build target

| File | Lines | Role |
|------|-------|------|
| `engine-ut99/Source/UnrealTournament/Src/Launch.cpp` | 938 | the SDL/web launcher (`main`), derived from Epic's **Linux** launcher `ut99-v400/XLaunch/Src/XLaunch.cpp` (Brandon Reinhart), *not* from the Win32 `Launch/Src/Launch.cpp` |
| `.../Src/LaunchPrivate.h` | 14 | `#include "Engine.h"` + `"UnRender.h"` only |
| `.../Src/Res/` | 3 files | `LaunchRes.rc`, `LaunchRes.h`, `Unreal.ico` — **not referenced by CMake** (dead; HP2's `Source/Game/Src/Res` is the same copy). *(Corrected in the 2026-09-26 re-run: the first run said "empty".)* |
| `.../CMakeLists.txt` | 303 | executable `UnrealTournament`; web output renamed `SDLLaunch.{js,wasm,data}` |
| `.../cmake/emit-artifact-meta.mjs` | — | writes `meta.json` for the monorepo's `packages/build` |
| `.../offscreen-depth24.sh` | — | post-link patch of the emitted loader: 24-bit depth for `-sOFFSCREEN_FRAMEBUFFER` |

**HP2 state today:** `Source/Game/Src/Launch.cpp` is a **byte-identical copy** of the template
(`diff` → identical), `Source/Game/cmake/emit-artifact-meta.mjs` is identical too, and
`Source/Game/CMakeLists.txt` differs only by: `project(Game CXX)`, Dreamcast branches removed,
`UT_HAS_NOPENALDRV→HP2_HAS_NOPENALDRV`, `UT99_ASSETS_DIR→HP2_ASSETS_DIR (${CMAKE_SOURCE_DIR}/../Assets)`,
`UT_WEB_PERF→HP2_WEB_PERF`, `--target=hp2`, and comment text (`Game.ini` instead of `UnrealTournament.ini`).
**ACTION:** everything in §2.6 below is still to be done in `Source/Game/Src/Launch.cpp`.

Several features the task names do **not** live in `Launch.cpp` in the template — they live in Core/Engine
and are only *consumed* by the launcher. Where each one lives (so HP2 ports them to the right file):

| Feature | Where it really lives in the template | HP2 equivalent file |
|---------|----------------------------------------|---------------------|
| Renderer switch table + persistence | `Launch.cpp:272-468` | `Source/Game/Src/Launch.cpp` (copied) |
| Subsystem normalisation (native) / forced web set | `Launch.cpp:324-362, 483-549` | same |
| FirstRun stamp | `Launch.cpp:557-561` | same (+ HP2 semantics, §2.6) |
| FrameRateCap + Emscripten main-loop inversion + pacing ring | `Launch.cpp:15-115, 627-737` | same |
| `ue_exec_command` (ADR-10) | `Launch.cpp:117-145` | same |
| `-ini:<File>:[<Section>]:<Key>=<Value>` | **Core** `Core/Src/UnMisc.cpp:1097-1151` (`appApplyIniOverrides`), called from `appInit` `UnMisc.cpp:1831`; decl `Core/Inc/UnFile.h:362-369` | HP2 `Core/Src/UnMisc.cpp` `appInit` (line 1845ff) — **absent today** |
| Global-option-is-not-a-map (`CPUSPEED=3000` bug) | **Core** `UnMisc.cpp:1078-1094` (`appIsGlobalOptionToken`) + **Engine** `UnGame.cpp:184-191` | HP2 `Core/Src/UnMisc.cpp` + `Engine/Src/UnGame.cpp:181-185` — **absent today** |
| `INI=` / `USERINI=` | stock `appInit` `UnMisc.cpp:1791-1826` | HP2 `appInit` `UnMisc.cpp:1912-1948` (present, but different semantics — §2.2.4) |
| AutoFOV | **Engine** `Engine/Inc/UnCamera.h:401-470`, `Engine/Src/UnCamera.cpp:247-390`, `Engine/Src/UnGame.cpp:1255-1300` (scoped override in `UGameEngine::Draw`) + `:1379-1400` (HUD), `Engine/Src/UnCanvas.cpp:570-645` (viewmodel), `Render/Src/UnRender.cpp:3224-3233` | HP2 `Engine/Src/UnCamera.cpp`, `UnGame.cpp:1479` (Draw) — **absent today** |
| `appRequestExit` (web: return to launcher) | **Core** `Core/Src/UnUnix.cpp:201-241` | HP2 `Core/Src/UnUnix.cpp:167` (stock Loki version — no web branch) |
| GLog → console (stdout echo) | **Core** `Core/Inc/FOutputDeviceFile.h:85-101` | HP2 `Core/Inc/FOutputDeviceFile.h` — **absent**, and it logs to `appUserDir()` (line 42) |
| `Received '<pkg>'` log line | **Engine** `Engine/Src/UnChan.cpp:1321-1327` (`UFileChannel::Destroy`) | HP2 `Engine/Src/UnChan.cpp` — check (§1.9) |
| Layout guard `[layout]` | **Core** `Core/Src/UnClass.cpp:295-302` (record `sizeof` in `UStruct` ctor) + `:364-386` (compare in `UStruct::Link`) | HP2 `Core/Src/UnClass.cpp` — **absent today** |
| `appBaseDir` = getcwd on web | **Core** `Core/Src/UnUnix.cpp:666-709` | HP2 `Core/Src/UnUnix.cpp:589-601` (getcwd always) |
| exceptions-off | top-level `Source/CMakeLists.txt:150-156` + launcher `GIsGuarded=0` + per-site NULL returns in `UnObj.cpp`/`UnGame.cpp` | §1.10 |

### 1.2 Feature / switch inventory — stock vs ufront additions

Diff basis: `diff --strip-trailing-cr -w /home/ion/epic-sources/ut99-v400/XLaunch/Src/XLaunch.cpp
engine-ut99/Source/UnrealTournament/Src/Launch.cpp` (the Win32 `Launch/Src/Launch.cpp` is the ancestor of
HP2's launcher, not of this one).

| # | Feature / switch | Template location | Origin | What it does | HP2 action |
|---|------------------|-------------------|--------|--------------|-----------|
| 1 | `main(argc,argv)`, argv joined with spaces into `CmdLine[1024]` | `:805-851` | stock XLaunch `:280-305` | no quoting preserved; >1023 chars overflows (`appStrcat` unchecked) | KEEP; **bound the join** (`appStrncat`, 1024) — the web host appends `-ini:` tokens and URLs |
| 2 | `appStrcpy( GModule, argv[0] )` | `:838` | stock | `GModule` is `ANSICHAR[32]` (`Core/Src/UnUnix.cpp:63`; HP2 `UnUnix.cpp:39` same) — **overflows for any argv[0] ≥ 32 chars** (e.g. `/home/ion/hp2-ufrontend/run/System/Game`) | FIX in HP2: copy the **basename** with `appStrncpy(GModule, basename, 32)` |
| 3 | `THIS_PACKAGE` = `appPackage()` | `:151, :841` | stock (`GPackage` renamed by ut99dc's static-link macro `THIS_PACKAGE` → `GPackageUnrealTournament[64]` under `UNREAL_STATIC`, `Core/Inc/Core.h:348-355`) | On Unix `appPackage()` **returns `GModule`, i.e. argv[0] verbatim** (`UnUnix.cpp:760-765`; HP2 `UnUnix.cpp:627-632` identical). The log file is `appBaseDir()+appPackage()+".log"` (`FOutputDeviceFile.h:45-51`), so the log name is derived from argv[0]: `./UnrealTournament` → `System/./UnrealTournament.log`; web argv[0] `./this.program` → `/System/./this.program.log`; an **absolute** argv[0] yields `<base>/<abs path>.log` = an unopenable path → `Dead=1`, **no log file at all** (stdout echo still works). | HP2 `Core.h:473` has plain `GPackage[]`; follow whatever the Core port adopts for the static single binary (template macro). **Launcher fix (with #2):** set `GModule` to the argv[0] **basename** so the log is always `Game.log` next to the ini |
| 4 | `~/.utconf` ini swap | `:853-870`, `:781-797` | stock (commented `system()`) | dead code | DROP |
| 5 | `GIsClient=1; GIsGuarded=0` before `appInit` | `:874-875` | **changed**: stock XLaunch sets `GIsGuarded = 1` (`XLaunch.cpp:323`); Win32 launchers set `GIsClient = GIsGuarded = 1` | with `GIsGuarded==0`, `appErrorf` → `FOutputDeviceAnsiError::Serialize` → `HandleError()` + `appRequestExit(1)` (= `exit(1)`) instead of `throw(1)` (`FOutputDeviceAnsiError.h:65-81`) — **part of the exceptions-off contract** | KEEP `GIsGuarded=0` |
| 6 | web `chdir("/System")` before `appInit` | `:876-882` | ufront 2.7b | makes `appBaseDir()` (getcwd on web) = `/System/` | KEEP (web) + ADD native chdir, §1.11 |
| 7 | `appInit( TEXT("UnrealTournament"), … , 1 )` | `:883` | stock (hard-coded name) | ini = `UnrealTournament.ini`, `User.ini` | HP2: `appInit( TEXT("Game"), … )` → `Game.ini` (retail `Game.exe` used `GPackage=appPackage()`="Game") |
| 8 | `-SERVER` → `GIsClient=0`; `-LAZY`/server → `GLazyLoad` | `:886-894` | stock | dedicated server mode | KEEP (harmless; HP2 has no server game) |
| 9 | `-LOG` → `Warn.AuxOut=GLog; GLog=&Warn` | `:899-904` | stock | echo log through `FFeedbackContextAnsi` (printf) | KEEP; note the stdout echo in `FOutputDeviceFile` already mirrors every line, so `-LOG` **doubles** console output in the template |
| 10 | `SelectRenderDevice()` + switch table | `:258-468` | ufront 2.9/2.12 | §1.3 | KEEP |
| 11 | `NormalizeSubsystem` (native only) | `:324-362, :528-549` | ufront 2.33/2.34 | §1.5 | KEEP; HP2 lists differ (§2.6) |
| 12 | web forced driver set | `:483-526` | ufront 2.7c/2.7d/2.12/2.38 | §1.5 | KEEP + HP2 deltas |
| 13 | FirstRun stamp | `:557-561` | stock XLaunch `:146-149` | §1.6 | ADAPT (§2.6) |
| 14 | `ServerActors.Empty()` (web) | `:571-585` | ufront 2.7d | §1.5 | KEEP and make it **all-platform** (HP2 `UGameEngine::ServerActors` exists, `Engine/Inc/UnGame.h:27`; HP2's Default.ini lists six IpDrv/IpServer/UWeb actors, none of which HP2 ships except IpDrv) |
| 15 | `PersistRenderChoiceIfSwitched()` | `:446-468, :589` | ufront 2.12 | writes `ufrontUserRenderDevice` marker + `Flush(0)` | KEEP |
| 16 | `GWebRTCEngine = Engine` | `:52-54, :593-596` | ufront 2.7d | used by `WebRTCDrv.cpp` and `ue_exec_command` | KEEP |
| 17 | `HandleError()` with `SDL_ShowSimpleMessageBox` | `:606-621` | ufront/ut99dc | stock XLaunch just `printf(GErrorHist)` | KEEP (native); on web the `catch(...)` is dead (§1.10) |
| 18 | `-EXEC=<file>` → `Viewports(0)->Exec("exec <file>")` | `:913-920` | stock | runs a console script after init | KEEP (HP2 Win32 launcher has it too) |
| 19 | `MainLoop` + `CurrentTickRate` per second | `:630-737` | stock + ufront 2.12/2.19/2.43 | §1.8 | KEEP; FTime adaptation (§2.6) |
| 20 | `CleanUpOnExit` (delete `Running.ini`, `appPreExit`, `appExit`) | `:770-800` | stock | | KEEP; add HP2's `Audio->SetViewport(NULL)` (§2.1) |
| 21 | `SimpleCopy` | `:747-765` | stock, unused | | DROP |
| 22 | Dreamcast (`PLATFORM_DREAMCAST`, KOS, FATFS, `FatalError`, `init_thread_stack`) | `:176-252, :807-829, :890-897` | ut99dc | | DROP entirely |
| 23 | X11 splash (`InitWindowing/InitSplash/ExitSplash`) | — | stock XLaunch `:40-132` | removed by ut99dc | stays dropped |
| 24 | `__Context::StaticInit()` | — | stock XLaunch `:282` (signal→exception context) | removed | stays dropped |
| 25 | `ufront_yield_frame` (`EM_ASYNC_JS` setTimeout(0)) | `:18-49` | ufront 2.13.7→2.43 | §1.8 | KEEP |
| 26 | frame-pacing ring + `ufront_pacing [reset]` | `:56-115, :128-137` | ufront 2.43/2.43a | §1.9 | KEEP |
| 27 | `ue_exec_command` | `:117-145` | ufront ADR-10 | §1.9 | KEEP |
| 28 | `-CPUSPEED=`/`CPUSPEED=` | not in launcher; Win32 `UnVcWin32.cpp:904` only | stock | the web host always passes `CPUSPEED=3000` (`ufrontend/packages/frontend/src/shell/boot.ts:31`); the Unix core ignores it — its only effect is being mis-read as the startup map (§1.11) | ADD the `appIsGlobalOptionToken` guard to HP2 |

Switches honoured **outside** the launcher that a HP2 run will meet: `INI=`, `USERINI=`, `CDPATH=`, `-STRICT`
(Core `appInit`); `LOG=`, `ABSLOG=` (log path, `FOutputDeviceFile.h:47-48`); `-ini:…` (Core); `-benchmark
SECONDS=N` (the host appends it for `?benchmark=`, `boot.ts:365-367` — check HP2's UnGame/UnLevTic has no
`-benchmark` handling, so it is a harmless no-op there).

### 1.3 Renderer switch table (`Launch.cpp:258-468`)

```cpp
static const FRenderSwitch GRenderSwitches[] =
{
	{ TEXT("XOPENGLRENDERER"),   TEXT("XOpenGLDrv.XOpenGLRenderDevice") },
	{ TEXT("UTGLRRENDERER"),     TEXT("OpenGLDrv.OpenGLRenderDevice") },
	{ TEXT("NOPENGLESRENDERER"), TEXT("NOpenGLESDrv.NOpenGLESRenderDevice") },
	{ TEXT("SOFTWARERENDERER"),  TEXT("SoftDrv.SoftwareRenderDevice") },
	{ TEXT("NULLRENDERER"),      TEXT("NullDrv.NullRenderDevice") },
};
static const TCHAR* GRenderPreference[] =
{ TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("OpenGLDrv.OpenGLRenderDevice"), TEXT("NOpenGLESDrv.NOpenGLESRenderDevice") };
```

`SelectRenderDevice()` (`:388-444`), run **before** `Engine->Init()`:
1. first `-<SWITCH>` present (via `ParseParam`) wins **if** `IsPackageClassAvailable(Device)`; sets
   `[Engine.Engine] GameRenderDevice`, `WindowedRenderDevice`, `RenderDevice` (`SetRenderDeviceConfig`,
   `:364-369`) and remembers `GRequestedRenderDevice`. An unavailable switch logs a warning and falls through.
2. else the persisted `GameRenderDevice` if available **and** (in `GRenderPreference` **or** equal to the
   `[Engine.Engine] ufrontUserRenderDevice` marker). This is what refuses the retail ini's
   `GameRenderDevice=SoftDrv…` as an implicit default.
3. else the first available device of `GRenderPreference`.

`IsPackageClassAvailable` (`:304-322`): web → `StaticFindObject(UClass, ANY_PACKAGE, <short class name>)`
(no file I/O, no throw — the static binary registered every native class at startup); native →
`StaticLoadClass(UObject::StaticClass(), NULL, Device, NULL, LOAD_NoWarn, NULL)`.

`PersistRenderChoiceIfSwitched()` (`:451-468`) after `Engine->Init()`: if `GameRenderDevice` still equals the
request, write `ufrontUserRenderDevice=<device>` and `GConfig->Flush(0)` — the pick sticks next boot.

**HP2 specifics / ACTIONS**
- `NOpenGLESDrv` is **not** part of the HP2 driver set (CLAUDE.md lists XOpenGLDrv, OpenGLDrv/UTGLR, SoftDrv,
  NullDrv). Remove `NOPENGLESRENDERER` from the table and from `GRenderPreference` (or leave it: an
  unavailable switch is a logged no-op — but a dead row is exactly the "dangling scaffolding" the template's
  own comments warn against). **Decision for the plan: remove.**
- HP2's retail `Default.ini` says `GameRenderDevice=D3DDrv.D3DRenderDevice`,
  `WindowedRenderDevice=SoftDrv.SoftwareRenderDevice`, `RenderDevice=GlideDrv.GlideRenderDevice`
  (`Config/System/Default.ini:22-31`). D3DDrv/GlideDrv are never built → step 2 rejects them → preference
  order picks XOpenGL. Good; no change needed.
- **Phase B boots before the hardware renderers exist.** With only SoftDrv + NullDrv compiled,
  `GRenderPreference` has no available entry and step 3 logs "no known render device is compiled into this
  build!" and leaves the ini's `D3DDrv…` in place → `LOAD_NoFail` of `D3DDrv` → fatal. So Phase B **must**
  launch with `-SOFTWARERENDERER` or `-NULLRENDERER` (the plan already says so). Optional hardening: when
  nothing in `GRenderPreference` is available, fall back to `SoftDrv.SoftwareRenderDevice` rather than
  leaving an unloadable device in the ini.
- The `ParseParam` used is the lax Epic one (`HP2 Core/Src/UnMisc.cpp:734-744`, identical in the template):
  it matches `-X`/`/X` **anywhere** as a substring (case-insensitive). None of the five switch names is a
  substring of another, so the table is safe; but `ParseParam(CmdLine,"LOG")` is also true for any token
  containing `/log…` (e.g. a path in a `-ini:` value). Low risk; note only.

### 1.4 `-ini` / `-userini` / `-ini:<File>:[<Section>]:<Key>=` seeding

**Stock** (`appInit`, template `UnMisc.cpp:1790-1831`; HP2 `UnMisc.cpp:1912-1952`):
- `INI=<file>` replaces `<Package>.ini`; `USERINI=<file>` replaces `User.ini`. Missing file + `RequireConfig`
  → created by copying `Default.ini` / `DefUser.ini`. (These are `Key=Value` tokens, **not** `-ini:`.)

**ufront 2.38 — `-ini:<File>:[<Section>]:<Key>=<Value>`** (`appApplyIniOverrides`, `UnMisc.cpp:1119-1151`):
tokenises `appCmdLine()` with `ParseToken` (quotes honoured), for each token starting `-ini:` splits at
`:[`, `]:`, `=` and calls `GConfig->SetString( Section, Key, Value, File )`; `File` is a generic name
(`User`, `System`) translated by `FConfigCacheIni::Find` (HP2 `FConfigCacheIni.h:119-122` has the same
`User.ini`/`System.ini` translation). It **writes** into the ini (persisted on the next Flush), it does not
override for one run. Called once, immediately after `GConfig->Init` — before any class loads config.

Who emits it: the frontend (`-ini:User:[DefaultPlayer]:Name=<player>`, the frame-rate cap
`-ini:System:[Engine.Engine]:FrameRateCap=<n>`, AutoFOV E2E `-ini:System:[ufront.AutoFOV]:AutoFOV=0`).

**HP2 ACTIONS**
1. Port `appApplyIniOverrides` + `appIsGlobalOptionToken` + their decls (`UnFile.h`) into HP2 Core.
2. **Placement is different in HP2:** HP2's `appInit` has an extra block right after `GConfig->Init`
   (`UnMisc.cpp:1955-1964`, "gk 10/15/01") that **overwrites `[Engine.Engine] Language` from
   `default.ini`**. If `appApplyIniOverrides()` is called at the template's spot, a
   `-ini:System:[Engine.Engine]:Language=det` is clobbered one statement later. Call it **after** that block
   (before `// Language.` at line 1968).
3. HP2's `appInit` also writes `[Core.System] SavePath` and `paths` and calls `GConfig->Flush(0)` at the very
   end (`UnMisc.cpp:2062-2065`) — an `-ini:` override of `[Core.System]` would be overwritten there; document
   it (nobody needs it today).

### 1.5 Ini normalisation (Windows/native vs web)

**Native / Windows / MinGW (`#ifndef __EMSCRIPTEN__`, `Launch.cpp:528-549`)** — `NormalizeSubsystem(Section,
Key, Preference[], N)`: if the configured class is available, leave it; else set the first available
preference; never a null driver.

```cpp
static const TCHAR* const ViewportPreference[] = { TEXT("NSDLDrv.NSDLClient") };
static const TCHAR* const AudioPreference[]    = { TEXT("NOpenALDrv.NOpenALAudioSubsystem"),
                                                   TEXT("SoundDrv.NullAudioSubsystem") };
static const TCHAR* const NetPreference[]      = { TEXT("IpDrv.TcpNetDriver") };
```
(The comment says "no null driver", yet `SoundDrv.NullAudioSubsystem` **is** the second audio entry — it is
the fallback when `BUILD_NOPENALDRV=OFF`. `UT_HAS_NOPENALDRV` / `HP2_HAS_NOPENALDRV` is defined by CMake but
**never read** by `Launch.cpp` — availability is probed at runtime instead. Harmless; either use the define
or delete it from CMake.)

**Web (`#ifdef __EMSCRIPTEN__`, `Launch.cpp:483-526`)** — unconditional `GConfig->SetString`:

| Section | Key | Value | Why |
|---------|-----|-------|-----|
| `Engine.Engine` | `ViewportManager` | `NSDLDrv.NSDLClient` | SDL2 window/input |
| `Engine.Engine` | `AudioDevice` | `NOpenALDrv.NOpenALAudioSubsystem` | Emscripten OpenAL + libxmp `.umx` |
| `Engine.Engine` | `NetworkDevice` | `WebRTCDrv.WebRTCNetDriver` | no BSD sockets in a browser |
| `NSDLDrv.NSDLClient` | `CaptureMouse` | `True` | pointer lock while playing |
| `Engine.GameInfo` | `bWorldLog`, `bLocalLog` | `False` | ngWorldStats / `appCreateProc` |
| `IpServer.UdpServerUplink` | `DoUplink` | `False` | GameSpy master server |

plus `((UGameEngine*)Engine)->ServerActors.Empty()` after `ConstructObject`, before `Init()` (`:583-584`).

**HP2 ACTIONS (web block)**
- `ViewportManager` → `NSDLDrv.NSDLClient`: KEEP (retail `WinDrv.WindowsClient`).
- `AudioDevice`: **not** NOpenALDrv until Phase E; HP2's audio subsystem interface differs (`UAudioSubsystem`
  433: `PlaySound(…Flags,FadeDuration,Priority)`, Ogg streaming). Until NOpenALDrv is 433-ported, set
  `SoundDrv.NullAudioSubsystem` — **better: make the web block use the same runtime probe as native**
  (`NOpenALDrv` if `IsPackageClassAvailable`, else `SoundDrv`), so no code edit is needed when Phase E lands.
  Retail value is `ALAudio.ALAudioSubsystem` — never built.
- `NetworkDevice` → `WebRTCDrv.WebRTCNetDriver`: KEEP only if WebRTCDrv is linked; same probe.
- `CaptureMouse=True`: KEEP — HP2's spell gestures need relative mouse, and the menus are UWindow (the
  `bShowMenu`-driven capture release — verify HP2's `UnGame`/`UWindow` path calls `SetMouseCapture(0)` when
  the FEBook is open; else the cursor is locked inside the menu).
- `bWorldLog/bLocalLog`: HP2's `Engine.GameInfo` — check the properties exist (`grep bWorldLog
  Source/Engine/Classes/GameInfo.uc`); `SetString` on an unknown key is harmless. `IpServer.UdpServerUplink`:
  DROP (HP2 has no IpServer).
- **`ServerActors.Empty()` is REQUIRED for HP2, and on every platform, not just web:** HP2's retail
  `Default.ini:66-74` `[Engine.GameEngine]` still lists `ServerActors=IpDrv.UdpBeacon`,
  `IpServer.UdpServerQuery`, three `IpServer.UdpServerUplink` (unreal.epicgames.com, master0.gamespy.com,
  master.mplayer.com) and `UWeb.WebServer`. HP2 ships **no** IpServer/UWeb packages at all, so any listen
  server (P2P host) would `Can't find file for package 'IpServer'`. Move the `Empty()` out of the
  `#ifdef __EMSCRIPTEN__` in the HP2 launcher (or filter to classes `IsPackageClassAvailable` accepts), and
  also drop those lines from `Config/System/web/Default.ini`.

**HP2 ACTIONS (native block)**: same three `NormalizeSubsystem` calls. Audio preference for HP2:
`{ "NOpenALDrv.NOpenALAudioSubsystem", "SoundDrv.NullAudioSubsystem" }` (ALAudio is the retail driver and is
never built). Net: `{ "IpDrv.TcpNetDriver" }` — with the note that until IpDrv links (Phase B step 6) this
list has no available entry → warning, value left → `LOAD_NoFail` would only fire if something *uses* the
net driver (standalone play does not instantiate it; `UGameEngine::Browse` with a remote URL does). Add
`WebRTCDrv.WebRTCNetDriver` as a second native preference only if WebRTCDrv is built natively.

### 1.6 FirstRun handling

Template `Launch.cpp:557-561` (identical to stock XLaunch `:146-149`):
```cpp
INT FirstRun=0;
if (FirstRun<ENGINE_VERSION)
	FirstRun = ENGINE_VERSION;
GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
```
It never *reads* the ini: it unconditionally stamps `[FirstRun] FirstRun=<ENGINE_VERSION>` (400) into the
game ini. Its only function is to keep code that checks `FirstRun<N` (the Win32 config wizard) quiet.

HP2 (`UnEngineWin.h:1675-1820`) **reads** `FirstRun`, `Reconfig`, `ForceSoftware` from `[FirstRun]` and, in
the retail flow, gates nothing on them any more (the `FirstRun<220` save migration is `#if 0`, the
first-time wizard is commented out). It then stamps `FirstRun=ENGINE_VERSION` (433). HP2's Default.ini ships
`[FirstRun] FirstRun=0 Reconfig=1 ForceSoftware=0`. **ACTION:** read-then-stamp like HP2 (so a log line can
say "first run"), write 433; ignore `Reconfig`/`ForceSoftware` (D3D-detection knobs). No dialogs.

### 1.7 AutoFOV (`[ufront.AutoFOV]`)

Lives in **Engine**, not the launcher (grep: `Engine/Inc/UnCamera.h:401-470`, `Engine/Src/UnCamera.cpp:247-390`,
`Engine/Src/UnGame.cpp:1255-1300` and `:1379-1400`, `Engine/Src/UnCanvas.cpp:570-645`,
`Render/Src/UnRender.cpp:3224-3233`).

Mechanism:
- `ufrontEffectiveFov(BaseFov, SizeX, SizeY, bLog)`: mode 0 Off (stock), 1 Hor+
  `2*atan(tan(Fov/2)*Aspect/Ref)`, 2 Adaptive `max(Fov, Hor+)` (default). Clamped 1..170.
- Applied as a **scoped override** in `UGameEngine::Draw` (`struct FScopedAutoFov`, `UnGame.cpp:1263-1278`):
  after `eventPlayerCalcView`, before `Viewport->Lock`, sets `Viewport->Actor->FovAngle` to the effective FOV
  and restores it in the destructor — so every render device and the engine's clip planes agree, while script
  (and `MouseScale = MouseSensitivity*DesiredFOV*0.0111`) sees the real FOV.
- `ufrontWeaponFov` (tan-space blend for the first-person viewmodel overlay, `UnCanvas.cpp`), and
  `ufrontHudScaleFactor` (AutoFOVUI: multiplies the HUD class's `HudScale` property, found by reflection,
  around `eventPostRender`).
- Config is parsed once from the free section `[ufront.AutoFOV]` of the **game ini**:
  `AutoFOV` (0/1/2, default 2), `ReferenceAspect` (default 4:3 = 1.333), `WeaponScale` (default 1.0),
  `AutoFOVUI` (default 1), `UIScale` (default 1.0). No command-line switches since 2026-09-10; per-launch
  changes use `-ini:System:[ufront.AutoFOV]:AutoFOV=0`. Log lines `[AutoFOV] mode=…`, `[AutoFOV] viewport=…`,
  `[AutoFOVUI] …` (the E2E asserts on them).
- Template ini block: `engine-ut99/Config/System/Default.ini:547-572` (same in `UnrealTournament.ini:445`).

**HP2 ACTIONS**: port to HP2 `Engine/Src/UnCamera.cpp` + `Engine/Inc/UnCamera.h` + `UnGame.cpp` Draw (HP2's
`UGameEngine::Draw` is at `UnGame.cpp:1479`, adds `GIsDrawing` and a Precache block — insert the scope after
`eventPlayerCalcView` at `:1494`). HP2 is **third-person** with a script camera (`HGame/Classes/BaseCam.uc`,
`FOVController.uc`, console `Cam_FOV`), so Hor+ is the relevant part; the viewmodel blend is inert. HP2's HUD
classes (`HGame/Classes/baseHud.uc`, `Menu/HPHud.uc`) have **no `HudScale`** property (grep finds none), so
AutoFOVUI is a no-op (reflection returns NULL) — correct and harmless. The launcher's only duty: the web
`Default.ini` carries `[ufront.AutoFOV]`. Engine analyst owns the code port.

### 1.8 FrameRateCap, main loop, Emscripten main-loop inversion

`MainLoop` (`Launch.cpp:630-737`):
- per frame: `DeltaTime = appSeconds()-OldTime`; `Engine->Tick(DeltaTime)`; `GWindowManager->Tick`;
  `CurrentTickRate` recomputed once per second (stock XLaunch).
- **FrameRateCap (ufront 2.12/2.19/2.61):** `Cap = Engine->GetMaxTickRate()`; if ≤0 (standalone — always
  for HP2 single-player) use `[Engine.Engine] FrameRateCap` (read once; default **120**; ≤0 = uncapped).
  Reason: `ULevel::Tick` floors `DeltaSeconds`, so an uncapped loop runs the game faster than real time.
  **HP2 has the same floor, tighter:** `Engine/Src/UnLevTic.cpp:960`
  `DeltaSeconds = Clamp(DeltaSeconds,0.005f,0.10f);` (template v400: `Clamp(…,0.001f,0.40f)`, line 849). So
  HP2 runs **too fast above 200 fps** and in **slow motion below 10 fps** (each frame advances at most
  100 ms). The 120 fps default cap is inside the honest range; keep it. The <10 fps side matters for
  SoftDrv on web — a slow software frame makes the game run slow rather than jumpy; that is stock HP2
  behaviour, not a port bug.
- **Native:** `appSleep( Max(0, 1/Cap - elapsed) )`.
- **Web (ufront 2.7b → 2.13.7 → 2.43):** Asyncify main-loop inversion; the loop never returns to the browser
  except by suspending:
  ```cpp
  DOUBLE Budget = Cap>0.0 ? (1.0/Cap) : 0.0;
  INT SleepMs = (INT)((Budget - (appSeconds()-OldTime)) * 1000.0);
  if( SleepMs > 1 ) emscripten_sleep( SleepMs );   // spare budget -> sleep = the cap
  else              ufront_yield_frame();          // EM_ASYNC_JS: await setTimeout(0) = present point
  ```
  Not `emscripten_set_main_loop` (its unwind collides with `main()`'s try/catch). The yield is the frame's
  **present point** for the worker-owned OffscreenCanvas; a `MessagePort` resume was measured to drop 44% of
  frames and was replaced by `setTimeout` (2.43). Link flags: `-sASYNCIFY -sASYNCIFY_STACK_SIZE=262144
  "-sASYNCIFY_IMPORTS=['emscripten_sleep','emscripten_set_main_loop','__syscall_openat']"`, `-pthread
  -sPROXY_TO_PTHREAD -sOFFSCREENCANVAS_SUPPORT -sOFFSCREENCANVASES_TO_PTHREAD=#canvas -sOFFSCREEN_FRAMEBUFFER`.
- **Pacing ring (2.43/2.43a):** 4096 samples of `{Start, TickMs, YieldMs, SleepReqMs, HeapMB}`, dumped as CSV
  by `ue_exec_command("ufront_pacing")`, reset by `ufront_pacing reset`.

**HP2 ACTIONS — FTime:** HP2 Core's `appSeconds()` returns **`FTime`** (`Core/Inc/Core.h:24-51`,
`UnFile.h:257`), a 32.32 fixed-point class with **no conversion to `double`**. Every
`DOUBLE x = appSeconds()` in the template fails to compile. Rewrite with `FTime` and float differences:
`FTime OldTime = appSeconds(); … FLOAT DeltaTime = NewTime - OldTime;` (`FTime::operator-(FTime)` returns
float), `appSeconds().GetFloat()` only for logging. Affected template lines: `:74, :88, :107, :478, :597,
:637-665, :714-723, :728`. (HP2's own `MainLoop`, `UnEngineWin.h:1900-1911`, is the reference — it also sets
`CurrentTickRate = 1.f/DeltaTime` every frame instead of per second; keep the template's per-second average.)

### 1.9 `ue_exec_command`, `appRequestExit`, GLog→console, `Received` persistence

- **`ue_exec_command(const char*)`** (`Launch.cpp:121-145`, `EMSCRIPTEN_KEEPALIVE`, exported as
  `_ue_exec_command` in `-sEXPORTED_FUNCTIONS`): returns captured output. Special-cases `ufront_pacing`; else
  `GWebRTCEngine->Exec(Cmd, FStringOutputDevice)`. HP2 has `FStringOutputDevice`
  (`Core/Inc/UnTemplate.h:1343`) and `UEngine::Exec(const TCHAR*, FOutputDevice&)` — ports as is. **Note:**
  it is called from JS via `ccall`, i.e. on the **browser main thread**, while the engine runs on the pthread
  (`-sPROXY_TO_PTHREAD`). Verify how the template guards that (ccall of a non-proxied export runs on the
  calling thread → a data race with the engine loop). Out of scope to change; flag it (open question Q5).
- **`appRequestExit(Force)`** (Core `UnUnix.cpp:201-241`): Force → `exit(1)`; else `GIsRequestingExit=1` and
  on web `MAIN_THREAD_EM_ASM` (threaded) navigates to `location.pathname` via `setTimeout(0)` — the page
  returns to the launcher instead of a dead canvas. HP2's `Core/Src/UnUnix.cpp:167` is the stock Loki body
  → **port the web branch** (Core analyst) — the launcher relies on it for HP2's "Quit" (`FEMainPage`
  `ExitButton` → `Root.DoQuitGame()` → `exit` console command → `USystem::Exec EXIT` → `appRequestExit(0)`).
- **GLog → console** (Core `FOutputDeviceFile.h:85-101`): every non-suppressed, non-`NAME_Title` line is also
  `fputs` to stdout (native terminal / web `Module.print` → `console.log`). HP2's `FOutputDeviceFile.h` lacks
  it and builds the log path from **`appUserDir()`** (line 42) instead of `appBaseDir()`. **ACTION:** port the
  echo; for the path see §2.2.4 (appUserDir on Unix).
- **`Received '<pkg>'`** (Engine `UnChan.cpp:1321-1327`): ufront added a `debugf` of the stock
  `NotifyProgress` message so the log names each downloaded package. The frontend **no longer** keys its
  download-cache flush on it — it flushes once per level on `/Bringing .* up for play/`
  (`ufrontend/packages/frontend/src/shell/boot.ts:390-404`). **HP2 ACTIONS:** (a) HP2 is on the 432+
  download architecture — `UFileChannel::Destroy` (`Engine/Src/UnChan.cpp:1297-1319`) just calls
  `Download->DownloadDone()`; the stock `Received '%s'` message is built in **`UDownload::DownloadDone`,
  `Engine/Src/UnDownload.cpp:176`** — add the one-line `debugf( TEXT("%s"), Msg )` there (only matters for P2P); (b) **make sure HP2 still logs
  `Bringing %s up for play`** in `UGameEngine::LoadMap` (`grep -n "up for play" Source/Engine/Src/UnGame.cpp`) —
  it is also a Phase B milestone.

### 1.10 Exceptions-off boot contract, headless/xvfb, layout guard

- **Contract:** engine objects are compiled with Emscripten's default `-fignore-exceptions`
  (`Source/CMakeLists.txt:150-156`): a `throw` aborts, the `try/catch` in `main` (`:830-938`) and in `guard`
  macros never catches. `-sNO_DISABLE_EXCEPTION_CATCHING` is linked (2.61) **only** so openal-soft's own
  `-fexceptions` objects can catch. Therefore:
  1. the launcher sets `GIsGuarded=0` (§1.2 #5) → `appErrorf` prints `GErrorHist` and `exit(1)`s instead
     of throwing;
  2. recoverable load failures are patched **per site** to return NULL on `__EMSCRIPTEN__`
     (`grep -rn __EMSCRIPTEN__ engine-ut99/Source/Core engine-ut99/Source/Engine` — UnObj.cpp,
     UnLinker, UnGame.cpp `LoadMap` + the startup-map fallback `UnGame.cpp:201-215`);
  3. `IsPackageClassAvailable` uses `StaticFindObject` on web so probing a missing driver never touches the
     file system.
  **HP2 ACTION:** keep 1 and 3 in the launcher; 2 is the Core/Engine analysts' list. HP2's
  `UGameEngine::Init` (`UnGame.cpp:212-214`) still `appErrorf`s when the startup map fails — port the
  template's web fallback to the Entry menu.
- **Headless / Xvfb:** `offscreen-depth24.sh` is web-only. Native headless: the template's CLAUDE.md notes
  the client stops at the GL-context step without a display; `-NULLRENDERER` needs no GL but NSDLDrv still
  opens an SDL window → run under `Xvfb :99 -screen 0 1280x720x24` (or `SDL_VIDEODRIVER=dummy` if NSDLDrv
  tolerates it — verify; `offscreen`/`dummy` drivers fail GL context creation, fine for NullDrv/SoftDrv
  only if NSDLViewport's software path does not need a GL context). `SDL_ShowSimpleMessageBox` in
  `HandleError` blocks forever under Xvfb with no one to click — **ACTION:** skip the message box when
  `ParseParam(appCmdLine(),"NOMSGBOX")`/`-unattended` or when `getenv("SDL_VIDEODRIVER")` is `dummy`, so a
  crash in CI exits.
- **Layout guard (2.32):** not in the launcher — Core `UnClass.cpp:295-302` records each native `UStruct`'s
  C++ `sizeof` in `GLayoutStructs/GLayoutSizes[4096]` at static-init; `UStruct::Link` (`:364-386`) compares it
  with the size derived from the **loaded .u** and logs `[layout] native X: C++ sizeof=%i but script
  layout=%i`. It fires during boot package loads, so the launcher's log is where it shows. Phase B acceptance
  requires zero `[layout]` lines. **ACTION (Core analyst):** port both hunks into HP2 `Core/Src/UnClass.cpp`
  (check HP2's `UStruct(ENativeConstructor…, INT InSize, …)` signature). Note the 2.32 lesson: size equality
  is necessary but not sufficient (ABrush tail-padding, `EngineClasses.h:1865-1890`).

### 1.11 URL / map defaults, chdir to System, `appBaseDir`

- Startup map: **not** in the launcher. `UGameEngine::Init` takes the first command-line token as the URL
  unless it is empty, `SERVER`, starts with `-`, or (ufront 2.38) is a `Key=Value` global option
  (`appIsGlobalOptionToken`, `Core/Src/UnMisc.cpp:1080-1094`: a `Key=Value` token with no `?` before the
  `=`); otherwise `FURL::DefaultLocalMap` (`[URL] LocalMap`). The web host always passes `CPUSPEED=3000` and
  puts an optional `configResult.cmdLine` (a map/URL) **first** (`boot.ts:28-39`).
- On web, a failed startup map falls back to Entry (`UnGame.cpp:201-212`) instead of `appErrorf`.
- **chdir:** web only — `chdir("/System")` before `appInit` (`:876-882`) so `appBaseDir()` (getcwd on web,
  `UnUnix.cpp:673-677`) is `/System/`. Native: `appBaseDir()` = `SDL_GetBasePath()` (exe dir,
  `UnUnix.cpp:678-681`) and `appInit` → `GFileManager->SetDefaultDirectory(appBaseDir())` chdir's there
  (`UnMisc.cpp:1778`). So in the template the native binary **must sit in `System/`** (the run tree does that).
- **HP2 difference:** HP2's Unix `appBaseDir()` is **getcwd()** on every platform (`Core/Src/UnUnix.cpp:589-601`,
  Loki era: "the executable must be launched from its own directory"). **ACTION:** the HP2 launcher
  chdir's on native too — to the directory of the executable (`SDL_GetBasePath()`, or `/proc/self/exe`
  dirname) — before `appInit`, so `./System/Game` launched from the run root works; or the Core port adopts
  the template's `appBaseDir`. Either way `appBaseDir()` must end in `System/`: HP2's `appInit` derives
  `GSys->MapPath` from `appBaseDir().Left(InStr("System"))` (`UnMisc.cpp:2040-2057`).

### 1.12 Windows / MinGW specifics

- The same `Launch.cpp` is the Windows (MinGW) launcher: `main()`, `FFileManagerLinux` (its `mkdir`
  branches on `PLATFORM_WIN32`), `UnUnix.cpp` as the platform layer for every GCC target
  (`UnVcWin32.cpp` is MSVC-only), `UnMinGW.cpp` for calls with no POSIX equivalent, `_spawnl` for RELAUNCH.
- `NormalizeSubsystem` exists because a real retail ini names `WinDrv.WindowsClient`/`Galaxy…` (ufront 2.33
  on Windows, 2.34 for all native).
- CMake: `OPENAL_MINGW_RUNTIME_DLL` copied next to the exe (`CMakeLists.txt:63-72`); Linux copies
  `libopenal.so.1` with `$ORIGIN` rpath (`:74-84`).
- ABrush `bColoredPad[3]` is `#ifndef __MINGW32__` (MinGW lays bitfields out MSVC-style) — relevant to HP2's
  EngineClasses port, not the launcher.
- **HP2 ACTION:** executable name `Game` → `Game.exe` on Windows (retail name). Windows subsystem: the template
  links a console `main`; if a GUI subsystem is wanted later, add `-mwindows` + `WinMain` shim — not needed.
  HP2's retail exe icon `Source/Launch/Src/Res/Game.ico` can be added as a `.rc` for MinGW (cosmetic).

### 1.13 CMakeLists.txt (template) — checklist for `Source/Game/CMakeLists.txt`

| Item | Template | HP2 today | Action |
|------|----------|-----------|--------|
| project / exe | `UnrealTournament` | `Game` | done |
| `UPACKAGE_NAME=${PROJECT_NAME}` | yes (twice, lines 18 and 54) | yes | remove the duplicate (cosmetic) |
| SDL link (non-web) | `${SDL2_LIBRARY} NSDLDrv` | same | done |
| static: `--whole-archive ${INSTALL_TARGETS} minus self` | yes | yes | done |
| `*_HAS_NOPENALDRV` define | defined, unused by code | `HP2_HAS_NOPENALDRV` | use it or drop it |
| OpenAL runtime copy (MinGW/Linux) | yes | yes | keep |
| web link flags (Asyncify, pthread, OffscreenCanvas, WasmFS, exports) | yes | yes | keep verbatim |
| `--preload-file Config/System/web/{Default,DefUser}.ini@/System/…` | yes | yes, **but `Config/System/web/` holds only `README-ut99.md`** | Phase D: write the HP2 web inis; the `foreach` silently skips missing files, then `LINK_DEPENDS` on the two missing files **fails the build** ("No rule to make target") — write them before the first wasm link |
| `OUTPUT_NAME SDLLaunch`, `SUFFIX .js`, dir `SDLLaunch/` | yes | yes | keep (frozen loader name) |
| `offscreen-depth24.sh` POST_BUILD | yes | yes | keep |
| `emit-artifact-meta.mjs --target=hp2` | `--target=ut99` | `--target=hp2`, but the **.mjs itself is the UT99 copy** | Phase D step 6: `Source/Game/cmake/emit-artifact-meta.mjs:60` default `targetId 'ut99'` (overridden by `--target`), `:65` `provenance: { repo: 'engine-ut99' }` and `:66` `base: 'maximqaxd/ut99dc (UnrealTournament v400)'` are hardcoded → change to `HarryPotter2` / `HP2 build 433 (EA/KnowWonder 2002)` |

### 1.14 History of `Source/UnrealTournament` (template)

The hard rule forbids running `git` inside `/home/ion/engines/*` (HP2 `CLAUDE.md`: "never write, build in,
or `git` in them"), so the per-path `git log` was **not** run. Reconstructed instead from the step markers
in the code and from reading (not executing) `engine-ut99/.git/logs/HEAD` (323 reflog lines):

| Step | Commit subject (reflog) | Launcher effect |
|------|-------------------------|-----------------|
| ut99dc base | — | XLaunch-derived SDL `main`, Dreamcast branches, `THIS_PACKAGE`, `GIsGuarded=0`, `HandleError` |
| 2.7b | (web layer) | `chdir("/System")`, Asyncify loop, bare loader `SDLLaunch` |
| 2.7c/2.7d | (web drivers, WebRTC) | forced web driver set, `ServerActors.Empty()`, `GWebRTCEngine`, `ue_exec_command` |
| 2.9a-4 | `feat(render): unified SelectRenderDevice launcher + persist + switch table` | switch table |
| 2.9/2.12 | `feat(softdrv)… -SOFTWARERENDERER`, `fix(softdrv): don't auto-adopt the shipped SoftDrv ini as the web default`, `feat(nulldrv)… -NULLRENDERER` | eligibility + marker |
| 2.12 | `fix(2.12): normal exit returns to the launcher on web (dead-canvas fix)` | `appRequestExit` web branch (Core) |
| 2.12/2.19 | (frame cap) | FrameRateCap native, then shared |
| 2.13.7 | `perf(2.13.7): drop the >=1ms yield floor — unclamped MessagePort macrotask` | yield (later reverted to setTimeout in 2.43) |
| 2.16/2.16b | `feat(2.16): AutoFOV…`, `feat(2.16b): AutoFOVUI…` | Engine, not launcher |
| 2.30/2.33 | `Launch: publish a frame-PACING distribution…`, `Launch: adaptive frame pacing…` | superseded — not in the current file |
| 2.33/2.34 | `Core: port the native-class layout guard from the 436 branch`, `build(2.34): hardware GL on Windows…` | `NormalizeSubsystem` |
| 2.38 | `fix(core): implement -ini:<File>:[<Section>]:<Key>=<Value>…` | Core `appApplyIniOverrides` |
| 2.43 | `fix(pacing): the per-frame yield was not presenting…`, `refactor(pacing): hardcode the timer yield, delete -YIELDMODE` | setTimeout yield + pacing ring |
| 2.61 | `config: the frame-rate cap lives in the ini only — drop the -FRAMERATECAP= switch`, `refactor(autofov): remove the command-line switches…` | ini-only settings |

Lesson carried to HP2 (the template's own rule): **one home per setting** — the ini; per-launch changes via
`-ini:`; no bespoke switches.


### 1.15 Errata / additions to §1 found in the 2026-09-26 verification pass

| # | Finding | Evidence | ACTION |
|---|---------|----------|--------|
| E1 | `Src/Res/` is **not** empty: `LaunchRes.rc/.h` + `Unreal.ico`, never compiled (no `.rc` in `SRC_FILES`). | `engine-ut99/Source/UnrealTournament/CMakeLists.txt:3`; `ls Src/Res` | HP2: replace `Source/Game/Src/Res/*` with a 3-line `Game.rc` (`IDI_ICON1 ICON "Game.ico"`, icon from `Source/Launch/Src/Res/Game.ico`) and add it to `SRC_FILES` only `if(MINGW)` — cosmetic, Phase F. Or delete the dead dir. |
| E2 | The log **file name comes from argv[0]** (`appPackage()`==`GModule`). With the `GModule[32]` overflow (§1.2 #2) this is two bugs in one line of `main`. | `Launch.cpp:838-841`, `UnUnix.cpp:760-765`, `FOutputDeviceFile.h:45-51` | HP2 launcher: `const char* B = strrchr(argv[0],'/'); B = B?B+1:argv[0];` (also `'\\'` on MinGW), strip a trailing `.exe`, `appStrncpy(GModule,B,ARRAY_COUNT(GModule))`. Result: `Game.log` natively, and on web `this.program.log` stays unless argv[0] is overridden — set `Module.thisProgram='Game'`? **No** — the frontend owns `Module`; leave web as is (stdout echo is the web log). |
| E3 | HP2's log path uses `appUserDir()` not `appBaseDir()`, and **`appUserDir()` has no Unix definition at all** (only `Core/Src/UnVcWin32.cpp:1048-1070`, which returns `My Documents\Harry Potter and the Chamber of Secrets\`). `appInit` uses it for `Game.ini`, `User.ini`, `SavePath` (`UnMisc.cpp:1915, 1935, 2002-2012`) and the engine for `Shot%04i.bmp`, `.rep` files. A Unix link fails with an undefined reference. | `grep -rn appUserDir Source/Core Source/Engine` | Core port defines `appUserDir()` in `UnUnix.cpp`: **return `""` by default** (inis, log, `../Save` relative to `System/`, exactly the template's layout that the frontend's `iniPath: "System/…ini"` and `userFiles` assume); opt-in `-usehomedir` → `$HOME/.hp2/` (Windows: `%USERPROFILE%\Documents\Harry Potter and the Chamber of Secrets\`) mirroring ut2004's 2.37 policy (`engine-ut2004/Source/Core/Inc/FFileManagerUnix.h:694-715`). Details §2.2.4. |
| E4 | HP2's `Core/Inc/FFileManagerLinux.h` is **abstract** (no `GetGlobalTime`/`SetGlobalTime`, pure in `Core.h:333-334`), so the launcher's `FFileManagerLinux FileManager;` does not compile; and its `MakeDirectory` passes `__S_IREAD && __S_IWRITE && __S_IEXEC` (logical AND = mode **1**, `d--------x`) → the `Save/` and `Save/Slot<N>/` dirs `appInit` creates would be unwritable. | `diff engine-ut99/…/FFileManagerLinux.h HarryPotter2/…/FFileManagerLinux.h` | Core port takes the template's `FFileManagerLinux.h` wholesale (it also carries the MinGW `mkdir` branch the launcher needs on Windows). Keep its `GetGlobalTime()==0` — see §2.2.4 (the HP2 "//ft" regeneration rule depends on it). |
| E5 | HP2 has no `appProcessSeconds()` on Unix (declared `UnFile.h:263`, defined only in `UnVcWin32.cpp`); HP2's own `InitEngine` calls it. | `grep appProcessSeconds Source/Core/Src` | The new launcher must not call it (the template's `InitEngine` does not). |

## 2. HP2 original launcher (Source/Launch, Engine/Inc/UnEngineWin.h, Source/FrontEnd)

Cross-reference: `hp2-game-boot-and-content.md` §1-§4 walks the same boot as a *content* flow (which maps,
which saves, which .int strings). This section is the *launcher-code* view: what each piece of the Win32
launcher does, whether it is stock v400 or HP2, and the keep/drop/change verdict for `Source/Game/Src/Launch.cpp`.

### 2.1 `Source/Launch/Src/Launch.cpp` (290 lines, Win32 `WinMain`) — line by line

Diff basis: `diff --strip-trailing-cr -w /home/ion/epic-sources/ut99-v400/Launch/Src/Launch.cpp Source/Launch/Src/Launch.cpp`.

| Lines | What | Origin | Verdict for the SDL launcher |
|-------|------|--------|------------------------------|
| 9-10 | `#include "LaunchPrivate.h"` (windows.h, commctrl, shlobj, `Window.h`, `Res\LaunchRes.h`) + `UnEngineWin.h` | stock | DROP — the SDL launcher includes only `Engine.h`+`UnRender.h` (template `LaunchPrivate.h`). **`UnEngineWin.h` must never be included by a GCC TU** (PLAN Phase A note). |
| 17-18 | `HINSTANCE hInstance`; `TCHAR GPackage[64]="Launch"` | stock | KEEP the `GPackage`/`THIS_PACKAGE` array (template `:151`); `hInstance` DROP |
| 22-30 | `FMallocWindows` (`TMallocDebug<>` in `_DEBUG`) + **`TMallocLog<FMallocWindows> LogMalloc`** selected by `-MEMSTAT` (`:139-140`) | **HP2** (MEMSTAT) | DROP. Template uses `FMallocAnsi`. HP2's `TMallocLog` (`Core/Inc/FMallocDebug.h:162ff`) is a tagged allocation tree fed by `FPushMemTag` (`UnFile.h:503-520`); it is portable C++ and *could* wrap `FMallocAnsi`, but HP2's Unix `USystem::Exec` answers `MEMSTAT` with "MEMSTAT command not available" (`Core/Src/UnUnix.cpp:83-88`), so nothing would print the stats. Optional later (Q-list). |
| 33-49 | `FOutputDeviceFile Log`, `FOutputDeviceWindowsError`, `FFeedbackContextWindows`, `FFileManagerWindows`, `FConfigCacheIni.h` | stock | REPLACE with the template's set: `FOutputDeviceFile`, `FOutputDeviceAnsiError`, `FFeedbackContextAnsi`, `FFileManagerLinux` (template `:153-173`) — on **every** GCC target incl. MinGW |
| 51-65, 85-95 | **SafeDisc**: `#include "CdaPfn.h"` (C-Dilla/Macrovision "SafeDisc API Protected Function" header, `CDAPFN_DECLARE_GLOBAL`, `CDAPFN_ENDMARK`), `CheckSafeDiskProtection()` in a `try` → `MessageBox("Copy protection failed.")` | **HP2** | DROP entirely. `CdaPfn.h` is MSVC-only (`#pragma pack`, `__declspec`) and is copy-protection plumbing. Do not copy it into `Source/Game`. |
| 97-127 | single-instance forwarding: find a running `WLog` window with prop `IsBrowser`, `WM_COPYDATA` the command line to it, exit | stock | DROP (no second process on web; natively not needed). The web equivalent — "send a command to the running engine" — is `ue_exec_command` (template `:121-145`). |
| 137 | `GIsClient = GIsGuarded = 1` | stock | CHANGE to template's `GIsClient=1; GIsGuarded=0` (exceptions-off contract, §1.10) |
| 139-141 | `-MEMSTAT` allocator pick; `appInit( GPackage, CmdLine, … , 1 )` with `GPackage == appPackage()` == `"Game"` (Win32 `appPackage()` = exe basename, `UnVcWin32.cpp:1132ff`) | stock + HP2 | `appInit( TEXT("Game"), CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 )` → `Game.ini`/`User.ini`. **Hard-code "Game"** as the template hard-codes "UnrealTournament" (never derive it from argv[0]: the web argv[0] is `this.program`). |
| 144-148 | `GIsServer=1; GIsClient=!SERVER; GIsEditor=0; GIsScriptable=1; GLazyLoad=!GIsClient\|\|LAZY` | stock | KEEP (template `:886-894` identical) |
| 154 | `ShowLog = ParseParam(appCmdLine(),"LOG")` | stock | KEEP as template's `-LOG` → `Warn.AuxOut=GLog; GLog=&Warn` (on Win32 it opened the `WLog` window). |
| 155-158 | **`RunCount`**: read `[Engine.Engine] RunCount`, write `RunCount+1` | **HP2** | KEEP (1 line, harmless, keeps the ini identical to retail's). Nothing reads it (`grep -rn -i RunCount Source --include=*.uc` → no hits). |
| 156-162, 184-188 | splash file names: `..\Help\EALogo<lang>.bmp` → `..\Help\EALogo1.bmp`; `..\Help\WBLegal<lang>.bmp` → `..\Help\WBLegal.bmp`. NB `FString::Printf(TEXT("…%s…"), lang)` passes an **`FString` object** through varargs (works on MSVC by layout accident) | **HP2** | DROP (no splash). Do not port the varargs-FString bug. The bitmaps exist in `Assets/Help/` (`EALogo1.bmp`, `WBLegal.bmp`, no `<lang>` variants) and again in `Assets/System/`; a web splash, if ever wanted, is HTML in the frontend using these files. |
| 165-200 | **splash sequence**: unless `-SAVESLOT=` or `-NOFRONTEND`: EA splash, sleep `[Engine.Engine] EASplashWaitTime` (1.5 s), WB legal splash, sleep `WBSplashWaitTime` (1.5 s). Suppressed also by `-LOG`, `server`, `TestRenDev`. | **HP2** | DROP the display **and the sleeps**. Headless equivalent = nothing: a 3 s `appSleep` before `Engine->Init` would only delay boot (and on web would block the worker before the first yield). Leave the two ini keys in `Default.ini` (retail fidelity; unused). If the owner wants the legal screens, that is a frontend (HTML) splash, not engine code. |
| 202-209 | `InitWindowing()`, `WLog` log window, `SetPropX(IsBrowser)` | stock | DROP (NSDLDrv owns windowing; GLog→stdout replaces WLog) |
| 211-233 | "demo check": logs `FileSize` of `Engine.dll`, `HPMenu.u`, `HarryPotter.u`; the size gate is commented out | **HP2** | DROP (all three names are absent in a static build / this drop — `HPMenu.u`, `HarryPotter.u` do not exist in `Assets/System`) |
| 236-256 | `InitEngine()`; `ExitSplash()`; `-EXEC=<file>` → `Viewports(0)->Exec("exec <file>")`; `MainLoop` | stock (+ HP2 splash) | KEEP (template `:906-925` identical apart from the output device) |
| 260-261 | `if(!GIsCriticalError) GMalloc->DumpAllocs()` | **HP2** | DROP (Ansi allocator's `DumpAllocs` is a no-op) |
| 262 | `GFileManager->Delete( appUserDir() * "Running.ini" )` | stock (+ HP2 path) | KEEP the template's `Delete("Running.ini")` in `CleanUpOnExit` (nothing creates it in the template; HP2's `InitEngine` creates it — see §2.2.3, we do not) |
| 267-268 | **`if( Engine && Engine->Audio ) Engine->Audio->SetViewport( NULL ); //kill the audio cleanly`** before `appPreExit` | **HP2** | **KEEP — add to `CleanUpOnExit`** (needs the `Engine` pointer: make `CleanUpOnExit(INT, UEngine*)` or a file-static). HP2's audio (ALAudio → NOpenALDrv port) streams Ogg music on a thread (`FFileStream`, `Core/Src/UnMisc.cpp:1611ff`); detaching the viewport stops the streams before `appPreExit` tears the objects down. Harmless for `SoundDrv`. |
| 270-285 | `appPreExit(); GIsGuarded=0; … catch(...) { Error.HandleError(); } appExit()` | stock | KEEP template's `CleanUpOnExit` + `HandleError` |

### 2.2 `Source/Engine/Inc/UnEngineWin.h` (1982 lines; epic v400 has 918)

`diff --strip-trailing-cr -w` vs epic: 1174 changed lines; HP2 added ~850 lines at `:167-1007` (the in-engine
front end) plus the splash/D3D-detect helpers at `:19-73`. Everything in this header is Win32 (windows.h,
`Window.h` controls, `ShellExecute`, `CreateDialog`, registry). **No part of it is compiled by the port**;
the table says which *behaviours* the SDL launcher must reproduce.

#### 2.2.1 Inventory

| Lines | Item | Origin | What it did | SDL launcher verdict |
|-------|------|--------|-------------|----------------------|
| 21-72 | `UseD3DDriver()` | HP2 | reads `[D3DDrv.D3DRenderDevice] dwVendorId/dwDeviceId`, `[CARDCAP] VRAM`, a per-card section `<vendor>.<device>` with `Softwareonly`/`topRes`/`shadows` → writes `[optionsettings] MaxRes/shadowsOn`, decides D3D vs software | DROP. Renderer choice = template's `SelectRenderDevice()` (§1.3). `[optionsettings] MaxRes`/`shadowsOn` are **not read by any script** (`grep -rn -i 'optionsettings\|MaxRes\|shadowsOn' Source --include=*.uc` → no hits), so nothing needs seeding. |
| 74-127 | splash: `SplashDialogProc`, `ThreadProc` (a message-pump thread showing a bitmap), `InitSplash(Filename)`, `ExitSplash()`; globals `ThreadId`, `Reconfig` | stock (+`Reconfig`) | DROP |
| 129-151 | `InitSysDirs()` (`GetSystemDirectory`, `GetModuleFileName` → `ThisFile`, `.ICD`→`.EXE` = SafeDisc wrapper name) | stock | DROP |
| 153-164 | `CreateDetected()`: if no `-nodetect`, deletes `<UserDir>\Detected.ini`, **re-launches itself** `testrendev=D3DDrv.D3DRenderDevice log=Detected.log` and polls up to 80 s for `Detected.ini` | stock | DROP (and `testrendev=` below) |
| 170-190 | front-end constants: `SAVE_FILENAME="Save0.usa"`, `SAVE_FILENAME_THUMBNAIL="Save0.bmp"`, `..\Help\Background.bmp`, `ButtonUp.bmp`, `ButtonThumbUp.bmp`, **`NEWGAME_COMMAND_LINE_TOKEN="PrivetDr.unr"`**, **`LOADGAME_COMMAND_LINE_TOKEN="-LOAD=0"`**, **`NUM_SAVE_SLOTS=7`** (slots 1..6 used) | HP2 | These are the **contract the frontend's slot picker must reproduce** (§2.5). |
| 200-339 | `class WFrontEnd : WFrontEndDialog` — owns 6 `SaveSlotData{bEmpty, Dir, Thumbnail}`; `OnInitDialog` creates `SavePath/Slot<i>` dirs, marks a slot used iff `Slot<i>/Save0.usa` size > 0, **deletes every file of an empty slot** (`CleanSaveSlotDir`, so no stale `*_pa.usa`); `LaunchNewGame(i)`: `ShellExecute(ThisFile, appCmdLine()+"PrivetDr.unr"+" -SAVESLOT=<i>")` + `EndDialog`; `LaunchLoadGame(i)`: same with `"-LOAD=0"` | HP2 | Behaviour → frontend (§2.5). ⚠ Note the concatenation has **no space** before the token: `appCmdLine()+"PrivetDr.unr"` → with a non-empty original command line (e.g. `-log`) the relaunch line is `-logPrivetDr.unr -SAVESLOT=1`, which breaks both the map token and `-log`. Retail was launched with an empty command line, so it worked; the web frontend must build the argv cleanly. |
| 341-436 | `WFrontEndPageNewGame`: 6 slot buttons (`GetButton(0)` aliases slot 1), used slot → `MessageBox(HPMenu.int [All] Select_Game_0001/0002)` confirm, `CleanSaveSlotDir`, `LaunchNewGame` | HP2 | frontend (§2.5) |
| 438-529 | `WFrontEndPageLoadGame`: empty slots disabled; `LaunchLoadGame` | HP2 | frontend (§2.5) |
| 531-668 | `WFrontEndPageConfigSound`: `[Engine.GameEngine] UseSound`, `[ALAudio.ALAudioSubsystem] UseEAX`, `SoundVolume`, `MusicVolume` (0..1 floats) | HP2 | DROP the dialog; the in-game `FESoundVideoPage` covers volumes (via `ini:Engine.Engine.AudioDevice …`, see `hp2-game-boot-and-content.md` §1.3). NOpenALDrv must expose `SoundVolume`/`MusicVolume` as 0..1 floats (Phase E). `UseSound=False` must be honoured by `UGameEngine::InitAudio` (stock). |
| 670-875 | `WFrontEndPageConfigVideo`: D3D vs Software radio, resolution list (D3D 640x480..1280x1024; Software **512x384 only**, forces 16-bit colour), `[WinDrv.WindowsClient] StartupFullscreen`, windowed checkbox visible only with `[HGame.baseConsole] bDebugMode=True`; writes `GameRenderDevice` | HP2 | DROP. Resolution/fullscreen belong to `[NSDLDrv.NSDLClient]` (web: canvas-sized). Renderer = `-*RENDERER` switch + frontend `videoDrivers` list. |
| 877-934 | `WFrontEndPageConfig`: Video / Sound / Joystick (`control.exe joy.cpl`) | HP2 | DROP |
| 936-1004 | `WFrontEndPageMainMenu`: New / Load / Options / Quit; calls `CreateDetected()` when `[D3DDrv.D3DRenderDevice] dwVendorId` is missing (i.e. on first run) | HP2 | DROP (the frontend's launcher page *is* the main menu on web) |
| 1011-1442 | `WConfigWizard` + pages `FirstTime`, `SafeOptions` (`-nosound -no3dsound -nohard -noddraw -defaultres -nommx -nokni -nok6 -nojoy`, reset-config deletes `<pkg>.ini`), `Detail`, `Driver`, `Renderer`, `SafeMode` (Run/Video/Safe/Web) | stock (HP1-era, **unused**: the only instantiation is inside a `/* … */` block at `:1771-1795`) | DROP. None of the `-no*` switches exists in the SDL/Unix path; do not invent them. |
| 1444-1491 | `LaunchMplayer()` (registry `software\mpath\mplayer\main`) | stock | DROP |
| 1495-1648 | `FExecHook` (`GExec`): `ShowLog`, `TakeFocus`, `EditActor Class=/Name=` (Legend 2000 fix → `WObjectProperties` window), `HideLog`, `Preferences` (server only), `MPLAYER`, `HEAT` | stock | DROP (template sets `GExec = NULL`). `EditActor` needs the Win32 property window; no web equivalent. |
| 1657-1877 | `InitEngine()` — see 2.2.2 | stock + HP2 | see 2.2.2 |
| 1882-1976 | `MainLoop()` — `FTime` timing, `CurrentTickRate = 1/DeltaTime` every frame, `GetMaxTickRate` cap, message pump, editor throttle | stock + HP2 (FTime) | REPLACE with the template's loop (FrameRateCap, Asyncify yield, pacing ring) using `FTime` arithmetic (§1.8). |

#### 2.2.2 HP2 `InitEngine()` (`UnEngineWin.h:1657-1877`) step by step

```cpp
FPushMemTag Push(TEXT("InitEngine"));                        // HP2 memtag (MEMSTAT)
FTime LoadTime[2] = { appSeconds(), appProcessSeconds() };   // HP2: FTime + process time
static FExecHook GLocalHook;  GExec = &GLocalHook;           // stock
CreateMutexX( NULL, 0, TEXT("UnrealIsRunning"));             // stock (installer check)
INT FirstRun=0; INT ForceSoftware=0;
GConfig->GetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
GConfig->GetInt( TEXT("FirstRun"), TEXT("Reconfig"), Reconfig );          // HP2
GConfig->GetInt( TEXT("FirstRun"), TEXT("ForceSoftware"), ForceSoftware ); // HP2
//	if(Reconfig) FirstRun=0;		//gk causes dialog right away
#if 0	// Obsolete code.
	if( FirstRun<220 ) { /* migrate savegames: set UnrealShare.UnrealSlotMenu SlotNames[i] */ }
#endif
```
- **stock v400 had** `if( ParseParam(appCmdLine(),TEXT("FirstRun")) ) FirstRun=0;` — HP2 **removed** the `-FirstRun` switch.
- **The `FirstRun<220` block is `#if 0`** and was a *savegame-name migration* (writes `[UnrealShare.UnrealSlotMenu] SlotNames[i]="Saved game"` for each `*.usa`), **not a config dialog**. The first-time *config dialog* (`FirstRun<ENGINE_VERSION → WConfigPageRenderer`) is inside the commented-out "Old HP1 code" block (`:1771-1795`). So in HP2 **nothing is gated on `[FirstRun]`**: it is read, then stamped `FirstRun=433` (`:1816-1819`). `Reconfig` and `ForceSoftware` are read into variables nobody uses.

Then, in order:

| Step | Code | Origin | SDL verdict |
|------|------|--------|-------------|
| `consolecommand=<cmd>` | `GExec->Exec(Command)`, **returns NULL** (process exits without an engine) | stock (for mplayer/heat) | DROP (with `GExec=NULL` it would crash; the web has `ue_exec_command`) |
| `testrendev=<Device>` | load + `Init(NULL,0,0,0,0)` the device inside `try{}catch(...){}`, write `DescFlags`, create `Detected.ini`, return NULL | stock | DROP (D3D probe; exceptions are off anyway) |
| Config UI | `if(!GIsEditor && GIsClient && !GDebugger)`: **unless `SAVESLOT=` or `NOFRONTEND` is on the command line**, run the modal `WFrontEnd` main menu; if it returns 0 (a relaunch happened or Quit) → return NULL (this process exits) | **HP2** | DROP the UI. KEEP the *semantics*: a bare boot (no map, no slot) is legal and lands on `Startup.unr` (§2.4). `GDebugger` (HP2's UnrealScript debugger, `Core.h:467`) stays NULL. |
| load splash | `..\Help\splashint.bmp` while loading (not with `-LOG`/`server`/`TestRenDev`) | HP2 | DROP (the file is `Assets/Help/Splashint.bmp`, plus per-language `splash<lang>.bmp`, `Splash0..4.bmp`; only a Windows case-insensitive FS finds it under the lower-case name) |
| `Running.ini` | create `<UserDir>\Running.ini` (semaphore for the dead "recovery mode" page) | stock | DROP the create (template does not create it); keep the harmless delete |
| FirstRun stamp | `if(FirstRun<ENGINE_VERSION) FirstRun=ENGINE_VERSION; SetInt("FirstRun","FirstRun",FirstRun)` | stock | KEEP — but **read first** (HP2 semantics), see §2.6 |
| CD check | `[Engine.Engine] CdPath` non-empty and `..\Textures\Palettes.utx` missing → loop `MessageBox("InsertCd")` until `CdPath\Textures\Palettes.utx` exists, Cancel → `ExitProcess(0)` | stock | DROP. HP2's `Default.ini` has **no `CdPath`** key (grep → none), so it never fired; on Unix a message-box loop would hang headless runs. |
| `ENGINE_VERSION<230` German story | compiled out (433) | stock | DROP |
| engine | `StaticLoadClass(UGameEngine, "ini:Engine.Engine.GameEngine", LOAD_NoFail)`; `ConstructObject`; `Engine->Init()`; log `Startup time: %f seconds total, %f app` | stock | KEEP template's (with `SelectRenderDevice`, normalisation, `ServerActors.Empty()`, `PersistRenderChoiceIfSwitched` around it) |

#### 2.2.3 HP2 `MainLoop()` (`UnEngineWin.h:1882-1976`)

```cpp
FTime OldTime = appSeconds();
while( GIsRunning && !GIsRequestingExit ) {
    FTime NewTime   = appSeconds();
    FLOAT DeltaTime = NewTime - OldTime;           // FTime::operator-(FTime) -> float
    Engine->CurrentTickRate = 1.f / DeltaTime;     // HP2: per-frame, stock v400 averaged per second
    Engine->Tick( DeltaTime );
    if( GWindowManager ) GWindowManager->Tick( DeltaTime );
    OldTime = NewTime;
    FLOAT MaxTickRate = Engine->GetMaxTickRate();
    if( MaxTickRate>0.0 ) appSleep( Max(0.f, (1.0f/MaxTickRate) - (appSeconds()-OldTime)) );
    /* PeekMessage pump; editor throttle */
}
```
It is the reference for the **FTime rewrite** of the template loop (§1.8): every `DOUBLE x = appSeconds()` becomes
`FTime x = appSeconds()`, every difference is `FTime - FTime` (float), and the pacing ring's `DOUBLE Start` is
stored as `(appSeconds() - GUfrontEpoch)` float or via `.GetFloat()` (`FTime::GetFloat` exists, `Core.h:39`).
Note `FTime(double)` exists but there is **no `FTime + double`** and **no `FTime > double`** (only `operator+(float)`,
`operator>(FTime)`), so `if( OldTime > SecondStartTime + 1 )` must become `if( OldTime - SecondStartTime > 1.f )`.
`GetMaxTickRate()` for HP2 standalone returns **0** (`Engine/Src/UnGame.cpp:1683-1695`: only a net driver or a demo
recorder yields a rate), so the template's `FrameRateCap` branch (default 120) always engages in single player — the
boot log must show `ufront: client frame rate cap 120 fps`.

#### 2.2.4 What `appInit` (Core) does differently in HP2 — the launcher's contract with it

These are Core-owned, but they decide what the launcher must pass and where files land; quoted because the
launcher spec depends on them (`Source/Core/Src/UnMisc.cpp`):

```cpp
// :1913-1929  (template: appSprintf(GIni,"%s.ini",InPackage) and create only if missing)
if( !Parse( appCmdLine(), TEXT("INI="), GIni, 256 ) )
    appSprintf( GIni, TEXT("%s%s.ini"), appUserDir(), InPackage );
bool bDefaultIniChanged = false;
if(   RequireConfig
   && (   GFileManager->FileSize(GIni) < 0
       || GFileManager->GetGlobalTime(TEXT("Default.ini"))  >  GFileManager->GetGlobalTime(GIni) //ft
      ) )
{ /* Game.ini = copy of Default.ini */ bDefaultIniChanged = true; }
// :1933-1948  User.ini = copy of DefUser.ini if missing, if DefUser.ini is newer, OR if Game.ini was just regenerated
```

| # | HP2 behaviour | Consequence for the port | ACTION (owner) |
|---|---------------|--------------------------|----------------|
| A1 | ini paths are `appUserDir()+Game.ini` / `appUserDir()+User.ini` | with `appUserDir()==""` (E3) they are `System/Game.ini`, `System/User.ini` — the template layout and the frontend's `iniPath` contract | Core: `appUserDir()` returns `""` by default |
| A2 | **"//ft" rule: regenerate `Game.ini` from `Default.ini` whenever `Default.ini` is newer; then also regenerate `User.ini`** | With the template's `FFileManagerLinux::GetGlobalTime(){return 0;}` the rule is dead (0 > 0 false) — **good**. If anyone implements `GetGlobalTime` with `stat()`: on the **web** the preloaded `/System/Default.ini` gets a fresh MEMFS mtime on **every page load**, so every boot would wipe the persisted `Game.ini` **and** `User.ini` (render-choice marker, key bindings, player name, volumes) | Core: keep `GetGlobalTime` returning 0 on Unix/web, and add a comment at `UnMisc.cpp:1919` saying why. Launcher: nothing. Test: web boot twice after changing a key binding — it must stick. |
| A3 | gk 10/15/01 block (`:1954-1965`): opens a second config cache on **`"default.ini"`** (lower-case) and forces `[Engine.Engine] Language` from it; `DTemp` uninitialised when the read fails; `DConfig` leaked | On a case-sensitive FS `default.ini` ≠ `Default.ini` → garbage/empty `Language`. Also clobbers any `-ini:System:[Engine.Engine]:Language=` applied before it | Core: `"Default.ini"`, `DTemp[0]=0`, skip `SetString` on failure (as `hp2-game-boot-and-content.md` §4 says). **Launcher-relevant:** `appApplyIniOverrides()` must be called **after** this block (§1.4 item 2). |
| A4 | `FFileStream::Init( [ALAudio.ALAudioSubsystem] Channels or 32 )` (`:1994-1997`) | a Core singleton created from an *audio driver's* ini section | keep; the web `Default.ini` keeps an `[ALAudio.ALAudioSubsystem] Channels=` line or accepts 32 |
| A5 | `SavePath`: `appUserDir()*"Save"` + `GSys->Paths[5]` rewrite if user dir, **else `TEXT("..\\Save")`** (`:2002-2012`) | With `appUserDir()==""` the Unix path is the **literal** `..\Save` → `mkdir("..\\Save")` creates a directory named `..\Save` **inside `System/`**, and every save lands there (FFileManagerLinux only converts `\` in `FindFiles`) | Core: `TEXT("..") PATH_SEPARATOR TEXT("Save")`. Result `../Save` = `/Save` on web, `<run>/Save` natively — same as the template's `SavePath=../Save`. |
| A6 | **`SAVESLOT=<n>`** → `SaveSlotPath = SavePath*"Slot"+n` (`:2017-2025`, `appSetSaveSlotDir` `:1832-1839`); else `SaveSlotPath = SavePath`; creates `SaveSlotPath` and `SaveSlotPath/cache` | the slot is a **boot-time** choice: to switch slots the process must restart (retail relaunched `Game.exe`). No console command changes it. | Launcher: nothing to parse (Core does it) — but the **web frontend must restart the engine to change slot** (§2.5). |
| A7 | `MapPath = appBaseDir().Left(InStr("System")) + "Maps"` (`:2040-2057`) | needs `appBaseDir()` to contain `System` → the launcher's chdir (§1.11) must land in a dir named `System` | Launcher: native chdir to the exe dir; run tree puts `Game` in `System/` |
| A8 | `LoadPersistentActorCache()` (`:2060`): empties `SaveSlotPath/cache`, copies `SaveSlotPath/*_pa.*` into it | per-level world state is restored at boot from the slot | frontend must persist `Save/Slot<N>/*_pa.usa` (§2.5) |
| A9 | `SetString("Core.System","SavePath",SaveSlotPath)`, `SetString("Core.System","paths",SaveSlotPath*"*.usa")`, `GConfig->Flush(0)` (`:2062-2065`) | `appInit` **writes and flushes `Game.ini` before the launcher runs any code after it** — a `-ini:System:[Core.System]:…` override is overwritten here. The `paths` write replaces the **last** `Paths=` line (per `hp2-game-boot-and-content.md` §3 table). | none for the launcher; document |

### 2.3 `Source/FrontEnd` — the standalone Win32 "FrontEnd" app (not built, not shipped)

| Fact | Evidence |
|------|----------|
| 21 files, 1554 lines of plain Win32 C++ (no engine headers): `win_main.cpp` (`WinMain` + `PeekMessage` loop), `FE_Base.cpp` (the `CFrontEnd` class), `FE_SplashMenu/MainMenu/NewGameMenu/LoadGameMenu/ConfigMenu.cpp`, a `MenuManager/` dialog stack, a `Bitmap/` BMP blitter. Header: "Copyright 2002, Amaze Entertainment", "Originally created on 07/25/2002", author Elijah Emerson. | `Source/FrontEnd/Source/win_main.cpp:12-14,40-44` |
| It is an **engine-free prototype of the same launcher** that ended up inside `UnEngineWin.h` (`WFrontEnd`): splash queue `EALogo1.bmp`, `WBLegal.bmp` at `SPLASH_WAIT_TIME 1500` ms; main menu; New Game / Load Game over **6 slots** in `<My Documents>\Harry Potter and the Chamber of Secrets\Save\Slot<N>\` (fallback `<base>\..\Save`); a slot is used iff `Slot<N>\Save0.usa` size > 0, thumbnail `Save0.bmp`; Config = `control.exe joy.cpl` / `video.cpl` only. | `FrontEnd.h:27-60`, `FE_Base.cpp:78-160` |
| Launch contract (identical to `WFrontEnd`): `ShellExecute("game.exe", "<its cmdline>PrivetDr.unr -SAVESLOT=<N>")` for New Game, `"<cmdline>-LOAD=0 -SAVESLOT=<N>"` for Load Game, then `PostQuitMessage(0)`. | `FrontEnd.h:31-35`, `FE_Base.cpp:292-333` |
| Evidence that retail used the **in-engine** version, not this app: the shipped `Game.exe` contains the `WFrontEndPageMainMenu` strings (`hp2-game-boot-and-content.md` §1 build-identity note); there is no `FrontEnd.exe` in `Assets/System`. | `ls Assets/System` |

**Verdict:** reference only. It confirms the *contract* (slot dirs, `Save0.usa`/`Save0.bmp`, the two
command-line shapes) twice over, which is what the web frontend's slot picker must reproduce (§2.5). Nothing
from it is ported to C++.

### 2.4 URL / map defaults, `autoplay.unr`, `Startup.unr`, `HGame.HPConsole` at boot

**Ini (`Config/System/Default.ini:1-12`):**
```ini
[URL]
Protocol=unreal
Name=Player
Map=autoplay.unr
LocalMap=Startup.unr
MapExt=unr
SaveExt=usa
Port=7777
Class=Engine.Playerpawn
```

| Key | Consumer | Reality in this drop | ACTION |
|-----|----------|----------------------|--------|
| `Map=autoplay.unr` → `FURL::DefaultMap` | `Engine/Src/UnURL.cpp:35`; used only by default-constructed `FURL()` (`:97, :120`) and a host-only URL (`:254`) — never by single-player boot | **`autoplay.unr` does not exist** (`find Assets -iname '*autoplay*'` → nothing). It is the file UnrealEd's "Play level" writes (`Maps/Autoplay.unr`, launched with `-NOFRONTEND`), and `FEBook.Created()` closes the book when the URL contains `AUTOPLAY` (see `hp2-game-boot-and-content.md` §2). | Web `Default.ini`: set `Map=Startup.unr` (so a stray host-only `open` cannot resolve to a missing file — which on web with exceptions off is an abort). Harmless natively; keep retail value in `Config/System/Default.ini`. |
| `LocalMap=Startup.unr` → `FURL::DefaultLocalMap` | `UGameEngine::Init` (`Engine/Src/UnGame.cpp:180-185`) when the first command-line token is empty, `SERVER`, or starts with `-` | on disk the file is **`Maps/startup.unr`** (lower-case, 83,009 bytes). Resolved natively by HP2's second, case-insensitive pass in `appFindPackageFile` (`Core/Src/UnMisc.cpp:1515ff`) and on the web by the manifest's lower-cased keys (`targets/ut99/file-manifest.json` keys are lower-case → the FS is case-insensitive). | keep. Add the template's `appIsGlobalOptionToken` guard to HP2 `UnGame.cpp:180-185` — **REQUIRED**, because the web argv always carries `CPUSPEED=3000` and a "Continue" launch has **no map token** (§2.5), so `CPUSPEED=3000` would become the map. |
| (no key) `Entry` | `UGameEngine::Init` `LoadMap(FURL("Entry"))`, `appErrorf` on failure (`UnGame.cpp:167-171`) | `Maps/Entry.unr`, 6,626 bytes; imports `Editor.B_PlyrOn` (needs `System/Editor.u`) | keep; precache `Maps/Entry.unr` and `System/Editor.u` |

**What the player sees at boot, per command line** (native and web, once the port matches retail):

| argv (after `CPUSPEED=3000` is ignored) | `SaveSlotPath` | First level | Menu state |
|----------------------------|----------------|-------------|------------|
| *(nothing)* | `../Save` | `Startup.unr` (night sky, Hedwig fly-by, title logos, `Music_StoryBook_v2.ogg`) | **book closed** — `HPConsole.RenderUWindow` creates `FEBook` and immediately `CloseBook()`s (`HGame/Classes/Menu/HPConsole.uc:1129-1149`; the "STARTUP opens Main" branch is commented out, `:1131-1144`). The player is on a backdrop with no menu; **Esc** opens the in-game book (`EscFromConsole`, `HPConsole.uc:776-779`, `FEBook.uc:563-568`), **F11** opens the quit dialog. |
| `PrivetDr.unr -SAVESLOT=N` | `../Save/SlotN` | `PrivetDr.unr` (new game) | book closed; autosaves go to `Save/SlotN/Save0.usa` |
| `-LOAD=0 -SAVESLOT=N` | `../Save/SlotN` | `Startup.unr`, then `ServerTravel("<SaveSlotPath>/Save0.usa")` at the end of `UGameEngine::Init` (`UnGame.cpp:245-256`, "HP2: load game command line parameter support") | book closed |
| `<Map>.unr` (dev) | `../Save` | `<Map>` | book closed |

`HGame.HPConsole` boot sequence (`[Engine.Engine] Console=HGame.HPConsole`, `Default.ini:27`):
1. `UGameEngine::Init` constructs it (`UnGame.cpp:227-229`); `_Init(Viewport)`.
2. First `Tick`: `if(firstRun==false) { LaunchUWindow(); firstRun=true; }` (`HPConsole.uc:343-347`, "kludge to insure menu comes up first") → `WindowConsole.LaunchUWindow` sets `Viewport.bShowWindowsMouse=True`, `bUWindowActive`, state `UWindow` (`UWindow/Classes/WindowConsole.uc:241-257`).
3. First `RenderUWindow`: `CreateRootWindow`, `root.SetScale(RealWidth/640)`, `SetupLanguage()` (font set by `GetLanguage()`), create `FEBook` (all pages), then `MenuBook.bGamePlaying=true; MenuBook.CloseBook(); HideConsole();` → `CloseUWindow` → `bShowWindowsMouse=False` (`FEBook.uc:487-492`, `WindowConsole.uc:262-275`).
4. Quit: in-game Quit / main-page Exit → `Root.DoQuitGame()` → `SaveConfig()` ×3 → `ConsoleCommand("exit")` (`UWindow/Classes/UWindowRootWindow.uc:469-476`) → `UGameEngine::Exec EXIT` → `appRequestExit(0)` (`Engine/Src/UnGame.cpp:400-414`) → `GIsRequestingExit=1` → the launcher's `MainLoop` ends → `CleanUpOnExit`. **On web this needs the template's `appRequestExit` web branch** (return to the launcher page), §1.9.

**Launcher implications:**
- The launcher needs **no HP2-specific map logic**: map, `-SAVESLOT=`, `-LOAD=` are all consumed by Core/Engine.
- The web mouse: steps 2-3 toggle `bShowWindowsMouse` — the template's `[webmouse]` block in `UGameEngine::Tick` (engine-ut99 `Engine/Src/UnGame.cpp:1599-1640`, keys on `bShowMenu || bShowWindowsMouse`) therefore releases pointer lock while the book is open and re-captures on close with **no HP2-specific code**. HP2's `UnGame.cpp:1722-1739` has only the stock pause-driven capture (it does consider `bShowWindowsMouse`, but only when `!IsFullscreen()` and via the pause transition) → Engine analyst ports the template's web block; the launcher's only part is `CaptureMouse=True` (§1.5).

### 2.5 Save slots (`-SAVESLOT=`) — who chooses, and how

| Layer | Retail (Win32) | Port (native) | Port (web) |
|-------|----------------|---------------|------------|
| slot picker UI | `WFrontEnd` dialog in `Game.exe` (process 1), relaunches `Game.exe` (process 2) | none — pass the switches by hand / script (`run.sh -SAVESLOT=1 PrivetDr.unr`) | **the ufront launcher page** (targets/hp2 + a small HP2 panel, or generic "launch profiles"): New Game in slot N / Continue slot N |
| slot selection | argv `-SAVESLOT=N` | same | argv `-SAVESLOT=N` via `engineOpts`; the map (`PrivetDr.unr`) via `cmdLine` (argv[0]) |
| directory | `My Documents\…\Save\SlotN` | `<run>/Save/SlotN` (with `appUserDir()==""`, A5 fixed) | `/Save/SlotN` |
| "slot used" test | `SlotN/Save0.usa` size > 0 | — | frontend reads the user-data store |
| new game on used slot | confirm, delete every file in `SlotN` | — | frontend deletes `Save/SlotN/*` (+ `cache/`) from the user-data store **before** boot |
| slot switch while running | impossible (restart) | restart | restart (`appRequestExit` → page reload → pick slot) |

**Constraints for the frontend contract (hand to the ufrontend-target analyst / Phase D):**
1. `userFiles` globs allow `*` only in the file name (`targets/ut99/target.json` `_userFilesComment`), so HP2 needs **explicit per-slot globs**: `Save/*.usa`, `Save/*.bmp`, and for N=1..6 `Save/SlotN/*.usa`, `Save/SlotN/*.bmp`. The `Save/SlotN/cache/` dir is rebuilt from `SlotN/*_pa.*` at every boot by `LoadPersistentActorCache` (A8) — it need **not** be persisted (verify by booting with an empty `cache/`).
2. argv shapes (after the frontend's own `CPUSPEED=3000`): New → `["PrivetDr.unr", "CPUSPEED=3000", "-SAVESLOT=N", …]`; Continue → `["CPUSPEED=3000", "-LOAD=0", "-SAVESLOT=N", …]`. The second shape is exactly the one that needs `appIsGlobalOptionToken` in HP2's `UGameEngine::Init`.
3. `targets/hp2/target.json` `defaults.map` = `PrivetDr` is the closest to a retail "Play" (as `hp2-game-boot-and-content.md` recommends); the slot default `-SAVESLOT=1`.
4. A launch **without** `-SAVESLOT=` saves to `Save/Save0.usa` (+ `Save/*_pa.usa`), which the frontend should also persist (glob in item 1).

**Engine side (Core, not the launcher):** nothing to add — `appInit` already parses `SAVESLOT=`; the launcher must
**not** re-parse it (retail's `Launch.cpp` only used it to skip splashes).

**`-ini=` / `INI=` (the UE1 "total conversion" switch) in HP2.** The frontend's conversion mechanism passes a raw
param such as `-ini=Infiltration.ini` (`targets/ut99-436-compat/target.json` `conversions`,
`packages/frontend/src/config/controller.ts:366-372`); Core's stock `Parse(appCmdLine(),"INI=",GIni)` picks it up
(the `-` satisfies `appStrfind`'s non-alphanumeric lead-in). In HP2 it would mean: `GIni` = that file relative to
`System/` (no `appUserDir()` prefix), created as a copy of **`Default.ini`** if missing, then the gk block forces
`Language` from `Default.ini` into it, and `appInit` **overwrites its `[Core.System] SavePath` and its last
`Paths=` line** with the save-slot path (A9) — a conversion ini whose last `Paths=` is one of its own package dirs
would lose it. HP2 has **no known total conversions** and the drop contains none (`Assets/System/*.ini` → none at all).
**ACTION:** `targets/hp2/target.json` `"conversions": []` with a comment like ut99's; no launcher code. If a TC ever
appears, fix A9 first (write `Paths[5]` by index, not the `"paths"` key).

### 2.6 Spec for `Source/Game/Src/Launch.cpp` (executable `Game`, web `SDLLaunch.js`)

Start state: the file is a byte-identical copy of the template (§1.1). The spec below is the **complete delta**;
anything not listed stays exactly as in the template (per PLAN ground rule 1).

#### 2.6.1 Switch / option table the HP2 launcher (and the code behind it) honours

| Switch | Parsed where | Behaviour in HP2 port | Status vs template |
|--------|--------------|-----------------------|--------------------|
| `<Map>[?opts]` (first token) | `UGameEngine::Init` | startup URL; else `[URL] LocalMap=Startup.unr` | same (needs the `appIsGlobalOptionToken` port in HP2 `UnGame.cpp:180-185`) |
| `-XOPENGLRENDERER` `-UTGLRRENDERER` `-SOFTWARERENDERER` `-NULLRENDERER` | launcher `SelectRenderDevice` | as template | same, **minus `-NOPENGLESRENDERER`** |
| `-ini:<File>:[<Section>]:<Key>=<Value>` | Core `appApplyIniOverrides` | writes the key; `File` = `System`/`User` generic names (HP2 `FConfigCacheIni.h:119-122` translates) | port to Core; call **after** the gk Language block (§1.4) |
| `INI=<file>` / `-ini=<file>` | Core `appInit` | alternative game ini (the UE1 "TC" switch) | stock; HP2 caveats §2.5 |
| `USERINI=<file>` | Core `appInit` | alternative user ini | stock |
| `-LOG` | launcher | echo log via `FFeedbackContextAnsi` (doubles stdout lines, see §1.2 #9) | same |
| `LOG=<file>` / `ABSLOG=<path>` | `FOutputDeviceFile` | log file name | stock (HP2 prefixes `appUserDir()`, = "" after E3) |
| `-EXEC=<file>` | launcher | `exec <file>` on viewport 0 after init | same (HP2 retail had it) |
| `-SERVER`, `-LAZY` | launcher | dedicated server mode / lazy load | same (HP2 has no server game; harmless) |
| `-SAVESLOT=<n>` | Core `appInit` | `SaveSlotPath = ../Save/Slot<n>` | **HP2**, no launcher code |
| `-LOAD=<n>` | `UGameEngine::Init` (`UnGame.cpp:245-256`) | after init, `ServerTravel(<SaveSlotPath>/Save<n>.usa)` | **HP2**, no launcher code |
| `-REPLAY=<file>` / `-RECORD=<file>` | `UGameEngine::Init` (`UnGame.cpp:188-199`) | HP2 input replay system (`Engine/Src/UnReplay.cpp`, file `appUserDir()+<name>.rep`) | **HP2**, no launcher code; untested on web |
| `CDPATH=`, `-STRICT`, `lanplay` | Core / Engine | stock | stock |
| `-NOFRONTEND` | (retail launcher only) | **accepted, no-op** — there is no Win32 front end to suppress | document only |
| `CPUSPEED=<n>` | nobody on Unix (Win32 `UnVcWin32.cpp` only) | ignored; must not become the map | needs `appIsGlobalOptionToken` |
| `-MEMSTAT`, `consolecommand=`, `testrendev=`, `-nodetect`, `changevideo`, `safe`, `readini`, `-nosound`/`-no3dsound`/`-nohard`/`-noddraw`/`-defaultres`/`-nommx`/`-nokni`/`-nok6`/`-nojoy`, `Server`/`NewWindow` instance forwarding | retail Win32 only | **dropped** — not implemented, not emulated | — |

#### 2.6.2 Keep / drop / change checklist (template `Launch.cpp` → HP2 `Launch.cpp`)

- [ ] **Header comment + `LaunchPrivate.h`**: keep (`Engine.h`, `UnRender.h`).
- [ ] **Dreamcast**: delete every `PLATFORM_DREAMCAST` block (`:176-252`, `:807-829`, `:890-897`, `:600-602`, the `HandleError` branch) — PLAN ground rule 2.
- [ ] **`SimpleCopy`** (`:747-765`) and the **`~/.utconf` swap** (`:781-797`, `:853-870`): delete (dead code).
- [ ] **`GRenderSwitches` / `GRenderPreference`**: delete the `NOPENGLESRENDERER` row and the `NOpenGLESDrv…` preference (PLAN §2 "No NOpenGLESDrv"). Optional hardening: after step 3 of `SelectRenderDevice` finds nothing, fall back to `SoftDrv.SoftwareRenderDevice` when available (Phase B has only SoftDrv/NullDrv), instead of leaving the retail `D3DDrv.D3DRenderDevice` in the ini to `LOAD_NoFail`.
- [ ] **`main`: argv join**: bound it — `appStrncat( CmdLine, " ", ARRAY_COUNT(CmdLine) ); appStrncat( CmdLine, argv[i], ARRAY_COUNT(CmdLine) );` (HP2 has `appStrncat`, `UnFile.h:316`). The web argv carries `-ini:` tokens + URL; 1023 chars is reachable.
- [ ] **`main`: `GModule`**: basename of argv[0] into `GModule[32]` with `appStrncpy` (E2); then `appStrcpy( THIS_PACKAGE, appPackage() )` as template.
- [ ] **`main`: `appInit( TEXT("Game"), … )`** instead of `TEXT("UnrealTournament")`.
- [ ] **`main`: native base dir**: web keeps `chdir("/System")`. Native: preferred fix is in **Core** — adopt the template's `appBaseDir()` (`SDL_GetBasePath()`, `engine-ut99/Source/Core/Src/UnUnix.cpp:666-709`) so `./System/Game` works from any cwd; if the Core port keeps HP2's `getcwd()` version (`HP2 UnUnix.cpp:589-601`), the launcher must `chdir( dirname(argv[0]) )` (or `SDL_GetBasePath()`) **before** `appInit`. Either way the exe lives in `System/` (A7: `MapPath` needs `System` in the base dir).
- [ ] **`main`: `GIsClient=1; GIsGuarded=0`** before `appInit`: keep (exceptions-off).
- [ ] **`InitEngine`: web driver block** (`:483-526`), HP2 values:
  ```cpp
  GConfig->SetString( TEXT("Engine.Engine"), TEXT("ViewportManager"), TEXT("NSDLDrv.NSDLClient") );
  #if HP2_HAS_NOPENALDRV   // defined by Source/Game/CMakeLists.txt when BUILD_NOPENALDRV (today: defined, never read)
  GConfig->SetString( TEXT("Engine.Engine"), TEXT("AudioDevice"), TEXT("NOpenALDrv.NOpenALAudioSubsystem") );
  #else                    // until the Phase E 433 NOpenALDrv exists
  GConfig->SetString( TEXT("Engine.Engine"), TEXT("AudioDevice"), TEXT("SoundDrv.NullAudioSubsystem") );
  #endif
  GConfig->SetString( TEXT("Engine.Engine"), TEXT("NetworkDevice"), TEXT("WebRTCDrv.WebRTCNetDriver") );
  GConfig->SetString( TEXT("NSDLDrv.NSDLClient"), TEXT("CaptureMouse"), TEXT("True") );
  GConfig->SetString( TEXT("Engine.GameInfo"), TEXT("bWorldLog"), TEXT("False") );   // exist in HP2 GameInfo.uc:110-111 (globalconfig)
  GConfig->SetString( TEXT("Engine.GameInfo"), TEXT("bLocalLog"), TEXT("False") );
  // IpServer.UdpServerUplink DoUplink: DELETE the line (HP2 has no IpServer package)
  ```
  (Alternative with identical effect: `IsPackageClassAvailable` probe as on native — but the compile-time define is
  deterministic, costs nothing, and finally gives `HP2_HAS_NOPENALDRV` a reader. Decide once; the define is preferred.)
- [ ] **`InitEngine`: native `NormalizeSubsystem`** (`:528-549`): keep; lists: viewport `{NSDLDrv.NSDLClient}`, audio `{NOpenALDrv.NOpenALAudioSubsystem, SoundDrv.NullAudioSubsystem}`, net `{IpDrv.TcpNetDriver}`. HP2's retail ini values `WinDrv.WindowsClient`, `ALAudio.ALAudioSubsystem` are never built → normalised. ⚠ On a native **shared** (non-static) build the probe is `StaticLoadClass(…, LOAD_NoWarn)` of `ALAudio.ALAudioSubsystem`: it looks for `ALAudio.u`/`ALAudio.so`; `Assets/System/ALAudio.dll` exists but is Win32 → the Unix loader ignores `.dll` (`appFindPackageFile` rejects `DLLEXT`) → NULL → normalised. OK. Verify once on the native run (log line `ufront 2.34: [Engine.Engine] AudioDevice=ALAudio.ALAudioSubsystem is not in this build -> …`).
- [ ] **`InitEngine`: FirstRun** — replace the template's unconditional stamp with HP2's read-then-stamp (no dialogs):
  ```cpp
  INT FirstRun=0;
  GConfig->GetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
  if( FirstRun<ENGINE_VERSION )
  {
      debugf( NAME_Init, TEXT("First run of this build (FirstRun=%i -> %i)"), FirstRun, ENGINE_VERSION );
      FirstRun = ENGINE_VERSION;   // 433
  }
  GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
  // [FirstRun] Reconfig / ForceSoftware are D3D-detection knobs HP2 reads and never uses (UnEngineWin.h:1679-1684): ignore.
  ```
  Optional (retail fidelity, 2 lines): `RunCount` increment (`Launch.cpp:155-158`). Nothing reads it.
- [ ] **`InitEngine`: `ServerActors.Empty()`**: move **out of** `#ifdef __EMSCRIPTEN__` — HP2's `Default.ini:69-74` lists `IpDrv.UdpBeacon`, `IpServer.*`, `UWeb.WebServer`; IpServer/UWeb do not exist anywhere in HP2, so a native listen server would fail the same way. Also delete those lines from the web `Default.ini`.
- [ ] **`InitEngine`: `LoadTime`** → `FTime LoadTime = appSeconds(); … appSeconds()-LoadTime` (float).
- [ ] **`MainLoop`**: FTime rewrite (§1.8 / §2.2.3); keep the per-second `CurrentTickRate` average, the `FrameRateCap` default 120, the Asyncify yield and the pacing ring (store `Start` as `FLOAT` seconds since the first frame).
- [ ] **`ue_exec_command` / pacing dump**: `appSeconds()` in `Printf("%f")` → `appSeconds().GetFloat()`; `DOUBLE Start` → `FLOAT`.
- [ ] **`CleanUpOnExit`**: add HP2's `if( Engine && Engine->Audio ) Engine->Audio->SetViewport( NULL );` **before** `appPreExit()` (Launch.cpp:267-268). Pass the engine in (signature `CleanUpOnExit( INT ErrorLevel, UEngine* Engine )`; the `catch(...)` path passes `NULL` — do not touch audio after a crash).
- [ ] **`HandleError`** (native): keep `SDL_ShowSimpleMessageBox`, but skip it when `ParseParam(appCmdLine(),TEXT("NOMSGBOX"))` or `getenv("SDL_VIDEODRIVER")` is `dummy`/`offscreen` — otherwise a crash under Xvfb/CI blocks forever on an invisible dialog. (Template improvement; flag it upstream only if the owner wants it.)
- [ ] **Web `Default.ini`/`DefUser.ini`** (`Config/System/web/`, Phase D, owned by the ini analyst): must exist **before the first wasm link** — `Source/Game/CMakeLists.txt` puts both on `LINK_DEPENDS` (template `:230-232`), so a missing file is a "No rule to make target" link failure, not a silent skip.
- [ ] **Do not add**: splash code or sleeps, SafeDisc, `MEMSTAT`, `Running.ini` creation, `UseD3DDriver`, `CreateDetected`, the CD check, `FExecHook`, `consolecommand=`/`testrendev=`. Each has a stated reason in §2.1-§2.2.

#### 2.6.3 Where the HP2 boot steps went (traceability table)

| Retail HP2 boot step | New home | Headless / web equivalent |
|----------------------|----------|---------------------------|
| SafeDisc check | — | dropped |
| instance forwarding (`WM_COPYDATA`) | — | `ue_exec_command` (web) |
| `appInit("Game")` → `Game.ini`/`User.ini` in `My Documents\…` | Core `appInit` + `appUserDir()==""` | `System/Game.ini`, `System/User.ini` (web: persisted by the frontend's ini layer) |
| `RunCount++` | launcher (optional) | same |
| EA splash 1.5 s, WB legal 1.5 s | — | none (frontend may show an HTML splash; never engine sleeps) |
| WLog window / `-LOG` | GLog→stdout (Core) + `-LOG` | console / `Module.print` |
| `WFrontEnd` main menu → relaunch with `PrivetDr.unr -SAVESLOT=N` / `-LOAD=0 -SAVESLOT=N` | **frontend launcher page** | new game / continue per slot → argv (§2.5) |
| `splashint.bmp` load splash | — | frontend's loading overlay |
| `Running.ini` semaphore | — | dropped |
| FirstRun stamp (433) | launcher `InitEngine` | same |
| CD check | — | dropped (no `CdPath`) |
| `Engine->Init()` | launcher | same + web driver set + render selection |
| `-EXEC=` | launcher | same |
| message pump + `FTime` main loop | launcher `MainLoop` | Asyncify yield + FrameRateCap |
| `Audio->SetViewport(NULL)` on exit | launcher `CleanUpOnExit` | same |
| `exit` → process ends | Core `appRequestExit` | web: navigate back to the launcher page |

#### 2.6.4 Acceptance for the launcher (Phase B / D)

Native (`cd run/System && ./Game -SOFTWARERENDERER -log`, and again as `/abs/path/run/System/Game -SOFTWARERENDERER`):
1. `Game.log` appears in `System/` in **both** invocations (E2), and `Game.ini`/`User.ini` are created there, not under `$HOME` (E3).
2. Log lines: `ufront 2.9: -SOFTWARERENDERER selects render device SoftDrv.SoftwareRenderDevice`; `ufront 2.34: … AudioDevice=ALAudio.ALAudioSubsystem is not in this build -> …`; `ViewportManager=WinDrv.WindowsClient … -> NSDLDrv.NSDLClient`; `First run of this build (FirstRun=0 -> 433)` on the first boot only; `ufront: client frame rate cap 120 fps`; `Bringing Level Startup.MyLevel up for play`.
3. `[FirstRun] FirstRun=433` in `Game.ini` after boot; `Save/` created with mode `rwx` (not `d--------x`, E4).
4. `-SAVESLOT=2 PrivetDr.unr` → `Save Slot Path: ../Save/Slot2` in the log (**prerequisite, Core:** `UnMisc.cpp:2030` is `debugf( TEXT("Save Slot Path: %s"), GSys->SaveSlotPath );` — an `FString` *object* through varargs. On gcc it prints garbage. On emcc it is **worse than a compile error**: `Source/CMakeLists.txt:81` (copied from the template) downgrades clang's default-error `-Wnon-pod-varargs` with `-Wno-error=non-pod-varargs -Wno-non-pod-varargs`, and clang then compiles the call into a **trap** ("call will abort at runtime") — this line runs on **every** boot (`SaveSlotPath` is never empty), so the web build would abort inside `appInit`. Fix to `*GSys->SaveSlotPath`; then temporarily build Core/Engine once with `-Wnon-pod-varargs` re-enabled to list every other such site on a boot path), `Save/Slot2/cache/` exists; after the first autosave `Save/Slot2/Save0.usa` exists.
5. `CPUSPEED=3000 -LOAD=0 -SAVESLOT=2` (the web "Continue" shape) → loads `Startup.unr`, then `Loading game... filename: ../Save/Slot2/Save0.usa`; **no** `Failed to load 'CPUSPEED=3000'`.
6. `-ini:System:[Engine.Engine]:Language=int` → `Ini override: [Engine.Engine] Language=int (System)` appears **after** the Language forcing (i.e. it sticks).
7. In-game Esc → Quit → Yes: `Closing by request`, `appRequestExit(0)`, clean `Exit` in the log, exit code 0.

Web: the same log lines in the browser console; Quit returns to the launcher page (`[SHELL] Engine exit requested, returning to launcher`); a key binding changed in-game survives a reload (A2).

## 3. UCC — template vs HP2, and `ucc make` for `IpDrv.u`

### 3.1 Files

| File | Lines | What it is |
|------|-------|------------|
| `/home/ion/epic-sources/ut99-v400/UCC/Src/UCC.cpp` | 274 | Epic stock (Win32 + Loki Linux) |
| `engine-ut99/Source/UCC/Src/UCC.cpp` | 277 | stock + **two** ufront 2.12 edits (below) |
| `engine-ut99/Source/UCC/CMakeLists.txt` | 51 | ufront 2.12 Part B + 2.79 static link |
| `HarryPotter2/Source/UCC/Src/UCC.cpp` | 370 | **HP2/PS2-branch** UCC: stock + the Brandon Reinhart `__STATIC_LINK` native-lookup table, `__PSX2_EE__` file manager, `NullNetDriver`, `appInit("HP")` |
| `HarryPotter2/Source/UCC/Src/{UCC.dsp,makefile}` | — | retail MSVC project (`/D UNICODE /D WIN32 /D ThisPackage=UCC`, links only `Core.lib`, output `System\UCC.exe`) and the Loki/PS2 makefile (`TARGETTYPE=psx2` → `NullNetDriver.a Engine.a Core.a`). Reference only. |
| `HarryPotter2/Source/UCC/CMakeLists.txt` | 51 | **already the template's**, minus `UWeb` in `UCC_LINK_PACKAGES` (HP2 has no UWeb) |
| `HarryPotter2/Assets/System/UCC.exe` | 122,880 B (2002-09-06) | retail binary |

### 3.2 Template vs epic (`diff --strip-trailing-cr -w`): the complete delta

| Template line | Change | Why |
|---------------|--------|-----|
| `UCC.cpp:85-89` | `#if !_MSC_VER && defined(UE_LEGACY_CONTEXT)` around `__Context::StaticInit();` (stock: `#if !_MSC_VER`) | the modern Core has no `__Context` (the setjmp/signal context of the Loki port) |
| `UCC.cpp:185-186` | `INT i;` hoisted out of the `for` | stock relied on MSVC 6 for-scope (`i` is used after the loop at `:194`) |
| `CMakeLists.txt` | `add_executable`, `OUTPUT_NAME ucc-bin`; `BUILD_STATIC` → `-Wl,--whole-archive Core Engine Render IpDrv UWeb Fire [Editor] -Wl,--no-whole-archive`; else `Engine Core`; defines `UPACKAGE_NAME=UCC __LINUX__` | 2.79: in a static build a package is only "loadable" if its objects are in the binary (`appGetDllHandle` answers from the `STATIC_EXPORT` registry, `Core/Inc/UnStaticExports.h`); ordinary archive linking kept only what `UCC.cpp` referenced → "Commandlet server not found". `__LINUX__` selects `FFileManagerLinux` + `FMallocAnsi` in `UCC.cpp:47-51`. |

Everything else is stock. In particular:
- **`GIsClient`**: *not* changed by ufront. `UCC.cpp:131` sets `GIsClient = GIsServer = GIsEditor = GIsScriptable = 1`
  (needed so `help` can load every commandlet class), then **per commandlet** `GIsClient = Default->IsClient;
  GIsServer = Default->IsServer; GIsEditor = Default->IsEditor; GLazyLoad = Default->LazyLoad;` (`:235-238`). The
  CMake comment "UCC.cpp sets GIsClient off" means exactly this (e.g. `ServerCommandlet` has `IsClient=0`). The
  static link simply omits NSDLDrv/renderers/audio because no headless commandlet opens them.
- **`GIsGuarded = 1`** (`:97`) — UCC keeps stock guarding: it is **native-only** (`if(NOT EMSCRIPTEN)`,
  template `Source/CMakeLists.txt:488`; HP2 `Source/CMakeLists.txt:289`), built with exceptions on, and the script
  compiler *needs* them (`FScriptCompiler` uses `appThrowf` + `try/catch`, HP2 `Editor/Src/UnScrCom.cpp:5641-5700`).
  Do **not** carry the launcher's `GIsGuarded=0` over to UCC.
- **Command-line / dispatch** (`:104-123`, `:128-257`): `argv[1]` is the commandlet token, `argv[2..]` joined with
  spaces is `appCmdLine()` (unbounded `appStrcat`, same 1024-char hazard as the launcher §1.2 #1). Lookup order:
  `StaticLoadClass(UCommandlet, NULL, Token)` → `Token+"Commandlet"` → scan of the `.int` registry
  (`GetRegistryObjects(List, UClass, UCommandlet)`, i.e. `[Public] Object=(Name=Pkg.XCommandlet,Class=Class,
  MetaClass=Core.Commandlet)` lines) comparing the short name. `Token=="Make"` (case-insensitive `FString==`) adds
  `LOAD_DisallowFiles` so `Editor.MakeCommandlet` is taken from the **in-memory native class**, never from a stale
  `Editor.u`. `LogToStdout` redirects `GLog` to the console for the run.
- **`appStrcpy( GModule, argv[0] )`** (`:101`): same `GModule[32]` overflow as the launcher (§1.2 #2/E2). The log is
  `appBaseDir()+argv[0]+".log"` → `./ucc-bin` gives `System/./ucc-bin.log`.

### 3.3 HP2 `UCC.cpp` vs template — what changes, line by line

`diff --strip-trailing-cr -w engine-ut99/Source/UCC/Src/UCC.cpp HarryPotter2/Source/UCC/Src/UCC.cpp`:

| HP2 lines | HP2 code | Status in the port | ACTION |
|-----------|----------|--------------------|--------|
| 18 | `#include "Engine.h"` (template: `Core.h`) | only needed for the `__STATIC_LINK` block | use `Core.h` (template) |
| 20-46 | `#if __STATIC_LINK` → `UnEngineNative.h`, `UnCon.h`, `UnRender.h`, `UnNet.h`, `NullNetDriver.h`; `#ifdef __LINUX_X86__` commented-out Fire/IpDrv/UWeb includes | **dead**: the port defines `UNREAL_STATIC` (HP2 `Source/CMakeLists.txt:92`), never `__STATIC_LINK`; `NullNetDriver.h` does not exist in this drop | delete |
| 48 | `INT GFilesOpen, GFilesOpened;` | referenced nowhere else in `Source/` (PS2 file manager bookkeeping) | delete |
| 76-77 | `#if WIN32 … FMallocWindows Malloc;` (template: `FMallocAnsi`) | MinGW: `FMallocWindows` is the MSVC pool allocator | take template (`FMallocAnsi` everywhere) |
| 78-82 | `#elif __PSX2_EE__ … FFileManagerPSX2` | PS2 | delete |
| 83 | `#elif __LINUX_X86__` (template: `#elif __LINUX__`) | **compile break**: CMake defines `__LINUX__` (`UCC/CMakeLists.txt` last line), not `__LINUX_X86__` → falls to `FFileManagerAnsi` and **no `Malloc` object** → `appInit(…, &Malloc, …)` fails to compile. (HP2's *current* `Core/Inc/UnGnuG.h:10-32` even `#error`s without `__LINUX_X86__`/`__PSX2_EE__`; the Core port replaces that header with the template's.) | take template |
| 121-159 | `#if __STATIC_LINK` fill `GNativeLookupFuncs[0..15]` with `FindCoreUObjectNative`, `FindEngineAActorNative`, … (IpDrv/UWeb entries commented out) | dead (see 20-46). This is HP2's PS2-era static-link mechanism (`Core/Inc/UnCoreNative.h:12-45`, `Core/Src/UnCoreNative.cpp:272`, `Engine/Inc/UnEngineNative.h:28`); the port uses the template's `STATIC_EXPORT` registry + `--whole-archive` instead | delete. (Core/Engine analysts: decide whether `UnCoreNative.*`/`UnEngineNative.*` are compiled at all under `UNREAL_STATIC`; if they are, they must not clash with the template's registration.) |
| 161-163 | `#if !_MSC_VER  __Context::StaticInit();` | compile break against the template Core (no `__Context`); and if HP2's `UnUnix.cpp:767ff` `__Context` survives the Core port, it installs `SA_ONESHOT` signal handlers that `longjmp` — incompatible with the exceptions-based guard model | take template guard (`&& defined(UE_LEGACY_CONTEXT)`) |
| 191 | `for( INT i=2; …)` join | — | take template; **bound it** (`appStrncat`, as the launcher) |
| 200 | **`appInit( TEXT("HP"), … )`** | retail value, **verified in the binary**: the UTF-16 string `"HP"` sits right after `"int"` at offset 103,660 of `Assets/System/UCC.exe` (the `SetLanguage(TEXT("int"))` literal), and there is no `"Game"` literal. So retail `ucc` read/wrote **`HP.ini`** (in `appUserDir()`), a *separate* copy of `Default.ini`, not `Game.ini`. | **Decision needed (Q-UCC1).** Recommended: **keep `"HP"`** — it isolates the tool's config (`EditPackages` edits for `make`, §3.5) from the game's `Game.ini`, which HP2's `appInit` flushes on every start (§2.2.4 A9) and which the web frontend persists. Template parity would be `"Game"`. Either way `INI=<file>` overrides. |
| 203-218 | `AUTO_INITIALIZE_REGISTRANTS_ENGINE; …_NULLNETDRIVER;` + commented PS2 meta-archive readers | dead | delete |
| 279 | `for( INT i=0; …)` then `i` used at 287 | gcc: `i` out of scope → compile error | take template hoist |

**Net ACTION:** replace `HarryPotter2/Source/UCC/Src/UCC.cpp` with the template's file and apply three edits:
(1) `appInit( TEXT("HP"), … )` (or `"Game"`, per Q-UCC1); (2) `GModule` = basename of `argv[0]` via `appStrncpy`
(fixes the overflow and gives `ucc-bin.log` for absolute invocations); (3) bounded argv join. Delete `UCC.dsp` and
`makefile` from the build (they are not referenced by CMake; keep on disk as reference, like `Source/Launch`).
`CMakeLists.txt`: keep as is (already template-with-UWeb-removed). Keep `__LINUX__` in its defines.

**MinGW:** `add_subdirectory(UCC)` is gated only on `NOT EMSCRIPTEN`, so the `windows` preset also builds `UCC`; on
MinGW `UCC.cpp:9-10,42-46,107-115` takes the `WIN32` branch (`windows.h`, `FFileManagerWindows`, `GetCommandLineA`).
The template's BUILD.md:155 lists `ucc.exe` as "out of scope" on Windows, i.e. it is **unverified** there. ACTION:
either verify the MinGW build of `ucc-bin.exe` once, or gate with `if(NOT EMSCRIPTEN AND NOT MINGW)` in HP2's
`Source/CMakeLists.txt:289` (recommended until someone needs it — native Linux `ucc` is enough to produce `IpDrv.u`,
which is platform-independent).

### 3.4 Commandlets available to an HP2 `ucc-bin` (static build)

Registry (what `ucc help` lists) = `[Public] Object=` lines of the `.int` files in `System/`:
`Core.int:7` HelloWorld; `Engine.int:3-4` Server, CheckSum; `Editor.int:2-12` Master, Make, Conform, BatchExport,
MergeDXT, PackageFlag, DataRip, PS2Convert, UpdateUMod, ChecksumPackage, Pkg. C++ classes present in `Source/`
(`grep IMPLEMENT_CLASS(U*Commandlet)`):

| Commandlet | C++ | In HP2 `Editor/CMakeLists.txt`? | Use for the port |
|------------|-----|------------------------------|------------------|
| `Core.HelloWorldCommandlet` | script only (Core.u) | — | smoke test: `ucc-bin Core.HelloWorldCommandlet` |
| **`Core.SimpleCommandlet`** | script only — **HP2-only**, not in epic v400 `Core/Classes` (which has only `HelloWorldCommandlet`) and **not registered in `Core.int`** | — | VM smoke test (§3.6); must be called by **full name** `ucc-bin Core.SimpleCommandlet` (the short name fails: no registry entry, and `StaticLoadClass("Simple")` resolves "Simple" as a package) |
| `Engine.ServerCommandlet` | `Engine/Src/UnEngine.cpp:395` | — | dedicated server; HP2 has no MP game mode → not a goal |
| `Engine.CheckSumCommandlet` | `Engine/Src/AStatLog.cpp:436` | — | ngStats checksum; irrelevant |
| `Editor.MakeCommandlet` | `Editor/Src/UMakeCommandlet.cpp:239` | yes | **`IpDrv.u` build** (§3.5) |
| `Editor.BatchExportCommandlet` | `UBatchExportCommandlet.cpp:66` | yes | **recommended Phase B pre-check**: `ucc-bin batchexport HGame.u class uc /tmp/x` loads HGame.u + all its imports headlessly (no renderer, no SDL) — a faster way to find `Can't find file for package` / `[layout]` / `Can't bind to native` problems than booting the client |
| `Editor.ConformCommandlet`, `PackageFlag`, `DataRip`, `CheckUnicode` | `UConformCommandlet.cpp:55,125,222,299` | yes | tooling |
| `Editor.MasterCommandlet`, `UpdateUMod`, `ChecksumPackage` | `UMasterCommandlet.cpp:459,600,636` | yes | UMOD tooling |
| `Editor.MergeDXTCommandlet`, `Editor.PkgCommandlet` | yes | yes | tooling |
| `Editor.PS2ConvertCommandlet` | `UPS2ConvertCommandlet.cpp` | **no** (stripped) | `help` silently skips it (`LOAD_NoWarn|LOAD_Quiet` → NULL) — expected |
| `IpDrv.MasterServerCommandlet`, `UpdateServerCommandlet` | `IpDrv/Src/*.cpp` | IpDrv linked | not in any `.int` (no `IpDrv.int` in HP2) → only by full name; irrelevant |

### 3.5 Can `ucc make` build `IpDrv.u` from the v400 `IpDrv/Classes` against HP2's `Engine.u`?

**Inputs.** `HarryPotter2/Source/IpDrv/Classes/*.uc` are byte-identical (modulo CR/whitespace) to both
`engine-ut99/Source/IpDrv/Classes` and epic `ut99-v400/IpDrv/Classes` (5 classes, 393 lines + `IpDrv.upkg`:
`AllowDownload=False ClientOptional=False ServerSideOnly=True`); `Source/IpDrv/Src` is identical to the template's.
HP2 ships **no** `IpDrv.u` (`ls Assets/System` → no `IpDrv.*`). The template never needed `make` (retail UT99
ships `IpDrv.u`), so this is new ground for the family.

**Script API check — every external symbol the 5 classes use, against HP2's `Engine.u`/`Core.u` sources:**

| Used by IpDrv | HP2 definition | OK? |
|---------------|----------------|-----|
| `class InternetInfo extends Info native transient` + `GetBeaconAddress(int)`, `GetBeaconText(int)` | `Engine/Classes/InternetInfo.uc` — **identical** to v400 (`diff` → same); present with script text in `Assets/System/Engine.u`; C++ `AInternetInfo` at `Engine/Inc/EngineClasses.h:2592` | yes |
| `Info`, `Actor.bAlwaysTick` (set in TcpLink/UdpLink defaults) | `Engine/Classes/Actor.uc:39` `var Const bool bAlwaysTick;` (v400 `:220`, same decl) | yes |
| `RemoteRole`, `ROLE_None` | `Actor.uc:76,83` | yes |
| `SetTimer`, `event Timer`, `event BeginPlay`, `event Destroyed` | `Actor.uc:1268` (`native(280)`), `:1175`, `:1366`, `:1158` | yes |
| `Level.TimeSeconds`, `Level.GetAddressURL()`, `Level.Game` | `LevelInfo.uc:29`, `:164` (native, `ALevelInfo::execGetAddressURL` `Engine/Src/UnScript.cpp:431`), `:116` | yes |
| `Level.Game.GetBeaconText()` | `GameInfo.uc:635` `event string GetBeaconText()` | yes |
| `Log`, `Len`, `Left`, `Mid`, `InStr`, `$`, `@`, `~=`, `int()` | `Core/Classes/Object.uc:339,318,321,320,319,307,308,315` — HP2's native table equals v400's **plus** `native(1033) RandRange` (diff of all `native(N)` decls) | yes |
| `var const Array<byte> SendFIFO` (TcpLink) | compiler: `Editor/Src/UnScrCom.cpp:3354-3361` parses `Array<…>` (`NAME_Array`, `Core/Inc/UnNames.h:182`); C++ side `TArray<BYTE>` in `IpDrvClasses.h` | yes |
| inline `var struct BeaconInfo {…} Beacons[32]`, struct `==` (`Beacons[i].Addr==Addr`), `arraycount`, `optional`/`coerce`, `byte B[255]` params | stock v400 language features, same compiler lineage | yes (low risk) |
| `defaultproperties { BroadcastAddr=-1; }` (trailing `;`) | v400 accepted it; HP2's `ImportProperties` is the same code lineage | probably; verify in the make log |
| Name collisions: IpDrv's functions/vars/enums (`Open`, `Close`, `Listen`, `Resolve`, `Validate`, `Accepted`, `Opened`, `Port`, `STATE_*`, `MODE_*`, `IpAddr`, …) vs HP2's `Object`/`Actor`/`Info` | `grep -w` over `Core/Classes`, `Engine/Classes`: the hits are in **sibling** classes only (Mover `Open/Close`, GameInfo `Listen/Port`, Inventory `Validate/Accepted`) — nothing in the `Object→Actor→Info→InternetInfo` chain (only a comment in `Actor.uc:1680`) | yes |

**Conclusion: the script side compiles as is.** The v400 IpDrv classes use nothing HP2's Engine lacks. The risks are
in the *tool*, not the language:

| # | Risk | Evidence | Mitigation (ACTION) |
|---|------|----------|---------------------|
| M1 | `make` needs the Editor + script compiler ported and linked into `ucc-bin` | `UCC/CMakeLists.txt` adds `Editor` only `if(BUILD_EDITOR)` (default ON, HP2 `Source/CMakeLists.txt:35`); `LOAD_DisallowFiles` means the native `Editor.MakeCommandlet` must be in the binary | Phase B: build `ucc-bin` with `BUILD_EDITOR=ON` |
| M2 | `UEditorEngine::InitEditor` → `VERIFY_CLASS_OFFSET(A,Actor,Owner)` / `(A,PlayerPawn,Player)` (`Editor/Src/UnEditor.cpp:52-89`) | an EngineClasses.h layout mismatch aborts `make` before anything compiles | same gate as Phase B's zero-`[layout]` rule; run `make` only after `Game` boots cleanly (PLAN step 6 already orders it so) |
| M3 | **`FFileManagerLinux::FindFiles` ignores the `Files`/`Directories` flags** (template `Core/Inc/FFileManagerLinux.h:304-413`: every matching `dirent` is returned, incl. `.`, `..` and plain files). HP2's make is the incremental 432+ version and **`Editor/Inc/Editor.h:14` defines `USE_SUB_DIRECTORIES`**, so `UMakeCommandlet.cpp:137-171` enumerates `../IpDrv/Classes\*` as *directories* → gets `.`, `..`, every `.uc` and `IpDrv.upkg`; the `.` entry imports all 5 classes via `../IpDrv/Classes/./X.uc`, and the extra `""` entry imports them a **second time** (`!PkgOrig` is true for a new package, `:161`) | re-import of the same class is probably benign, but untested | Core analyst: make `FindFiles` honour the flags (skip `.`/`..`; `stat()`/`d_type` → file vs dir), as Win32 `FindFirstFile` did. Cheap, and HGame's sub-folder layout (`HGame/Classes/{Menu,Spells,…}`) needs it if anyone ever re-makes HGame. |
| M4 | **Header export** — `bHeader` defaults on (`UMakeCommandlet.cpp:50,208-214`): after compiling a native package it writes `../IpDrv/Inc/IpDrvClasses.h` and **`appErrorf("Failed to export")` if that directory does not exist** | the template's `IpDrvClasses.h` is hand-maintained (differs from epic's) — an auto-generated one would overwrite it if the run tree links `Source/IpDrv` | always pass **`-NOHEADER`** |
| M5 | **Collateral rebuild of retail packages.** For each already-existing EditPackage the currency check (`:85-130`) re-imports a class when `GetGlobalTime(../Pkg/Classes/X.uc) > GetGlobalTime(Pkg.u)`, and `CheckScripts` (`UnScrCom.cpp:5939-5990`) can mark classes dirty via dependency CRCs; a dirty package is **re-saved over `System/<Pkg>.u`** (`:228`). The shipped `.u` files carry their script text (Engine.u: 127 `class … extends` headers = all 127 source files; HGame.u 713), so a recompile is possible — and would replace EA's binaries with ours. | With the template's `GetGlobalTime(){return 0;}` (kept for §2.2.4 A2) and no `../<Pkg>/Classes` directories in the run tree, `FileTime<=0` → the class is only flagged `RF_TagGarbage` (a GC scratch flag) and skipped; nothing re-imports. | Run `make` in a **scratch copy** of `run/System` whose parent has **only** `IpDrv/Classes` (symlink to `Source/IpDrv/Classes`); never pass `-ALL`; after the run, `grep -E "Updating\.\.\.|Building\.\.\." ucc-bin.log` must name **only** `IpDrv`; copy out only `IpDrv.u`. |
| M6 | `EditPackages` must contain `IpDrv` (after `Engine`, before anything that could depend on it — nothing in HP2 does) | HP2 `Default.ini:327-337` lists Core, Engine, Editor, UWindow, Fire, UnrealShare, HPSounds, HPModels, HPParticle, HProps, HGame — no IpDrv | Do **not** use `-ini:System:[Editor.EditorEngine]:EditPackages=IpDrv`: `SetString` on a multi-valued key replaces the *first* `EditPackages=` (Core) — breaks the list. Use a dedicated ini: with Q-UCC1 = `"HP"`, the tool's own `HP.ini` (seeded from `Default.ini` + one inserted `EditPackages=IpDrv` line after `EditPackages=Engine`), or `ucc-bin make -NOHEADER INI=MakeIpDrv.ini` (the `INI=` token lands in `appCmdLine()`, which `appInit` parses). A `tools/dev/make-ipdrv.sh` should generate it. |
| M7 | UCC's `appInit` side effects (HP2 §2.2.4): creates `SavePath`/`Slot` dirs, flushes the ini; with the unfixed A5 bug a literal `System/..\Save` directory appears | cosmetic in a scratch tree | fix A5 in Core first, or ignore in the scratch tree |
| M8 | The compiled package's imports reference `Engine`/`Core` by the **shipped** GUIDs | fine as long as the same retail `Engine.u`/`Core.u` are used at runtime (they are — never rebuilt, M5) | — |

**Is `IpDrv.u` actually needed?** For the **net driver: no**. `StaticLoadClass("IpDrv.TcpNetDriver")` → `UPackage`
ctor → `BindPackage` (`Core/Src/UnObj.cpp:1574-1593`) sets `DllHandle` from the in-binary export registry; then
`GetPackageLinker` finds no `IpDrv.u` but returns NULL instead of throwing because of `LOAD_AllowDll && DllHandle`
(`UnObj.cpp:2199-2203` region), and `StaticFindObject` returns the native `UTcpNetDriver`. So TcpNetDriver (a
C++-only class, no `.uc`) works without `IpDrv.u` — **PLAN step 6's fallback is sound**. What *does* need `IpDrv.u`:
the script classes `UdpBeacon`, `ClientBeaconReceiver`, `TcpLink`, `UdpLink`, `InternetLink` — i.e.
`ServerActors=IpDrv.UdpBeacon` (`Default.ini:69`, removed by §2.6's all-platform `ServerActors.Empty()`) and any LAN
server browser (HP2 has none). Also note `Default.ini:173` `DownloadManagers=IpDrv.HTTPDownload`: there is **no
`HTTPDownload.cpp` in `Source/IpDrv/Src`** (template identical) → a P2P client that needs to download a package will
try to load a class that does not exist; `UChannelDownload` is the fallback in the 432+ architecture — IpDrv analyst
to confirm; the launcher is unaffected.
**Recommendation:** ship **without** `IpDrv.u` (nothing in HP2 needs the script classes); keep `ucc make` for
`IpDrv.u` as an optional Phase F nicety, done with M3-M6 in place. If it is ever built, add `IpDrv.u` + an `IpDrv.int`
(empty registry) to the run tree and the web manifest together.

### 3.6 `Core/Classes/SimpleCommandlet.uc` (HP2-only)

```unrealscript
class SimpleCommandlet expands Commandlet;
function int TestFunction() { return 666; }
function int Main( string Parms ) { … log(...) … }          // no 'return' — returns 0
defaultproperties { HelpCmd=Simple  HelpOneLiner=Simple test commandlet  HelpUsage=Simple (no parameters)
                    HelpWebLink=  LogToStdout=true }
```
- Origin: not in epic v400 (`ut99-v400/Core/Classes` has `HelloWorldCommandlet.uc` only); it arrived with the
  PS2/Loki static-link branch HP2's UCC also comes from (a UnrealScript VM bring-up test). It **is compiled into the
  shipped `Core.u`** (the name occurs in `Assets/System/Core.u`), but it has **no `Core.int` registry line**, so it
  is invisible to `ucc help` and must be invoked as `ucc-bin Core.SimpleCommandlet`.
- It exercises only Core natives: a script function call, float→int conversion, `Min`, `Left`, `Mid`+`InStr`, `$`/`@`
  concatenation, `Log` — i.e. it runs with **no Engine** state, which makes it the ideal **first UCC acceptance test**
  (proves Core + the script VM + `FOutputDeviceFile`/`LogToStdout` on the new platform layer before anything else).
- Expected output (UE1 float→string is `%.2f`):
  ```
  Simple commandlet says hi.
  Testing function calling.
  Function call returned 666
  Testing cast to int.
  Temp is cast from 3.00 to 3
  Testing min()
  Temp is min(32, 666): 32
  3 is a woo
  otherstring: alker
  ```
  ACTION: add this as the Phase B UCC smoke test (a 9-line golden file); a mismatch in `3.00` or `alker` points at
  `appSprintf`/`FString` porting bugs (e.g. the `FString`-through-varargs class of bug noted in §2.6.4 item 4).

### 3.7 UCC acceptance (Phase B)

1. `./ucc-bin` → banner + `Use "ucc help" for help`; `./ucc-bin help` lists HelloWorld, Server, CheckSum, Master,
   Make, Conform, BatchExport, MergeDXT, PackageFlag, DataRip, UpdateUMod, ChecksumPackage, Pkg (no PS2Convert).
2. `./ucc-bin Core.SimpleCommandlet` → the 9 golden lines (§3.6); `./ucc-bin Core.HelloWorldCommandlet` → its lines.
3. `./ucc-bin batchexport HGame.u class uc <scratch>` → loads HGame and all imports with **no** `[layout]`, no
   `Can't find file for package`; exports 700+ `.uc` files.
4. (optional) `make -NOHEADER` in the scratch tree (§3.5 M5/M6) → log shows `IpDrv - Release`, `Building...`,
   `Success - 0 error(s)`, `Saving...` for IpDrv **only**; the resulting `IpDrv.u` loads in `Game` (`obj list
   class=UdpBeacon` finds it).
5. `/abs/path/run/System/ucc-bin help` (absolute argv[0]) → no crash, `ucc-bin.log` written (GModule fix).

## 4. Open questions (with how to verify)

| # | Question | How to verify / who decides |
|---|----------|-----------------------------|
| Q1 | Native `appBaseDir()`: adopt the template's `SDL_GetBasePath()` version in Core, or keep HP2's `getcwd()` and `chdir` in the launcher? (§1.11, §2.6.2) | Core analyst; either satisfies A7. Test: launch `/abs/run/System/Game` from `/`. |
| Q2 | `appUserDir()` on Unix returns `""` (proposed E3) — confirm the frontend's `iniPath`/`userFiles` contract (`System/Game.ini`, `Save/…`) with the ufrontend-target analyst. | `targets/hp2/target.json` review |
| Q3 | Does NSDLDrv tolerate `SDL_VIDEODRIVER=dummy` with `-NULLRENDERER`/`-SOFTWARERENDERER` (headless CI without Xvfb)? | one native run each; otherwise Xvfb (§1.10) |
| Q4 | Is `HandleError`'s `SDL_ShowSimpleMessageBox` suppression (`-NOMSGBOX` / dummy driver) wanted, and should it go upstream to the template? | owner |
| Q5 | `ue_exec_command` runs on the browser main thread (JS `ccall`) while the engine runs on the `PROXY_TO_PTHREAD` pthread — how does the template avoid the data race? | read `ufrontend/packages/frontend/src/shell/*` for how `_ue_exec_command` is called (proxied via a worker message?) — web-layer analyst |
| Q6 | `-MEMSTAT`/`TMallocLog`: port as an optional allocator wrapper over `FMallocAnsi`? Only useful with a Unix `MEMSTAT` exec answer (`Core/Src/UnUnix.cpp:83-88` says "not available"). | owner; default no |
| Q-UCC1 | `ucc` ini name: retail `"HP"` (→ `HP.ini`, isolated from the game) vs template-style `"Game"` (one ini). Recommendation `"HP"` (§3.3). | owner |
| Q-UCC2 | Is `ucc-bin.exe` on MinGW wanted? If not, gate `add_subdirectory(UCC)` with `NOT MINGW` (§3.3). | owner; verify one MinGW build if yes |
| Q-UCC3 | Does the double import of each IpDrv class (M3) matter if `FindFiles` is left unfixed? | run `make` once with `-SHOWDEP` and look for duplicate "Parsing"/"Compiling" lines or class-redefinition errors |
| Q-UCC4 | `DownloadManagers=IpDrv.HTTPDownload` names a class that does not exist in this source tree (no `HTTPDownload.cpp`) — what does a P2P client do when it must download a package? | IpDrv / WebRTC analyst; P2P test with a package missing on the client |
| Q7 | ~~Are HP2's `.int` files UTF-16 (retail was a `/D UNICODE` build)?~~ **Resolved:** `file Assets/System/*.int Assets/SystemLocalized/*` → 0 of 208+ files are UTF-16 (ASCII / extended-ASCII CRLF). The ANSI Unix `FConfigCacheIni` reads them; the extended-ASCII one (one `.int`, Latin-1 umlauts etc.) needs no conversion in an ANSI build. | done |

## Status: complete

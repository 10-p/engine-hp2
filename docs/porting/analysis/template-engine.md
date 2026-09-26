# template-engine — the ENGINE package portability delta the HP2 port must replicate

**Key:** `template-engine` · **Written:** 2026-09-26 (re-run; supersedes the 2026-09-25 draft) ·
**Subject:** every change `engine-ut99/Source/Engine` makes relative to Epic `ut99-v400/Engine`, its
rationale, and whether/how it applies to HP2 build 433 `Source/Engine`; plus the GCC/Win32 hazards HP2's
Engine carries on its own, the CMake source list, and the `EngineClasses.h` ↔ `Engine.u` layout check.

> Report is written incrementally. Sections marked _(pending)_ are still being researched.

## 0. Executive summary
_(pending)_

## 1. Paths, method, legend

| Symbol | Path |
|--------|------|
| **HP2** | `/home/ion/hp2-ufrontend/HarryPotter2/Source/Engine` (`Inc/` 71 files, `Src/` 60 files, `Classes/` 128 `.uc`, `CMakeLists.txt`) |
| **TPL** | `/home/ion/engines/engine-ut99/Source/Engine` (READ ONLY; base import maximqaxd/ut99dc) |
| **EPIC** | `/home/ion/epic-sources/ut99-v400/Engine` (READ ONLY; CRLF; the common ancestor) |
| **TPL-Core** | `/home/ion/engines/engine-ut99/Source/Core` (the 2.32 layout guard lives here, not in Engine) |
| **Assets** | `/home/ion/hp2-ufrontend/HarryPotter2/Assets/System/{Core.u,Engine.u}` |

**Method.** Every file present in EPIC/TPL/HP2 `Engine/Inc` + `Engine/Src` was diffed three ways with
`diff --strip-trailing-cr -w` (E-T = Epic→template, E-H = Epic→HP2, T-H = template→HP2). Each template hunk
was attributed to its commit with `git -C engine-ut99 log -- Source/Engine/<file>` (read-only git), then
the same site was located in HP2 by grepping for the Epic text. Class layouts were checked by parsing the
shipped `Engine.u`/`Core.u` (package tables + property exports) with a throw-away Python script in the
scratchpad (reproduced in Appendix A so the result is re-runnable).

**Commit origin tags** used below:

| Tag | Commit(s) | Meaning |
|-----|-----------|---------|
| **DC-GCC** | `cbdb054` "first blood" | ut99dc's initial import = Epic v400 + the fixes a modern GCC needs (for-scope hoists, `#endif X`, forward decls, ASM removal, `GCC_PACK`→`GCC_ALIGN` on BYTE-after-bitfield) |
| **DC** | `17b0864` "Dreamcast support", `a5e2533` SWORD BSP, `2673db1` mips, `383f08c` actor hash hack | Dreamcast (`PLATFORM_DREAMCAST` / `PLATFORM_LOW_MEMORY`) work, plus the `Min/Max/Clamp<INT>` explicit template args that sh-elf's `int32_t == long` forced |
| **WEB** | `d5a6bfd` (squash of the 2.6–2.12 web port) | exceptions-off NULL returns, `?connect` URL, SDL mouse capture, `MAX_NODES` restore, `PUSH_HIT` decltype |
| **UF** | `67976cb 52641f8 1fb5f26 144954c 29a71a7 d31fdfd 6525113 e2e724b 63595e5 b40994b 0f620a2 301a9ca 1546db7 1d3dfa5 5674fc7 3ff2ed5 7c31a53 a57d578` | ufront feature/fix commits after the squash |

**Classification legend** (used in every hunk row):

| Tag | Meaning |
|-----|---------|
| **APPLIES-CLEANLY** | HP2's text at the same site is identical (modulo whitespace) to Epic's; the template hunk drops in |
| **NEEDS-ADAPTATION** | same intent, but the surrounding HP2 code differs; port the idea by hand (row says how) |
| **ALREADY-PRESENT** | HP2's 433 source already contains the fix / the equivalent |
| **NOT-APPLICABLE** | Dreamcast / LOW_MEMORY / UT99-content-specific, or not required on the port's three targets |
| **DO-NOT-PORT** | a template change that is a regression or a bug; HP2 must deliberately NOT copy it |

Port targets are **native Linux i386 (gcc -m32)**, **Emscripten wasm32 (clang)**, **MinGW i686 (gcc)**. On
all three `int32_t` is `int`, so every `Min/Max/Clamp<INT>` edit the template carries (tag DC) is
**not needed** unless HP2 adopts a `typedef int32_t INT` and something else differs; they are harmless if
copied, and the compiler will name any site that genuinely needs one.


## 2. Template history — why each Engine change exists (git log)

`git -C /home/ion/engines/engine-ut99 log --oneline -- Source/Engine` has only **24 commits**; the rationale
below is quoted/condensed from their messages. (FrameRateCap and GLog→console, which the brief lists, are
**not Engine changes**: FrameRateCap lives in `Source/UnrealTournament/Src/Launch.cpp:678-697` and reads
`[Engine.Engine] FrameRateCap` from the ini with `GConfig` — it is not a class property; GLog routing is
Core/Launch. They belong to the launcher/core reports.)

| Commit | Title (abridged) | Engine files | Why (from the message) | HP2 relevance |
|--------|------------------|--------------|------------------------|---------------|
| `cbdb054` | ut99dc "first blood" | all | Epic v400 + the modern-GCC fixes: ISO for-scope hoists, `#endif NAMES_ONLY`→`#endif`, `CompareHits` forward decl, `GCC_PACK`→`GCC_ALIGN` on BYTEs after bitfields, UnRender.h ASM removal, `TFieldIterator` decl, `!a==b` precedence, `PUSH_HIT`… | **Required in kind** — HP2 has the same MSVC6-isms, plus more in its 433-only code |
| `17b0864` | Dreamcast support | 21 | DC build; `Min/Max/Clamp<INT>` explicit template args (sh-elf `int32_t` is `long`), `int` vs `INT` signature fixes, LOW_MEMORY GCache, UnTex.h `TEXF_EXT_*` + 1555/565 helpers | **Not needed** on i386/wasm32/MinGW (harmless if copied) |
| `a5e2533` | DC: BSP fields to SWORD | UnObj.h UnModel.cpp UnDynBsp UnTrace UnFPoly UnAudio | shrink `FBspNode`/`FBspSurf` for 16 MB RAM | **NOT-APPLICABLE**, and it carries one all-platform regression (UnTrace `Use[FrontFirst]`, §5) — **DO-NOT-PORT** |
| `2673db1` | DC: don't keep mips in RAM | UnTex.cpp UnGame.cpp UnModel.h | DC VRAM | NOT-APPLICABLE |
| `383f08c` | DC: actor-hash dirty hack | UnActCol.cpp | DC | NOT-APPLICABLE |
| `d5a6bfd` | Web + native port (squash of 74 commits) | UnCamera.h UnModel.h UnGame.cpp | exceptions-off NULL-return sites in `LoadMap`/`Init` (2.6g, 2.12), `?connect` URL host promotion (2.7d), `SetProgress` NULL-Actor guard (2.12), SDL whole-game mouse capture (2.12), stock `MAX_NODES/MAX_POINTS` (2.7c), `PUSH_HIT` `decltype` for clang (2.7d) | **Required** (web) |
| `67976cb` | AutoFOV (2.16) | UnCamera.h/.cpp UnCanvas.cpp UnGame.cpp | aspect-correct Hor+ FOV as a SCOPED override around `UGameEngine::Draw`; `[ufront.AutoFOV]` ini section, never written back (PlayerPawn MouseScale) | **Adapt** — HP2 is third-person; see §5 UnGame/UnCanvas |
| `52641f8` | AutoFOV log only primary view | same | `bLog` flag; viewmodel double-correction fix | with AutoFOV |
| `1fb5f26` | AutoFOVUI (2.16b) | UnCamera.h/.cpp UnGame.cpp | scoped `HudScale` multiply around `eventPostRender`, found by name via `FindField<UFloatProperty>` | **Probably N/A** — HP2's HUD has no `HudScale` (verify, §5) |
| `144954c` | AutoFOV: remove cmd-line switches | UnCamera.cpp | ini is the only home; `-ini:System:[ufront.AutoFOV]:AutoFOV=0` | with AutoFOV |
| `29a71a7` | Viewmodel at 4:3 size (2.29) | UnCanvas.cpp (+Render) | pin FovAngle to base during `eventRenderOverlays`, derive effective FOV in `execDrawActor` | UT99 `Inventory.uc` specific — **verify HP2 script** |
| `d31fdfd` | Publish viewmodel placement (2.29) | UnCanvas.cpp | `[AutoFOV] weapon …` log line for the E2E | only if the HP2 E2E wants it |
| `6525113` | FLUSHCONFIG (2.28) | UnEngine.cpp | explicit "persist now" for the web host before harvesting files | **Required** (host contract) |
| `e2e724b` | World ran faster above the tick floor | UnLevTic.cpp (+Launch) | floor 0.005→0.001; client cap | **Required** |
| `63595e5` | `PriorityActors[0]` uninitialised under C++17 | UnLevTic.cpp | `a[i] = p + i++` is sequenced right-first in C++17 | **Required** (HP2 has the same statement, §5) |
| `b40994b` | SoftDrv fog / filtering | UnRenDev.h UnCamera.cpp | `VolumetricLightingMeshes` config bool on `URenderDevice` | only with the template SoftDrv; HP2's `URenderDevice` differs |
| `0f620a2` | Pad `ABrush` trailing `bColored` | EngineClasses.h | movers ran one leg: `NumKeys` read 3 bytes early | **Required** — HP2 has the identical layout hazard (§10.4) |
| `301a9ca` | MinGW (2.33) | EngineClasses.h UnTex.cpp | `#ifndef __MINGW32__` around the pad; `#ifdef WIN32`→`#if _MSC_VER` on the s3tc block | **Required** (Windows target) |
| `1546db7` | Save: create whole path | UnGame.cpp | `MakeDirectory(SavePath, 1)`; exceptions-off turned a failed save into an abort | **Required** (adapt: HP2's save code differs) |
| `1d3dfa5` | three core defects | UnGame.cpp | `appIsGlobalOptionToken` in `Init`; absolute menu cursor comment | **Required** (needs Core's `appIsGlobalOptionToken`) |
| `5674fc7` | savegame load on Unix | UnGame.cpp | `PATH_SEPARATOR` '/' parsed as URL syntax by FURL → loaded `Index.unr` | **Required** (adapt: HP2 has 3+ extra savegame URL sites) |
| `3ff2ed5`,`7c31a53` | "Received" log line | UnChan.cpp | one `debugf` per downloaded package | **Adapt** — HP2 moved downloads to `UnDownload.cpp` |
| `a57d578` | package version mismatch fails the join | UnGame.cpp (+Core) | NULL linker check in `LoadMap` before `VerifyLinker` | **Required** for net play (web) |


## 3. Inventory — which Epic Engine files the template touches, and HP2's distance from Epic at each

Numbers are changed-line counts (`diff --strip-trailing-cr -w | grep -c '^[<>]'`). **E-T** = what the
template changed vs Epic (the delta this report is about); **E-H** = how far HP2 433 has drifted from Epic
at the same file (the bigger, the more hand-adaptation); **T-H** = direct template→HP2 distance. Presence:
E/T/H = file exists in Epic / template / HP2.

Totals: the template changes **36 of Epic's Engine files** (3,051 unified-diff lines incl. context); it adds
**no** new `.cpp`/`.h` to Engine (only `CMakeLists.txt`; it drops the `.dsp`/makefile/`palette.cpp`).
HP2 adds **24** Engine files Epic v400 does not have (rows with presence `--H`).

| File | E-T | E-H | T-H | presence |
|------|----:|----:|----:|:--------:|
| `Inc/AActor.h` | 0 | 41 | 41 | ETH |
| `Inc/AAmmo.h` | NA | NA | NA | --H |
| `Inc/ABrush.h` | 0 | 6 | 6 | ETH |
| `Inc/ACamera.h` | 0 | 0 | 0 | ETH |
| `Inc/ACarcass.h` | NA | NA | NA | --H |
| `Inc/AGameReplicationInfo.h` | 0 | 2 | 2 | ETH |
| `Inc/AInterpolationManager.h` | NA | NA | NA | --H |
| `Inc/AInterpolationPoint.h` | NA | NA | NA | --H |
| `Inc/AInventory.h` | 0 | 5 | 5 | ETH |
| `Inc/ALevelInfo.h` | NA | NA | NA | --H |
| `Inc/AMover.h` | 0 | 2 | 2 | ETH |
| `Inc/AParticleFX.h` | NA | NA | NA | --H |
| `Inc/APawn.h` | 0 | 13 | 13 | ETH |
| `Inc/APickup.h` | NA | NA | NA | --H |
| `Inc/APlayerPawn.h` | 0 | 11 | 11 | ETH |
| `Inc/APlayerReplicationInfo.h` | 0 | 2 | 2 | ETH |
| `Inc/AWeapon.h` | NA | NA | NA | --H |
| `Inc/AWind.h` | NA | NA | NA | --H |
| `Inc/AZoneInfo.h` | 0 | 2 | 2 | ETH |
| `Inc/ENCVAG.H` | NA | NA | NA | --H |
| `Inc/EngineClasses.h` | 42 | 1424 | 1462 | ETH |
| `Inc/Engine.h` | 0 | 22 | 22 | ETH |
| `Inc/Palette.h` | 0 | 0 | 0 | ETH |
| `Inc/S3tc.h` | 0 | 0 | 0 | ETH |
| `Inc/UGesture.h` | NA | NA | NA | --H |
| `Inc/UI3DL2Listener.h` | NA | NA | NA | --H |
| `Inc/ULevelSummary.h` | 0 | 0 | 0 | ETH |
| `Inc/UnActor.h` | 9 | 17 | 26 | ETH |
| `Inc/UnAnim.h` | NA | NA | NA | --H |
| `Inc/UnAudio.h` | 0 | 208 | 208 | ETH |
| `Inc/UnBunch.h` | 0 | 2 | 2 | ETH |
| `Inc/UnCamera.h` | 73 | 33 | 106 | ETH |
| `Inc/UnChan.h` | 0 | 29 | 29 | ETH |
| `Inc/UnCon.h` | 0 | 22 | 22 | ETH |
| `Inc/UnConn.h` | 0 | 34 | 34 | ETH |
| `Inc/UnDDraw.h` | 0 | 0 | 0 | ETH |
| `Inc/UnDemoPenLev.h` | 0 | 5 | 5 | ETH |
| `Inc/UnDemoRec.h` | 0 | 7 | 7 | ETH |
| `Inc/UnDownload.h` | NA | NA | NA | --H |
| `Inc/UnDynBsp.h` | 0 | 0 | 0 | ETH |
| `Inc/UnEngineGnuG.h` | 0 | 0 | 0 | ETH |
| `Inc/UnEngine.h` | 0 | 21 | 21 | ETH |
| `Inc/UnEngineNative.h` | NA | NA | NA | --H |
| `Inc/UnEngineWin.h` | 0 | 1174 | 1174 | ETH |
| `Inc/UnGame.h` | 0 | 11 | 11 | ETH |
| `Inc/UnIn.h` | 0 | 9 | 9 | ETH |
| `Inc/UnInterpolationPoint.h` | NA | NA | NA | --H |
| `Inc/UnLevel.h` | 0 | 38 | 38 | ETH |
| `Inc/UnMesh.h` | 0 | 144 | 144 | ETH |
| `Inc/UnModel.h` | 8 | 112 | 120 | ETH |
| `Inc/UnNetDrv.h` | 0 | 12 | 12 | ETH |
| `Inc/UnNet.h` | 0 | 5 | 5 | ETH |
| `Inc/UnObj.h` | 50 | 25 | 75 | ETH |
| `Inc/UnParticleFX.h` | NA | NA | NA | --H |
| `Inc/UnParticle.h` | NA | NA | NA | --H |
| `Inc/UnParticleList.h` | NA | NA | NA | --H |
| `Inc/UnPenLev.h` | 0 | 14 | 14 | ETH |
| `Inc/UnPlayer.h` | 0 | 3 | 3 | ETH |
| `Inc/UnPrim.h` | 0 | 172 | 172 | ETH |
| `Inc/UnReach.h` | 0 | 0 | 0 | ETH |
| `Inc/UnRender.h` | 120 | 78 | 188 | ETH |
| `Inc/UnRenderIterator.h` | 0 | 42 | 42 | ETH |
| `Inc/UnRenderIteratorSupport.h` | NA | NA | NA | --H |
| `Inc/UnRenDev.h` | 1 | 14 | 15 | ETH |
| `Inc/UnReplay.h` | NA | NA | NA | --H |
| `Inc/UnScrTex.h` | 0 | 2 | 2 | ETH |
| `Inc/UnSkeletalMesh.h` | NA | NA | NA | --H |
| `Inc/UnSoundContainer.h` | NA | NA | NA | --H |
| `Inc/UnStats.h` | NA | NA | NA | --H |
| `Inc/UnTex.h` | 27 | 91 | 118 | ETH |
| `Inc/UnURL.h` | 0 | 0 | 0 | ETH |
| `Inc/vssver.scc` | NA | NA | NA | E-- |
| `Src/Amd3d.h` | 0 | 0 | 0 | ETH |
| `Src/AStatLog.cpp` | 2 | 93 | 95 | ETH |
| `Src/Engine.cpp` | 0 | 0 | 0 | ETH |
| `Src/Engine.dsp` | NA | 397 | NA | E-H |
| `Src/EnginePrivate.h` | 0 | 0 | 0 | ETH |
| `Src/makefile` | NA | 11 | NA | E-H |
| `Src/mssccprj.scc` | 0 | NA | NA | ET- |
| `Src/palette.cpp` | NA | 18 | NA | E-H |
| `Src/ULodMesh.cpp` | 0 | 68 | 68 | ETH |
| `Src/UnActCol.cpp` | 31 | 57 | 88 | ETH |
| `Src/UnActor.cpp` | 18 | 689 | 707 | ETH |
| `Src/UnAudio.cpp` | 222 | 1028 | 1166 | ETH |
| `Src/UnBunch.cpp` | 0 | 0 | 0 | ETH |
| `Src/UnCamera.cpp` | 151 | 457 | 608 | ETH |
| `Src/UnCamMgr.cpp` | 0 | 7 | 7 | ETH |
| `Src/UnCanvas.cpp` | 93 | 145 | 238 | ETH |
| `Src/UnChan.cpp` | 21 | 171 | 192 | ETH |
| `Src/UnCon.cpp` | 2 | 21 | 23 | ETH |
| `Src/UnConn.cpp` | 0 | 142 | 142 | ETH |
| `Src/UnDemoPenLev.cpp` | 0 | 2 | 2 | ETH |
| `Src/UnDemoRec.cpp` | 0 | 21 | 21 | ETH |
| `Src/UnDownload.cpp` | NA | NA | NA | --H |
| `Src/UnDynBsp.cpp` | 102 | 11 | 113 | ETH |
| `Src/UnEngine.cpp` | 31 | 46 | 77 | ETH |
| `Src/UnEngineNative.cpp` | NA | NA | NA | --H |
| `Src/UnFont.cpp` | 0 | 12 | 12 | ETH |
| `Src/UnFPoly.cpp` | 10 | 13 | 23 | ETH |
| `Src/UnGame.cpp` | 280 | 614 | 880 | ETH |
| `Src/UnGesture.cpp` | NA | NA | NA | --H |
| `Src/UnIn.cpp` | 10 | 38 | 48 | ETH |
| `Src/UnLevAct.cpp` | 9 | 266 | 275 | ETH |
| `Src/UnLevel.cpp` | 14 | 623 | 635 | ETH |
| `Src/UnLevTic.cpp` | 49 | 383 | 428 | ETH |
| `Src/UnMesh.cpp` | 0 | 121 | 121 | ETH |
| `Src/UnModel.cpp` | 75 | 260 | 333 | ETH |
| `Src/UnMover.cpp` | 4 | 0 | 4 | ETH |
| `Src/UnNetDrv.cpp` | 0 | 9 | 9 | ETH |
| `Src/UnParams.cpp` | 0 | 16 | 16 | ETH |
| `Src/UnParticle.cpp` | NA | NA | NA | --H |
| `Src/UnParticleFX.cpp` | NA | NA | NA | --H |
| `Src/UnParticleList.cpp` | NA | NA | NA | --H |
| `Src/UnPath.cpp` | 3 | 1411 | 1414 | ETH |
| `Src/UnPath.h` | 0 | 82 | 82 | ETH |
| `Src/UnPawn.cpp` | 10 | 551 | 561 | ETH |
| `Src/UnPenLev.cpp` | 2 | 71 | 71 | ETH |
| `Src/UnPhysic.cpp` | 8 | 1268 | 1272 | ETH |
| `Src/UnPlayer.cpp` | 0 | 0 | 0 | ETH |
| `Src/UnPrim.cpp` | 0 | 849 | 849 | ETH |
| `Src/UnReach.cpp` | 0 | 16 | 16 | ETH |
| `Src/UnRenderIterator.cpp` | 0 | 21 | 21 | ETH |
| `Src/UnReplay.cpp` | NA | NA | NA | --H |
| `Src/UnRoute.cpp` | 4 | 270 | 274 | ETH |
| `Src/UnScript.cpp` | 11 | 1112 | 1123 | ETH |
| `Src/UnScrTex.cpp` | 4 | 10 | 14 | ETH |
| `Src/UnSkeletalMesh.cpp` | NA | NA | NA | --H |
| `Src/UnTex.cpp` | 41 | 475 | 516 | ETH |
| `Src/UnTrace.cpp` | 127 | 107 | 234 | ETH |
| `Src/UnURL.cpp` | 3 | 0 | 3 | ETH |
| `Src/UnWind.cpp` | NA | NA | NA | --H |
| `Src/vssver.scc` | NA | NA | NA | E-- |
| `Src/winstuff.cpp` | NA | NA | NA | --H |

Reading the table:
- Files with **E-T > 0 and E-H = 0** (template changed it, HP2 kept it at Epic text): `Src/UnMover.cpp`,
  `Src/UnURL.cpp` — the template file can be **copied as-is** (after confirming the E-T delta is wanted; for
  both it is only `Clamp<INT>`/for-scope, see §5).
- **E-T = 0** files need nothing from this report (the template kept Epic's text), but HP2's own drift (E-H)
  may still carry GCC hazards — covered in §5 "HP2-own GCC hazards" and §8.
- `Src/palette.cpp` exists in Epic and HP2 but the template **deleted** it; HP2's CMake list still compiles
  it (see §9: it is the MSVC-only palette-quantiser shim and must be checked).
- `Inc/UnEngineWin.h` E-H = 1174: HP2 rewrote it wholesale (it is the Win32 launcher helper header; §9).


## 4. Hunk-by-hunk: Engine/Inc

Only 8 `Inc/` headers differ between EPIC and TPL. `EngineClasses.h` (42 lines) is covered in §10; the rest are
listed here. HP2 line numbers are `Source/Engine/Inc/<file>`.

| # | File:line (TPL) | Template change (tag) | What / why | HP2 site | Class. | ACTION |
|---|-----------------|-----------------------|------------|----------|--------|--------|
| I1 | `UnActor.h:61-62` | `AActor* Top; for( Top=this; …);` (DC-GCC) | ISO for-scope: `GetTopOwner()` uses `Top` after the loop; MSVC6 leaked the loop variable, GCC errors (`'Top' was not declared in this scope`) | `UnActor.h:61` identical Epic text `for( AActor* Top=this; Top->Owner; Top=Top->Owner );` | **APPLIES-CLEANLY** | hoist the declaration (2-line edit) |
| I2 | `UnActor.h:187-192` | 6 blank lines | noise | – | NOT-APPLICABLE | none |
| I3 | `UnModel.h:16-25` | `#ifdef PLATFORM_DREAMCAST` 4096/8192 `#else` 65536/128000 (WEB 2.7c) | ut99dc had hard-coded DC BSP limits; 2.7c restored stock limits for non-DC | `UnModel.h:16-17` `MAX_NODES = 65536`, `MAX_POINTS = 128000` (stock) | **ALREADY-PRESENT** | none — never import the DC values |
| I4 | `UnRenDev.h:48` | `BITFIELD VolumetricLightingMeshes;` inserted after `VolumetricLighting` (UF `b40994b`) | SoftDrv mesh-fog opt-out; registered as a `Client` config bool in `UnCamera.cpp:27` (`URenderDevice::StaticConstructor`), read in `SoftDrv/Src/DrawPoly.cpp:4494` | HP2 `UnRenDev.h:37-57` has the v400 bool list + `Pad1[8]`/`Pad0[8]` but its **virtual interface is 433's** (see CLAUDE.md) | **NEEDS-ADAPTATION** / optional | Only if the template SoftDrv's mesh-fog code is ported. Inserting a member shifts every later `URenderDevice` field; that is safe only because `URenderDevice` has no script class in HP2 (§10.2) **and** every driver (XOpenGLDrv/UTGLR forks, NullDrv, SoftDrv) is rebuilt against the same header. Prefer appending it **after `DetailTextures`** (before `Pad1`) to minimise churn, or skip it and hard-wire mesh fog in SoftDrv |
| I5 | `UnObj.h:96-135, 174, 245-252, 259` | `#ifdef PLATFORM_DREAMCAST` `FBspNode`/`FBspSurf` with `SWORD` indices (DC `a5e2533`) | 16 MB RAM shrink | HP2 `UnObj.h:96-107` plain `INT iSurf`, `:204-209` `INT pBase…iBrushPoly` | **NOT-APPLICABLE** | none. Do not import (changes the on-disk serialisation contract if `PLATFORM_DREAMCAST` ever leaks) |
| I6 | `UnCamera.h:307-310` | `PUSH_HIT(frame,expr) … PushHit(expr,sizeof(decltype(expr)))` (WEB 2.7d) | clang/Emscripten parses `sizeof(HFoo())` as sizeof of a function type → hard error | HP2 `UnCamera.h:323` still `sizeof(expr)`; users: `Render/Src/UnRender.cpp`, `Render/Src/UnSprite.cpp`, `Editor/Src/{UnEdCam,UnEdRend,UnMatinee}.cpp` (22 sites) | **APPLIES-CLEANLY** | copy the template macro verbatim (+comment). Required for the web target the moment Render compiles |
| I7 | `UnCamera.h:400-467` | AutoFOV API: `enum EAutoFovMode`, `ufrontEffectiveFov`, `ufrontWeaponFov`, `GAutoFovFrameBase`, `ufrontHudScaleFactor` (UF `67976cb`,`52641f8`,`1fb5f26`) | declarations for §5 UnCamera.cpp/UnGame.cpp/UnCanvas.cpp | HP2 `UnCamera.h` ends at `:419`, no such block | **NEEDS-ADAPTATION** (feature, not portability) | append the block to HP2 `UnCamera.h` after the `UClient` class **if** AutoFOV is adopted (§5.C). Drop `ufrontHudScaleFactor` unless an HP2 HUD property to scale is identified |
| I8 | `UnRender.h:125-243, 253` | delete the `#if ASM __asm{…}` and `#elif ASMLINUX asm volatile("…multi-line…")` bodies of `FOutVector::ComputeOutcode`, keep only the C `#else` path (DC-GCC) | MSVC inline asm is not GCC syntax; the AT&T variant uses **multi-line string literals** (a hard error since GCC 3.3) and is x87-only (impossible on wasm32) | HP2 `UnRender.h:129 #if ASM`, `:130 __asm`, `:192 #elif ASMLINUX`, `:194,229 asm volatile ("`, `:246 #else`, `:256 #endif` — identical to Epic | **APPLIES-CLEANLY** | delete lines 129-246 and the `#endif` at 256, leaving the C clip-number/outcode code. **Critical**: HP2's raw `Core/Inc/UnGnuG.h:15-18` `#undef ASM … #define ASMLINUX 1` would actually **select** the asm branch on Linux (UnBuild.h `#define ASM 1` runs first at `Core.h:22`, then UnGnuG.h undefines it, so `#if ASM`=0 but `#elif ASMLINUX`=1). The template both deleted the block **and** forced `ASM 0 / ASMLINUX 0 / ASM3DNOW 0` in `Core/Inc/UnGnuG.h:207-212` — the Core port must do the latter too, otherwise `UnMesh.cpp` (§8) is also hit |
| I9 | `UnTex.h:56-59` | `Clamp<INT>(appFloor(…),0,255)` in `FColor(const FPlane&)` (DC) | sh-elf `int32_t==long` ambiguity | HP2 `UnTex.h:63-66` identical Epic text | NOT-APPLICABLE (harmless) | none required on i386/wasm32/MinGW; copy if convenient |
| I10 | `UnTex.h:101-108` | `FColor::RGB888ToARGB1555()`, `BGRA7777ToRGB565()` (DC) | DC PVR texture upload | – | NOT-APPLICABLE | none (verify no ported driver calls them: `grep -rn RGB888ToARGB1555` in the forks) |
| I11 | `UnTex.h:233-241` | `TEXF_EXT_START=128 … TEXF_EXT_MAX` in `ETextureFormat` (DC) | DC formats | HP2 `UnTex.h:228-231` `TEXF_DXT1=3,TEXF_RGB8=4,TEXF_RGBA8=5,TEXF_MAX=0xff` | NOT-APPLICABLE | none. Note TPL `TEXF_MAX` is still 0xff so the enum stays a byte |
| I12 | `UnTex.h:305` | `BYTE LODSet GCC_ALIGN(4);` (DC-GCC) | warning cleanup (`packed` ignored on BYTE); layout identical | HP2 `UnTex.h:297` `BYTE LODSet GCC_PACK(4);` | APPLIES-CLEANLY (cosmetic) | optional; `UTexture` script layout already verified identical under both models (§10.2) |
| I13 | `EngineClasses.h` (9× `GCC_PACK`→`GCC_ALIGN`, `bColoredPad`, `#endif NAMES_ONLY`) | DC-GCC / UF `0f620a2` / `301a9ca` | see §10.3/§10.4 | HP2 has 13 `BYTE … GCC_PACK(4)` sites + the same ABrush hazard + an extra PlayerStart hazard | NEEDS-ADAPTATION | §10.4 L1+L2 |

**Headers HP2 must also touch that the template did not have to** (HP2-own drift, GCC-relevant, found while
diffing Inc/):
- `UnCamera.h:222,238` `FTime LastUpdateTime/CurrentTime` (433 uses the `FTime` class instead of `DOUBLE`) —
  make sure Core's `FTime` compiles under GCC (Core report); nothing to do in Engine.
- `UnCamera.h:286-293` new non-pure `UViewport` virtuals `TryRenderDevice`, `DrawString`, `CreateNativeFont`,
  `LoadSmackerMovie/BlitNextSmackerFrame/ShutdownSmacker` — NSDLDrv's `USDLViewport` inherits the no-op
  defaults; Smacker movies therefore silently do nothing on the port (drivers report's concern).
- `UnCamera.h:383,394` new `UClient` config members `NoFractalAnim`, `ParticleDensity` — the ported `UClient`
  config section (`[SDLDrv.SDLClient]`/`[NSDLDrv.NSDLClient]`) should carry them in the web ini.

## 5. Hunk-by-hunk: Engine/Src

Sub-sections: **5.A** the mechanical GCC/C++17 hunks in the small files (one row per hunk, located in HP2 by exact
text match — `HP2@n` is the HP2 line holding Epic's original text); **5.B** UnLevTic / UnEngine / UnActCol / UnTex /
UnChan; **5.C** UnCamera / UnCanvas (AutoFOV); **5.D** UnGame; **5.E** UnAudio; **5.F** UnTrace / UnDynBsp / UnModel
(the Dreamcast SWORD BSP set); **5.G** HP2-own hazards found at the same sites that the template never had.

General rule for the whole class of "for-scope" hunks: GCC ≥ 9 removed `-fno-for-scope`, so every MSVC6 leaked
loop variable is a hard error (`'i' was not declared in this scope`) and **the compiler lists every site**. The
template's hunks are the UT99-v400 subset; HP2's 433 code (E-H thousands of lines) has more (§5.G). Treat the
rows below as a checklist, not as the complete set.

### 5.A Small files (mechanical)

| # | TPL file:line | Change (tag) | HP2 site | Class. | ACTION |
|---|---------------|--------------|----------|--------|--------|
| S1 | `AStatLog.cpp:124` | `for (INT j=0;…)` (DC-GCC) | `AStatLog.cpp:155` `for (j=0; j<FileName.Len(); j++)` | APPLIES-CLEANLY | add `INT` (or check a prior `j` in HP2's longer function; HP2 E-H=93) |
| S2 | `UnActor.cpp:279-287` | expand `DOREP(PlayerPawn,TargetViewRotation)` into a copy-to-temporaries block (DC-GCC) | `UnActor.cpp:366` identical `DOREP(PlayerPawn,TargetViewRotation);` | **APPLIES-CLEANLY** | copy the 9-line block. Reason: `FRotator TargetViewRotation GCC_PACK(4)` (HP2 `EngineClasses.h:2046`, after the bool run `ngSecretSet`,`ReceivedSecretChecksum`) cannot bind to `NEQ(FRotator&,FRotator&,…)` → GCC "cannot bind packed field". I checked every `GCC_PACK(4)` non-bool/non-BYTE member of HP2's header (52 of them) against every `DOREP`/`DOREPARRAY` in HP2 Engine: the only other replicated ones are `MultiSkins` (`:168`, `UObject*` by value), `RemainingTime` (`:518`, `INT` by value), `TalkTexture` (`:491`, pointer), `ViewTarget` (`:359`, pointer) — all by-value overloads, fine. So TargetViewRotation is the **only** DOREP site to fix |
| S3 | `UnActor.cpp:858,870,880,952` | `for( INT i=0; i<ARRAY_COUNT(Actor->Touching)…)` (DC-GCC) | `UnActor.cpp:1161,1173,1183,1255` | APPLIES-CLEANLY | add `INT` at the four sites |
| S4 | `UnCon.cpp:135` | `::Max<INT>(0,BorderLines)` (DC) | `UnCon.cpp:147` | NOT-APPLICABLE | none (i386/wasm/MinGW `INT`=`int`) |
| S5 | `UnFPoly.cpp:278,486` | `INT VeryPrecise`→`int`, `INT FPoly::RemoveColinears()`→`int` (DC: header/impl signature agreement when `INT`≠`int`) | `UnFPoly.cpp:280,485` | NOT-APPLICABLE | none |
| S6 | `UnFPoly.cpp:283-287` | `#ifdef PLATFORM_DREAMCAST … Surfs((INT)Node.iSurf)` (DC SWORD) | `UnFPoly.cpp:284-285` | NOT-APPLICABLE | none |
| S7 | `UnFPoly.cpp:355` | `for( INT i=0; i<NumVertices…)` (DC-GCC) | `UnFPoly.cpp:353` (also `:75,146,186` same text) | APPLIES-CLEANLY | add `INT` at 353; let the compiler judge the others |
| S8 | `UnIn.cpp:32` | `for( TFieldIterator<UProperty> It(Class); It; ++It )` (DC-GCC: `TFieldIterator` has no default ctor/assignment) | `UnIn.cpp:32` identical | APPLIES-CLEANLY | copy |
| S9 | `UnIn.cpp:131,158` | hoist `INT i;` before `for( i=0; i<Cache->Count; …)` (DC-GCC: `i` used after the loop) | `UnIn.cpp:131,157` (and `:414`, HP2-only site) | APPLIES-CLEANLY | hoist at 131/157; check 414 |
| S10 | `UnIn.cpp:459` | `UBOOL UInput::FindKeyName`→`int` (DC) | `UnIn.cpp:479` | NOT-APPLICABLE | none |
| S11 | `UnLevAct.cpp:12-14` | forward-declare `QSORT_RETURN CDECL CompareHits(const FCheckResult*,const FCheckResult*);` (DC-GCC) | HP2 uses it at `UnLevAct.cpp:1509`; it is only an **inline friend** in `Inc/UnPrim.h:65`, invisible to ordinary lookup under ISO C++ | **APPLIES-CLEANLY** | add the 2-line forward decl at the top of HP2 `UnLevAct.cpp` |
| S12 | `UnLevAct.cpp:612,725,1110,1228` | hoist `INT i;`, `FCheckResult* Test;`, `FCheckResult* Check;` (DC-GCC) | `UnLevAct.cpp:627` (Import properties), `:1270` (encroach notify), `:1388` (`for( FCheckResult* Check = FirstHit;…`) ; the `:725` site has no unique anchor in HP2 (E-H=266) | NEEDS-ADAPTATION | hoist at the three located sites; compiler finds the rest |
| S13 | `UnLevel.cpp:310,322` | `INT i;` / `for( INT i=0; i<Actors.Num()…)` in `MatchEditorOther` (DC-GCC) | `UnLevel.cpp:839,851` | APPLIES-CLEANLY | copy |
| S14 | `UnLevel.cpp:649,661` | `Min<INT>(RemoteVer,ENGINE_VERSION)`, `Clamp<INT>(Rate,500,…)` (DC) | `UnLevel.cpp:1187`; HP2 `:1199` is `Clamp( Rate, 2000, NetDriver->MaxClientRate )` (HP2 raised the floor) | NOT-APPLICABLE | none; keep HP2's 2000 |
| S15 | `UnLevel.cpp:679` | `!Engine->ChallengeResponse(C)==Response` → `!(Engine->ChallengeResponse(C)==Response)` (DC-GCC, `-Wlogical-not-parentheses`) | `UnLevel.cpp:1232` identical Epic text | **APPLIES-CLEANLY — behaviour change** | Copy. Note Epic's form is `(!X)==Response`, which is almost always false, i.e. the server **never** rejected a bad challenge; the fix makes it real. Harmless when both ends run this engine (same `UGameEngine::ChallengeResponse`), but if a join fails with `FAILURE CHALLENGE`, this is the line |
| S16 | `UnLevel.cpp:691` | hoist `const TCHAR* Tmp;` (DC-GCC) | `UnLevel.cpp:1246` | APPLIES-CLEANLY | copy |
| S17 | `UnMover.cpp:106,125` | `Clamp<INT>` (DC) | `UnMover.cpp:106,125` | NOT-APPLICABLE | none (file otherwise identical to Epic in HP2 — can be copied wholesale) |
| S18 | `UnPath.cpp:257,659,898` | hoist `INT i;`/`INT j;` (DC-GCC) | `:257`→HP2 `:459`; `:659`,`:898` have no anchor (HP2 rewrote UnPath, E-H=1411) | NEEDS-ADAPTATION | compiler-driven |
| S19 | `UnPawn.cpp:294,1467` | hoist `INT j;`, `INT i;` (DC-GCC) | `UnPawn.cpp:459,1752` | APPLIES-CLEANLY | copy |
| S20 | `UnPawn.cpp:647,658` | `::Max<INT>`/`Min<INT>` stairRot (DC) | `UnPawn.cpp:849,860` | NOT-APPLICABLE | none |
| S21 | `UnPenLev.cpp:220` | `for( INT i=0; i<Connection->PackageMap->List.Num()…)` | `UnPenLev.cpp:202,284` already `for( INT i=0;…)` | **ALREADY-PRESENT** | none |
| S22 | `UnPhysic.cpp:330,332,1373` | `Min<INT>`/`::Max<INT>` (DC) | HP2 `:417,419` use `appRound(…)` (already INT), `:1668` | NOT-APPLICABLE | none |
| S23 | `UnPhysic.cpp:1914` | `findNewFloor(…, int Iterations)`→`INT` (DC) | `UnPhysic.cpp:2213` | NOT-APPLICABLE | none |
| S24 | `UnRoute.cpp:274` | `for ( INT j=0; j<AltEndPoints.numPoints…)` (DC-GCC) | `UnRoute.cpp:274` | APPLIES-CLEANLY | copy |
| S25 | `UnRoute.cpp:470` | `::Max<INT>(10,…)` (DC) | `UnRoute.cpp:470` | NOT-APPLICABLE | none |
| S26 | `UnScript.cpp:410-416` | `#ifdef PLATFORM_DREAMCAST` memcpy of `DeltaSeconds` (DC unaligned access) | `UnScript.cpp:564` `FLOAT DeltaSeconds = *(FLOAT*)Result;` | NOT-APPLICABLE | none (wasm32 tolerates unaligned loads; x86 too) |
| S27 | `UnScript.cpp:1844,1858,2082` | hoist `INT i;`, `INT j;` (DC-GCC) | `UnScript.cpp:2314,2328,2551` | APPLIES-CLEANLY | copy |
| S28 | `UnScrTex.cpp:242,245` | `Min<INT>( Max<INT>(…))` (DC) | `UnScrTex.cpp:242,245` | NOT-APPLICABLE | none |
| S29 | `UnURL.cpp:433` | hoist `INT i;` (DC-GCC) | `UnURL.cpp:433` (HP2 file is otherwise identical to Epic: E-H=0) | APPLIES-CLEANLY | copy the template file wholesale |

### 5.B UnLevTic / UnEngine / UnActCol / UnTex / UnChan

| # | TPL file:line | Change (tag) | What / why | HP2 site | Class. | ACTION |
|---|---------------|--------------|------------|----------|--------|--------|
| B1 | `UnLevTic.cpp:546-555` | split `PriorityActors[ConsiderCount] = PriorityList + ConsiderCount++;` into two statements (UF `63595e5`) | C++17 sequences the right operand of `=` first; the subscript then uses the incremented value, `PriorityActors[0]` is never written, `Sort()` dereferences an uninitialised GMem slot → "memory access out of bounds" on the web server at high net speed | **THREE** copies in HP2: `UnLevTic.cpp:655` (the 433-only "add LevelInfo to the list" loop), `:686` (bAlwaysRelevant branch), `:703` (UpdateFrequency branch) | **NEEDS-ADAPTATION (3×)** | apply the template split at all three sites; carry the template comment once. (Or compile with `-std=gnu++14`; the template chose to fix the code — do the same) |
| B2 | `UnLevTic.cpp:638` | `for( INT i=0; i<NetDriver->ClientConnections.Num()…)` in the stats block (DC-GCC) | leaked `i` | `UnLevTic.cpp:795` identical | APPLIES-CLEANLY | add `INT` |
| B3 | `UnLevTic.cpp:816-849` | `Clamp(DeltaSeconds,0.005f,0.40f)` → `Clamp(DeltaSeconds,0.001f,0.40f)` + long rationale (UF `e2e724b`, step 2.19) | the floor hands actors a larger delta than elapsed above 200 fps → the world runs fps/200× too fast on an uncapped web client | HP2 `UnLevTic.cpp:959-960`: `// Clamp time between 200 fps and 10 fps.` `DeltaSeconds = Clamp(DeltaSeconds,0.005f,0.10f);` — **HP2 changed the ceiling to 0.10** (a 100 ms hitch cap, KnowWonder's choice) | **NEEDS-ADAPTATION** | change only the floor: `Clamp(DeltaSeconds,0.001f,0.10f)`. Keep HP2's 0.10 ceiling (game-tuned; physics/tunnelling depend on it). Port the template comment, adjusting the ceiling sentence. Pair it with the launcher `FrameRateCap` (launcher report) |
| B4 | `UnEngine.cpp:100-104` | `#ifdef PLATFORM_DREAMCAST GCache.Init(256K,2048) #else … Clamp<INT>(…)` (DC) | – | `UnEngine.cpp:107` Epic text | NOT-APPLICABLE | none (wasm heap growth covers `CacheSizeMegs`; the web ini sets it — web-ini report) |
| B5 | `UnEngine.cpp:218-238` | `FLUSHCONFIG` console command, **first** in `UEngine::Exec`, `GConfig->Flush(0)` + `Ar.Log("ufront: flushed config to disk (ok)")` (UF `6525113`, step 2.28) | explicit persist-now the ufrontend host sends through `ue_exec_command` before harvesting MEMFS/IDB; a tab is closed, never shut down | HP2 `UnEngine.cpp:222-235`: `UEngine::Exec` starts with the subsystem fan-out (`GSys`, `StaticExec`, `GCache`, `GExec`, `Client`, `Render`, `Audio`) then `FLUSH` | **APPLIES-CLEANLY** | paste the template block immediately after `guard(UEngine::Exec);` (before the fan-out, so no subsystem can shadow it). HP2's `UObject::SaveConfig` already ends in `GConfig->Flush(0)` (`Core/Src/UnObj.cpp:961`), same as v400. Also required by the host contract (ufrontend-target-contract report): the literal log text is what the host may match |
| B6 | `UnEngine.cpp:301-302` | hoist `UClass* C;` in the quit-menu key hook (DC-GCC) | `C` tested after the loop | `UnEngine.cpp:288` identical | APPLIES-CLEANLY | copy. (Note the class names it checks, `UnrealQuitMenu`/`UnrealYesNoMenu`, do not exist in HP2 — the hook is dead but harmless) |
| B7 | `UnActCol.cpp:39-49` | `#ifdef PLATFORM_LOW_MEMORY` smaller hash (DC) | – | `UnActCol.cpp:38-43` | NOT-APPLICABLE | none |
| B8 | `UnActCol.cpp:127` | `for( INT i=0; i<NUM_BUCKETS…)` (DC-GCC) | leaked `i` | `UnActCol.cpp:121` | APPLIES-CLEANLY | add `INT` |
| B9 | `UnActCol.cpp:259-271, 303-310` | Dreamcast "moved without proper hashing, correcting" repair instead of `appErrorf` (DC `383f08c`) | on DC a mis-hashed actor aborted the game | HP2 `UnActCol.cpp:254-256` already downgraded: `debugf( TEXT("%s moved without proper hashing"), … ); //appErrorf(…)`, and `RemoveActor` gained a `bool bThorough` bucket sweep (`:262`) | **ALREADY-PRESENT** (HP2's own softer fix) | none. Do not add the DC relocation hack |
| B10 | `UnTex.cpp:97` | `Min<INT>` (DC) | – | `UnTex.cpp:106` | NOT-APPLICABLE | none |
| B11 | `UnTex.cpp:310-329` | `PLATFORM_LOW_MEMORY` force lazy-load; DC mip-data drop after load (DC `2673db1`) | 16 MB RAM | `UnTex.cpp:320-331` (HP2 already has `Min<INT>` in the LOD line) | NOT-APPLICABLE | none |
| B12 | `UnTex.cpp:384-385` | `Clamp<INT>` UClamp/VClamp (DC) | – | `UnTex.cpp:384-385` | NOT-APPLICABLE | none |
| B13 | `UnTex.cpp:437-575` | `#ifndef PLATFORM_LOW_MEMORY` around the palette/`UTexture` helper block (DC) | – | – | NOT-APPLICABLE | none |
| B14 | `UnTex.cpp:667,731-732` | hoist `INT i;` (DC-GCC) — `UPalette::ReplaceWithExisting` tests `i == NUM_PAL_COLORS` after `for( int i=0;…)` | `UnTex.cpp:727-728` identical | APPLIES-CLEANLY | copy |
| B15 | `UnTex.cpp:752-759` | the `Compress`/`Decompress` s3tc block: `#ifdef WIN32` → `#if _MSC_VER` (UF `301a9ca`, step 2.33) | MinGW defines `WIN32`, compiled the block, then failed to link `S3TCencode/S3TCdecode/HeckbertQuantize` (only `s3tc.lib`, MSVC-only, provides them) | HP2 `UnTex.cpp:747 #ifdef WIN32` … `:956 #else` (return-0 stubs) … `:967 #endif` | **APPLIES-CLEANLY** | change `:747` to `#if _MSC_VER` + template comment. Also change the include guard at HP2 `UnTex.cpp:12-16` (`#ifdef WIN32 #include "UnDDraw.h" "Palette.h" "S3tc.h"`) to `#if _MSC_VER` — the template left that one at `WIN32`, which works only because the three headers happen to compile under MinGW; HP2's `Palette.h`/`S3tc.h` are identical to Epic's (E-H=0), so either is fine, but `_MSC_VER` is the honest guard. CLAUDE.md: "s3tc's encode path is editor-only" — consistent |
| B16 | `UnChan.cpp:149` | `Max<INT>` (DC) | – | `UnChan.cpp:149` | NOT-APPLICABLE | none |
| B17 | `UnChan.cpp:245,353,840,953,1043` | hoist `FInBunch** InPtr;`, `FOutBunch** OutLink;`, `UFunction* Test;`, `AActor* Top;`, `INT* R;` (DC-GCC — each is used after its `for(…);`) | leaked loop vars | `UnChan.cpp:245,352,865,977,1067` identical | APPLIES-CLEANLY | copy all five |
| B18 | `UnChan.cpp:1323-1326` | `debugf( TEXT("%s"), Msg );` after `appSprintf( Msg, TEXT("Received '%s'"), PrettyName );` in `UFileChannel::ReceivedBunch` (UF `3ff2ed5`,`7c31a53`) | one log line per downloaded package, for the native log and the web console (the page does **not** key cache flushes on it) | HP2 moved the download path into **`UnDownload.cpp`** (HP2-only file): `UnDownload.cpp:176-178` `appSprintf( Msg, TEXT("Received '%s'"), Info->Parent->GetName() ); …NotifyProgress( TEXT("Success"), Msg, 4.f ); …NotifyReceivedFile(…)` | **NEEDS-ADAPTATION** | insert `debugf( TEXT("%s"), Msg );` after `UnDownload.cpp:176`. Only reachable once IpDrv/WebRTCDrv joins exist (HP2 retail had no MP) |

### 5.C UnCamera.cpp / UnCanvas.cpp (AutoFOV + mechanical)

| # | TPL file:line | Change (tag) | HP2 site | Class. | ACTION |
|---|---------------|--------------|----------|--------|--------|
| C1 | `UnCamera.cpp:27` | register `VolumetricLightingMeshes` as a `Client` config bool in `URenderDevice::StaticConstructor` (UF `b40994b`) | HP2 `UnCamera.cpp:22-35` (same function, v400 property list) | goes with I4 | only if I4 is taken |
| C2 | `UnCamera.cpp:247-392` | AutoFOV implementation: `GAutoFovFrameBase`, `ufrontParseAutoFov()` (reads `[ufront.AutoFOV] AutoFOV/ReferenceAspect/WeaponScale/AutoFOVUI/UIScale` with `GConfig`, logs `[AutoFOV] mode=… referenceAspect=…`), `ufrontEffectiveFov()` (Hor+ / Adaptive, logs `[AutoFOV] viewport=%ix%i aspect=%f base=%f effective=%f` when inputs change and `bLog`), `ufrontWeaponFov()`, `ufrontHudScaleFactor()` (UF `67976cb`,`52641f8`,`1fb5f26`,`144954c`) | nothing in HP2. Pure new code, no dependency on 433 internals except `GConfig`, `appAtan/appTan`, `Clamp/Max/Abs`, `debugf` — all present in HP2 Core | **APPLIES-CLEANLY** (new block) | paste after HP2 `UnCamera.cpp:253` (end of the `UViewport::SetDrag` section, before `FSceneNode::ComputeRenderCoords`) together with the I7 header block. Keep the section name `[ufront.AutoFOV]` (it is spelled identically across ufront engines; the host/E2E reads the log lines verbatim). Drop `ufrontHudScaleFactor` + `GAutoFovUi*` if C4 is skipped (or keep them and ship `AutoFOVUI=0`) |
| C3 | `UnCamera.cpp:656,672` | `Clamp<INT>( Rate, 500, Driver->MaxClientRate )` in `UViewport::Exec` NETSPEED (DC) | HP2 `UViewport::Exec` (`:558…`) | NOT-APPLICABLE | none |
| C4 | `UnCanvas.cpp:569-658` | **viewmodel pass**: in `UCanvas::execDrawActor`, recompute the effective FOV from `GAutoFovFrameBase`, temporarily set `Viewport->Actor->FovAngle = ufrontWeaponFov(…)` + `Frame->ComputeRenderSize()` around `Render->DrawActor`, and log `[AutoFOV] weapon viewport=… placeFov=… drawFov=… dist=…` (UF `29a71a7`,`d31fdfd`, step 2.29) | HP2 `UnCanvas.cpp:612-633` `execDrawActor(Actor, WireFrame, ClearZ)` (433 added `ClearZ` → `RenDev->ClearZ`) plus an HP2-only `execDrawClippedActor` (`:635…`). Script users: `Engine/Classes/Inventory.uc`, `Weapon.uc` (UT-style viewmodel, largely unused by HP2), and `HGame/Classes/hprop.uc:130` `canvas.DrawActor(self,false,true)` (props flying into the HUD) | **NOT-APPLICABLE as written** | The 2.29 pass exists only because the template's **Render** package pins `FovAngle` to the base FOV during `eventRenderOverlays` (render report); in the template `SavedWeaponFov` is therefore the base. HP2 has no such pin, so during `eventPostRender` `Viewport->Actor->FovAngle` already holds the *effective* FOV (C5 scope), `bWeaponFovPass` would be false and the code is a no-op except for the log line. **Skip it** unless the render pin is ported; hprop/inventory actors drawn from PostRender then use the same corrected FOV as the world, which is what a third-person game wants. If ported anyway, add it to **both** `execDrawActor` and `execDrawClippedActor` |
| C5 | `UnCanvas.cpp:242,296` | `Min<INT>(NewPage,4)`, `for( INT i=0; i<5; i++ )` (DC / DC-GCC) | HP2 `UnCanvas.cpp` font-page code | APPLIES-CLEANLY (296) / NOT-APPLICABLE (242) | compiler-driven |

**AutoFOV applicability to HP2 (decision input).** HP2 is third-person; its camera scripts drive
`DesiredFOV` (`HGame/Classes/BaseCam.uc:167`, `FOVController.uc:49,64`) and `PlayerPawn.uc:2689-2690` derives
`MouseScale` from `DesiredFOV * 0.01111` exactly like UT — so the template's "scoped override, never written back"
design is precisely right for HP2 too. One HP2 script reads `FovAngle` for projection, `PlayerPawn.uc:3450`
`CameraToWorld()`; it runs outside `Draw` (gameplay tick) so it sees the stock FOV — same as the template. **ACTION:**
adopt C2 + D-AutoFOV (UnGame scope), skip C4 and the AutoFOVUI hunk (D12), ship `[ufront.AutoFOV] AutoFOV=2`.

### 5.D UnGame.cpp

HP2's `UnGame.cpp` is 2,100+ lines vs Epic's 1,700 (E-H 614): replays (`Replay.Record/Replay`), `-LOAD=` boot,
`GSys->SaveSlotPath` save slots, persistent-actor cache (`SAVEPACTORS`, `LoadPersistentActorCache`), `OBJCLEAN`,
a queued-save hook in `Tick`, `#if HPDEMO` map gate, `LEGEND` RCI blocks. Every template hunk still has an
identifiable site.

| # | TPL line | Change (tag) | HP2 site | Class. | ACTION |
|---|----------|--------------|----------|--------|--------|
| D1 | `:170-174` | `PLATFORM_LOW_MEMORY` flush+GC after Entry load (DC) | `:171` | NOT-APPLICABLE | none |
| D2 | `:187-190` | `\|\| appIsGlobalOptionToken(Parm)` — a global option such as `CPUSPEED=3000` is not a map (UF `1d3dfa5`, step 2.38) | `UGameEngine::Init` `:181-185` identical (`\|\| Parm[0]=='-' )`) | **NEEDS-ADAPTATION** | copy the Engine line; **requires** `appIsGlobalOptionToken` from template Core (`engine-ut99/Source/Core/Inc/UnFile.h:361`, `Src/UnMisc.cpp:1080`) — HP2 Core has none. Core report owns the Core half |
| D3 | `:203-215` | `#ifdef __EMSCRIPTEN__` failed startup map → warn + fall back to `GEntry` instead of `appErrorf` (WEB 2.6g) | `:213-214` `if( !Success ) appErrorf( LocalizeError("FailedBrowse"), Parm, *Error );` identical | **APPLIES-CLEANLY** | copy. Note HP2 inserts the `-REPLAY=`/`-RECORD=` block (`:187-199`) before it; irrelevant |
| D4 | `:288-292` | `SetProgress`: `&& Client->Viewports(0)->Actor` NULL guard (WEB 2.12) | `SetProgress` `:303` `if( Client && Client->Viewports.Num() )` identical | **APPLIES-CLEANLY** | copy |
| D5 | `:509-521` | `#ifdef __EMSCRIPTEN__` `<host>?connect` → promote `URL.Map` to `URL.Host` (WEB 2.7d) | `Browse` right after `debugf( TEXT("Browse: %s"), *URL.String() );` (`:650`) | **APPLIES-CLEANLY** | copy, placed before `if( !URL.Valid )`. Only needed once IpDrv/WebRTCDrv joins are wired (HP2 retail = single-player) |
| D6 | `:570-577` | hub-**pop** branch: build `FURL PopURL(&URL,TEXT(""),TRAVEL_Partial)`, then set `PopURL.Map = Filename` (UF `5674fc7`) | `:701-702` `URL = FURL( &URL, Filename, TRAVEL_Partial ); URL.Portal = SavedPortal;` — HP2 builds `Filename` from `GSys->SaveSlotPath` (`:699`) | **APPLIES-CLEANLY** | copy |
| D7 | `:593-615` | **`?load=N`** branch: `FURL LoadURL( &LastURL, TEXT("?load"), TRAVEL_Partial ); LoadURL.Map = …Save%i.usa;` + hoisted `INT i;` + local `FString Temp` (UF `5674fc7` + DC-GCC) | `:714-739`: `Temp=FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa?load"), *GSys->SaveSlotPath, …)` fed to `FURL(&LastURL,*Temp,TRAVEL_Partial)`; `for( INT i=0;…)` then `i++` after the loop (`:723,735`) | **NEEDS-ADAPTATION** | port the template branch substituting `GSys->SaveSlotPath` for `GSys->SavePath`; hoist `INT i;` (GCC error otherwise). Reached from `PlayerPawn.uc:1677` `ClientTravel( "?load=9", … )` |
| D7b | — (HP2-only) | same `'/'`-vs-FURL hazard, **not in the template** | `UGameEngine::Exec` `LOADGAME` (`:462-476`) and `Init` `-LOAD=` (`:245-255`) call `GLevel->GetLevelInfo()->eventServerTravel( Filename, 0 )` with `Filename = "<SaveSlotPath>/SaveN.usa"`, i.e. `../Save/Slot1/Save0.usa` on Unix. Traced through HP2 `UnURL.cpp:190-320`: no drive letter → not a "pure filename"; first `.` is at index 0 → no host; `IsInternal()` → first `/` makes `..` the map and `Save/Slot1/Save0.usa` the portal, second `/` with trailing text → **`Valid = 0`** → Browse fails `InvalidUrl`. On Windows (`\`) it parses as a map | **NEEDS-ADAPTATION (HP2-only, load-bearing — this is the game's Load menu)** | Fix once in `FURL::FURL` rather than per call site: extend the pure-filename test at `UnURL.cpp:191` to `appStrlen(URL)>2 && ( URL[1]==':' \|\| (URL[0]=='.' && URL[1]=='.' && URL[2]=='/') \|\| URL[0]=='/' )` under `#if __UNIX__ \|\| __EMSCRIPTEN__` (HP2 never builds a network URL starting with `../` or `/`; `//host` is the only legal leading-slash form — keep it by testing `URL[0]=='/' && URL[1]!='/'`). Verify with a native E2E: save in slot 1, `LOADGAME 0`, expect `Browse: ../Save/Slot1/Save0.usa` followed by a successful `LoadMap`. D6/D7 can then keep either form |
| D8 | `:724-725` | hoist `INT i;` before the Pending `PackageMap->List` loop (DC-GCC) | `LoadMap` `:896` `for( INT i=0;…)` then `:905 for( i=0;…)` | APPLIES-CLEANLY | hoist |
| D9 | `:734-750` | `#ifdef __EMSCRIPTEN__` NULL `Linker` → `PackageVersion`/`PackageNotFound` error, `EndLoad`, `SetProgress`, `return NULL` **before** `VerifyLinker` (UF `a57d578`, 2.79o) | between `:904` (GetPackageLinker loop) and `:905` (VerifyLinker loop) | **APPLIES-CLEANLY** | copy. Dependencies present in HP2: `appFindPackageFile` (`Core/Inc/UnFile.h:88`), `Core.int` keys `PackageNotFound`,`PackageVersion`,`UrlFailed` (`Assets/System/Core.int:18,22,48`). The Core half of `a57d578` (GetPackageLinker returning NULL on version mismatch instead of throwing) is the Core report's |
| D10 | `:756-774` | capture `ULevel* Verified = LoadObject<ULevel>(… LOAD_Verify …)`; web: NULL → `return NULL` before DissociateViewports (WEB 2.12) | `:910` `LoadObject<ULevel>( MapParent, TEXT("MyLevel"), *URL.Map, LOAD_Verify \| LOAD_Throw \| LOAD_NoWarn, NULL );` then `EndLoad();` `:911` | **APPLIES-CLEANLY** | copy; the template's early return does **not** call `EndLoad()` after `BeginLoad()`… it relies on SafeLoadError having unwound the load count — keep identical to the template (verified working there), but note it in the port commit |
| D11 | `:871-883` | `#ifdef __EMSCRIPTEN__` `if( !GLevel )` after the real level load → error + `return NULL` (WEB 2.6g) | after `:1066 GLevel = LoadObject<ULevel>( …, LOAD_NoFail, NULL );` and **before** `VERIFY_CLASS_OFFSET` (`:1085`) / `check(GLevel)` (`:1091`). HP2 also dereferences `Info` earlier (`:1040`, `LevelInfo0` load) — guarded by `if( Info )` already | **APPLIES-CLEANLY** | insert after `:1067` (after the `LoadLevel` guard) |
| D12 | `:916` | `Max<INT>(SavedHubStackLevel-1,0)` (DC) | `:1101` | NOT-APPLICABLE | none |
| D13 | `:976-979, 1082-1085, 1173-1174` | hoist `INT i, j;` / `INT i;` (DC-GCC) | `:1175-1188` (64×64 loops), `:1277-1285` (kill client actors), `:1373` (`for( INT i=2;…)`) | APPLIES-CLEANLY | hoist; HP2 has further sites (`:1808`, `:2005`, `SaveGame :1990-2001` uses `i` after the loop) → compiler-driven |
| D14 | `:1254-1280, 1297-1300` | `struct FScopedAutoFov` (sets `GAutoFovFrameBase`, writes the effective FOV, restores in dtor) armed in `Draw` after `eventPlayerCalcView` (UF `67976cb`) | `Draw` `:1490-1495` (`eventPlayerCalcView`, `check(ViewActor)`) — HP2 also sets `GIsDrawing` and returns early on re-entry | **APPLIES-CLEANLY** (if AutoFOV adopted) | insert `FScopedAutoFov AutoFovScope( Viewport );` after `check(ViewActor);` at `:1495`. ⚠ HP2's `Draw` has **early `return`s after `GIsDrawing = 1`** — the scope object restores FovAngle on every exit path, which is exactly why the template used RAII; do not convert it to paired statements |
| D15 | `:1379-1402` | AutoFOVUI: scoped `HudScale *= ufrontHudScaleFactor()` around `eventPostRender`, found with `FindField<UFloatProperty>(Hud->GetClass(),TEXT("HudScale"))` (UF `1fb5f26`) | `:1562-1563` (`EndFlash`, `eventPostRender`) | **NOT-APPLICABLE** | HP2 has **no `HudScale`** on any HUD class; its HUD sizing is `Canvas.GetHudScaleFactor() { return SizeX / HUD_BASE_RESOLUTION_X; }` (`Engine/Classes/Canvas.uc:106-109`, 6 script callers) — a width-only formula computed in script from `Canvas.SizeX`, so no C++-side property scale reaches it without recompiling Engine.u (forbidden). Leave out; list as open question §13 |
| D16 | `:1524,1528` | `Clamp<INT>` in `GetMaxTickRate` (DC) | `:1688,1692` | NOT-APPLICABLE | none |
| D17 | `:1552-1592` | wrap the stock "if paused release the cursor" block in `#if !defined(__EMSCRIPTEN__) && !defined(PLATFORM_SDL)` (WEB 2.12) | `Tick` `:1720-1739` identical Epic block | **APPLIES-CLEANLY** | copy the guard |
| D18 | `:1594-1643` | `#ifdef PLATFORM_SDL` whole-game capture state machine: `Want = level up && no menu`, `Menu = bShowMenu \|\| bShowWindowsMouse`, `SetMouseCapture(Want,Want,0)` on change, `[webmouse] relative-capture=…` log (WEB 2.12 → UF 2.38) | after `:1739 else WasPaused=0;` | **NEEDS-ADAPTATION** | copy, then verify against HP2's menu stack: HP2 menus are UWindow-based (`Source/UWindow`) plus the HP2 `FrontEnd`; they must set `bShowWindowsMouse`/`bShowMenu` for `Menu` to be 1 — and NSDLDrv must set `bWindowsMouseAvailable` (drivers report). Check with the `[webmouse]` log line when opening the in-game menu |
| D19 | `:1822-1832` | `GFileManager->MakeDirectory( *GSys->SavePath, 1 )` — Tree=1 (UF `1546db7`, 2.35) | **12 calls**: `:250-251`, `:445-446`, `:470-471`, `:486-487`, `:1767-1768`, `:1972-1973`, each `MakeDirectory(SavePath,0)` + `MakeDirectory(SaveSlotPath,0)` | **NEEDS-ADAPTATION** | set Tree=1 on every `SaveSlotPath` call (the nested `Save/SlotN` is exactly the case 2.35 fixed; with `-fno-exceptions` a failed mkdir turns the next `CreateFileWriter` NULL into an abort). The `SavePath` calls can stay or also go to 1 |
| D20 | `:1846-1847` | `SaveGame`: hoist `INT i;` (DC-GCC) | `SaveGame :1990-2001` (`for( INT i=0;…)` then `i++` in the `while`) | APPLIES-CLEANLY | hoist |
| D21 | — (HP2-only) | — | `Tick :1760-1762`: `APlayerPawn* Player = Client->Viewports(0)->Actor; if( Client && Client->Viewports.Num() && Player->bQueuedToSaveGame )` — dereferences `Client`/`Viewports(0)` **before** the check and never tests `Player` | **HP2-own bug** | reorder: test `Client && Client->Viewports.Num() && Client->Viewports(0)->Actor` first. Dedicated server/`ucc server` (no `Client`) and the post-`DissociateViewports` window the template's D4 guard describes both crash here with exceptions off |

### 5.E UnAudio.cpp

Method note: to separate the Dreamcast noise from real changes I re-ran the E-T diff after evaluating every
`#ifdef PLATFORM_DREAMCAST` / `PLATFORM_LOW_MEMORY` block as false (throw-away `undc.py`, Appendix A). For
`UnAudio.cpp` 222 changed lines collapse to **25**, all cosmetic.

| # | TPL line | Change (tag) | HP2 site | Class. | ACTION |
|---|----------|--------------|----------|--------|--------|
| A1 | `:99-106` | DC "drop RAM copy after AICA upload" (commented-out `Empty()`) | HP2 `FSoundData::Load` `:23-120` (433 rewrite: streaming handles, lip-sync) | NOT-APPLICABLE | none |
| A2 | `:124-139` | `FSoundData::GetPeriod`: read `pChannels/pBitsPerSample/pWaveDataSize` into locals (DC: `__builtin_memcpy` for unaligned; non-DC: plain derefs) | HP2 `:122-147` — HP2 adds a `FileType=="PS2"` branch (period from header via `appMemcpy`) | NOT-APPLICABLE | none (x86 and wasm32 tolerate unaligned loads; `ReadWaveInfo`'s pointers are into a byte array) |
| A3 | `:152-159`, `:733-737` | `USound::Serialize`/`UMusic::Serialize`: set `OriginalSize = Data.Num()` when loading (DC memory accounting) | HP2 `USound::Serialize :150-191` is a different format (`CoreFlags`, `Duration`, `raw_NumSamples` (Ver>76), `raw_BitsPerSample/raw_NumChannels` (>77), `raw_SampleRate` (>78), `LipSyncData` if `SF_HasLipSync`); HP2 has **no `UMusic`** (music is Ogg streamed by the audio driver, CLAUDE.md) | NOT-APPLICABLE | none. Never touch HP2's `USound::Serialize` field order — it is the `.uax` on-disk contract for package version 79 |
| A4 | `:191-292`, `:373-433` | DC re-implementation of `FWaveModInfo::ReadWaveInfo`/`UpdateWaveData` with `appMemcpy` chunk walking | HP2 `ReadWaveInfo :777-947` has its **own** `#if __PSX2_EE__ … #else … #endif` split (`:794/869/940`) | NOT-APPLICABLE | none; make sure `__PSX2_EE__` is never defined (CLAUDE.md "Do NOT") |
| A5 | `:714-728` | `PLATFORM_LOW_MEMORY` music seek-skip | – (no UMusic) | NOT-APPLICABLE | none |

**HP2-own UnAudio.cpp GCC hazards (not in the template, must be fixed for the build):**

| # | HP2 file:line | Code | Problem | ACTION |
|---|---------------|------|---------|--------|
| A6 | `UnAudio.cpp:576-587`, `:605-609` in `USound::CompressXA` | `#ifdef _WINDOWS … void *hProcess = appCreateProc(…); #else check(0); #endif` then, **unconditionally**, `while (!appGetProcReturnCode(hProcess, &iRet))` | on every non-`_WINDOWS` build `hProcess` is undeclared → **compile error**. `_WINDOWS` is defined only by the MSVC project (`Engine.dsp:45-46,73-74` `/D "_WINDOWS"`); **MinGW does not define it either**, so all three port targets fail here | Wrap the whole body of `CompressXA` (editor-only: spawns `sx -eaxa_blk`, deletes with `cmd /c del`) in `#if _MSC_VER` … `#else return false; #endif`. Runtime never calls it (only the editor's sound import) |
| A7 | `UnAudio.cpp:628-764` `USound::PS2Convert` | `#if HAVE_VAG` | `Inc/UnAudio.h:17 #define HAVE_VAG 0` — already neutralised (CLAUDE.md) | ALREADY-PRESENT; keep `HAVE_VAG 0`, do not link `Thirdparty/ENCVAG` |
| A8 | `Inc/UnAudio.h` (E-H 208) | HP2's own `UAudioSubsystem` virtual face | not an Engine-compile problem; it is the contract NOpenALDrv/SoundDrv-null must implement | drivers report (`template-drivers.md`) |

### 5.F UnTrace.cpp / UnDynBsp.cpp / UnModel.cpp (the Dreamcast SWORD-BSP set)

After removing the DC blocks: `UnTrace.cpp` → 3 changed lines, `UnDynBsp.cpp` → 6, `UnModel.cpp` → 5. Everything
else in these three files (≈300 lines) is `#ifdef PLATFORM_DREAMCAST (INT)Node.iX #else Node.iX #endif`.

| # | TPL line | Change (tag) | HP2 site | Class. | ACTION |
|---|----------|--------------|----------|--------|--------|
| F1 | `UnTrace.cpp:191` | `for( INT i=0; i<NumHulls; i++ )` in the `CLIP_COLLISION_PRIMITIVE` macro's 2nd loop (DC-GCC) | HP2 `UnTrace.cpp:170` `for( i=0; i<NumHulls; i++ ) \` (1st loop `:154 for( int i=0;…)`) | **APPLIES-CLEANLY** | add `INT` at `:170` |
| F2 | `UnTrace.cpp:569-573` | DC branch **and the non-DC `#else` branch both lost `if( Use[FrontFirst] )`** before `BoxLineCheck( iNode, Node.iChild[FrontFirst], …)` in `FBoxLineCheck` (DC `a5e2533`) | HP2 `UnTrace.cpp:522` still has `if( Use[FrontFirst] )` | **DO-NOT-PORT** | keep HP2's line. The template's edit is an all-platform regression (traverses the near child even when the swept box cannot touch it — correct results, wasted work; a latent divergence from Epic). Never copy `engine-ut99/…/UnTrace.cpp` wholesale; if a whole-file copy is ever tempting, run the `undc.py` diff first |
| F3 | `UnTrace.cpp` rest (≈120 lines) | `(INT)` casts for SWORD BSP indices | – | NOT-APPLICABLE | none |
| F4 | `UnDynBsp.cpp:273-274` | hoist `FActorLink* Link;` (DC-GCC) | HP2 `UnDynBsp.cpp:274` identical | APPLIES-CLEANLY | copy |
| F5 | `UnDynBsp.cpp:307-308` | hoist `INT i;` before `for( i=0; i<Brush->Polys->Element.Num()…)` (DC-GCC) | HP2 `:303` `for( INT i=0;…)` followed by `:308`, `:320` `for( i=0;…)` (HP2 added a third loop) | **APPLIES-CLEANLY** | hoist once at `:303`; covers `:308` and `:320` |
| F6 | `UnDynBsp.cpp` rest | `ExpandDb(…, MAXSWORD)`, SWORD `iRenderBound` writes, `goto FilterLoop` with casts (DC) | – | NOT-APPLICABLE | none |
| F7 | `UnModel.cpp:19-20` | split `Ar << Surf.PolyFlags << AR_INDEX(Surf.pBase) << …` into two statements | HP2 `UnModel.cpp:19` identical | NOT-APPLICABLE (cosmetic) | none — left-to-right evaluation of chained `<<` is guaranteed from C++17 and the operands have no side effects |
| F8 | `UnModel.cpp:102` | `for( INT i=0; i<_NumZones; i++ )` (DC-GCC) | HP2 `UnModel.cpp:150,175` already `for( INT i=0; i<NumZones; i++ )` | **ALREADY-PRESENT** | none |
| F9 | `UnModel.cpp` rest, `UnModel.h` | DC serialisation of SWORD node/surf fields; `MAX_NODES` (see I3) | – | NOT-APPLICABLE | none |

_(5.G pending)_

## 6. Template-only files (added by the template, absent in Epic)
_(pending)_

## 7. HP2-only Engine files — portability notes
_(pending)_

## 8. `__asm` / `#if ASM` / `_MSC_VER` / `WIN32`-only blocks in HP2 Engine
_(pending)_

## 9. Windows headers in HP2 Engine, and the CMake source list
_(pending)_

## 10. `EngineClasses.h` — generated? matches `Assets/System/Engine.u`?

### 10.1 It is generated (never hand-edit except the one documented pad)
`Source/Engine/Inc/EngineClasses.h:1-4`:
```cpp
/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
```
The generator is HP2's own `ucc make` (`Source/Editor/Src/UMakeCommandlet.cpp:208-214`: "Autogenerating C++ header:
`..\<Pkg>\Inc\<Pkg>Classes.h`", unless `-NOHEADER`) driving `UnEdExp.cpp:276-300` (the `H` exporter). The
exporter's packing rule is the root cause of every GCC layout hazard below — it emits `GCC_PACK(4)` only on
the first bool of a bool run and on the first non-bool after a run, **within one class**:
```cpp
// Source/Editor/Src/UnEdExp.cpp:289-296
if (It->IsA(UBoolProperty::StaticClass()))
{   if (LastIt == NULL || !LastIt->IsA(UBoolProperty::StaticClass()))
        Ar.Logf( TEXT(" GCC_PACK(%i)"), PROPERTY_ALIGNMENT );
} else {
    if (LastIt != NULL && LastIt->IsA(UBoolProperty::StaticClass()))
        Ar.Logf( TEXT(" GCC_PACK(%i)"), PROPERTY_ALIGNMENT );
}
```
`LastIt` restarts at NULL for every class, so a subclass whose first member is a `BYTE` never gets the
alignment that MSVC's "a bitfield run owns a whole DWORD" rule gives it for free.

**ACTION:** we do **not** recompile `Engine.u` (owner decision), so we do **not** regenerate this header either.
Treat it as frozen data; the only permitted hand edits are layout pads/alignments (§10.4), each with a comment
pointing here, exactly as the template did at `engine-ut99/Source/Engine/Inc/EngineClasses.h:1865-1886`.

### 10.2 Does it match the shipped `Engine.u`? — **Yes, exactly** (verified)
Method: the shipped `Assets/System/Core.u` + `Engine.u` (both package version **79**, licensee 0; Engine.u has
4,260 names / 69 imports / 6,787 exports) were parsed; for every class the `Children→Next` field chain was
walked and HP2 433's own `UStruct::Link` rules were replayed (`Core/Src/UnClass.cpp:353-395`,
`Core/Src/UnProp.cpp` `*::Link`: Byte=1/1, Int/Float/Object/Class/Name/Bool=4/4, Str/Array=12/4,
Struct=`PropertiesSize` aligned 1/2/4, FixedArray=inner×count, consecutive bools merge into one DWORD in classes,
never across a class boundary, `PropertiesSize` aligned to 4). Results:

| Check | Result |
|-------|--------|
| Classes in `Engine.u` vs `Source/Engine/Classes/*.uc` | 127 = 127, identical name sets (`Engine.upkg` is the 128th file) |
| Native classes in `EngineClasses.h` | 59 (`AActor`…`ULevelSummary`), every one exists in `Engine.u` |
| Property **names and order** per class (header member list vs `.u` link order) | identical for **all 59** |
| `sizeof` under the **MSVC/Zp4 model** (what Epic/KnowWonder's retail build used) vs script `PropertiesSize` | identical for **all 59** — the header is the one this `Engine.u` was built with |
| Per-property offsets under the **MSVC model** | identical for all 59 |
| Per-property offsets under the **GCC/clang model** (native Linux, wasm) | **2 classes wrong: `AMover`, `APlayerStart`** (§10.4) |

Script sizes of the classes that matter most (the numbers the layout guard prints as `script layout=`):

| Class | Script `PropertiesSize` | | Class | Script `PropertiesSize` |
|-------|------:|-|-------|------:|
| Object (Core) | 40 | | LevelInfo | 1340 |
| Actor | 684 | | ZoneInfo | 1028 |
| Pawn | 1512 | | GameInfo | 1764 |
| PlayerPawn | 2396 | | NavigationPoint | 992 |
| Brush | 776 | | PlayerStart | 1000 |
| Mover | 1320 | | Inventory / Weapon | 900 / 1108 |
| ParticleFX | 1052 | | Decoration / Carcass | 720 / 744 |
| Texture | 192 | | Canvas / Console / Player | 132 / 2072 / 84 |

Hand-written native classes that also carry script properties (not in `EngineClasses.h`; C++ in `UnPlayer.h`,
`UnCamera.h` (`UCanvas`), `UnCon.h`, `UnTex.h` (`UBitmap`,`UTexture`,`UPalette`), `UnEngine.h`,
`UnRenderIterator.h`, `UnScrTex.h`): all **match** under both models (UPlayer 84, UCanvas 132, UConsole 2072,
UBitmap 84, UTexture 192, UPalette 52, UEngine 120, URenderIterator 56, UScriptedTexture 216). Names differ in a
few places but offsets coincide (`UCanvas::Color`↔`DrawColor`, `X/Y`↔`SizeX/SizeY`, `Frame`↔`FramePtr`;
`UTexture::PolyFlags` DWORD ↔ the 32 script bools `bInvisible…bX7` at 128; `UPlayer` has one extra C++ bool
`bSoftwareRendering` inside the same DWORD; `UScriptedTexture`'s private `OldSourceTex/LocalSourceBitmap/PaletteMap`
↔ script `Junk1..3`). HP2 433 has **no script class** for `GameEngine, Viewport, Client, Level, Model, Mesh,
Font, Sound, Music, AudioSubsystem, RenderDevice, NetDriver, Primitive…` — they are C++-only, so their layout
is unconstrained by `Engine.u`.

⚠ Model caveat: the "GCC model" is an emulation, not a compile (HP2's raw `Core/Inc/UnGnuG.h` does not compile
with modern GCC yet — 190 errors, the Core report's job). It was **validated against every documented template
observation**: run on the template header + UT99 v400 `Engine.u` it predicts exactly (a) the v400 mover bug
offsets `script 608/611/613 vs C++ 605/608/610` (commit `0f620a2`), (b) the MinGW double-pad
`Brush: C++ sizeof=612 but script layout=608` (commit `301a9ca`), and (c) the "pre-existing v400 PlayerStart"
layout warning the template still logs on Linux (`301a9ca`: "the only layout warning is the pre-existing v400
PlayerStart one"). A real compile of a 20-line test (`g++ -m32 -fpack-struct=4`, `em++`, `i686-w64-mingw32-g++`)
reproduced the tail-padding placement on all three targets:
```
gcc -m32   nopad: sizeof(ABrush)=16 Enc=13 KeyNum=15 NumKeys=17   pad: Enc=16 KeyNum=18 NumKeys=20
em++       nopad: sizeof(ABrush)=16 Enc=13 KeyNum=15 NumKeys=17   pad: Enc=16 KeyNum=18 NumKeys=20
mingw      nopad: sizeof(ABrush)=16 Enc=16 KeyNum=18 NumKeys=20   (MSVC bitfields: no pad needed)
```

### 10.3 Other generated-header facts HP2 must know
- `#if _MSC_VER #pragma pack (push,4)` (`EngineClasses.h:6-8`, pop at the end) — GCC does not see it; the
  template's answer is the global `-fpack-struct=4` (TPL `CLAUDE.md`: "`-fpack-struct=4` (reflected class sizes)").
  **ACTION:** HP2's CMake must pass `-fpack-struct=4` to every package (Core report / build skeleton). On i386
  and wasm32 it changes nothing for these classes (no 8-byte members) but it is load-bearing for MinGW (doubles).
- `#endif NAMES_ONLY` (last lines of HP2's header) — GCC/clang warn `extra tokens at end of #endif`; the template
  changed it to `#endif`. **ACTION:** APPLIES-CLEANLY (cosmetic).
- 13 `BYTE x GCC_PACK(4)` members (HP2 lines 822, 862, 960, 978, 985, 2063, 2613, 2660, 2810, 3102, 3212, 3553,
  3664) — GCC warns `'packed' attribute ignored for field of type 'BYTE'`; the `aligned(4)` half still applies
  so the layout is identical (verified by compile, test A1/A2: identical offsets). The template rewrote its 9
  sites to `GCC_ALIGN(4)` (`cbdb054`). **ACTION:** optional warning cleanup; if done, do all 13 (HP2 has 4 more
  than UT99: `StandingCount`, `LightSource`, `bMouseAltFire`, `TellOtherTrigger`). `GCC_ALIGN` is already defined
  in HP2 `Core/Inc/UnGnuG.h:87`.

### 10.4 The two layout defects the port MUST fix (GCC + clang only, not MinGW/MSVC)

| # | Base (trailing bool run) | Subclass first member | GCC model | Script | Size guard sees it? | C++ that touches it |
|---|--------------------------|-----------------------|-----------|--------|---------------------|---------------------|
| L1 | `ABrush` ends `BITFIELD bColored:1 GCC_PACK(4);` (`EngineClasses.h:2276`) | `AMover::MoverEncroachType` BYTE (`:2314`) | `MoverEncroachType..BrushRaytraceKey` at **773-780** | **776-783** | **NO** (`MoveTimes[8]` realigns at 784, sizeof(AMover)=1320 both) | `UnLevAct.cpp:844-853` (encroach type), `UnActor.cpp:816` (`KeyNum` replication), `UnScript.cpp:167,205` (`KeyNum`,`MoverGlideType`) — every multi-key mover, exactly the v400 bug |
| L2 | `ANavigationPoint` ends with an 8-bool run `bPlayerOnly…bTwoWay` (`:3391-3398`) | `APlayerStart::TeamNumber` BYTE (`:3490`) | `TeamNumber` 989, bools 992, sizeof 996 | 992, 996, **1000** | **yes** (`PlayerStart: C++ sizeof=996 but script layout=1000`) | `UnGame.cpp:817-818` writes `PS->bEnabled=false` (clears bit 2 of the script `TeamNumber` byte instead) |

Other trailing runs are harmless because the subclass's first member is 4-aligned: `APickup→AAmmo` (INT),
`AStatLog→AStatLogFile` / `ADecoration→ACarcass` (first member is itself a `GCC_PACK(4)` bool),
`ANavigationPoint→ALiftExit/ALiftCenter/AWarpZoneMarker/AInventorySpot/ATeleporter` (FName/pointer/FString).
`AParticleFX`, `AInterpolationManager`, `APlayerStart` end in bool runs but have no native subclass.

**ACTION L1 (copy the template verbatim, adapting only the `DECLARE_CLASS` arity):**
```cpp
    INT PolyFlags;
    BITFIELD bColored:1 GCC_PACK(4);
    // <template comment from engine-ut99 EngineClasses.h:1865-1882, retargeted: script 776/779/781 vs C++ 773/776/778>
#ifndef __MINGW32__
    BYTE bColoredPad[3];
#endif
    DECLARE_CLASS(ABrush,AActor,0,Engine)
```
**ACTION L2 (template does NOT fix this — HP2 should):** same pattern after `BITFIELD bTwoWay:1;` in
`ANavigationPoint` — `#ifndef __MINGW32__ BYTE bTwoWayPad[3]; #endif` (8 bools = 1 byte under GCC, so 3 pad
bytes; `sizeof(ANavigationPoint)` stays 992 under every model so no other subclass moves). Equivalent
one-line alternative that needs no MinGW guard: `BYTE TeamNumber GCC_ALIGN(4);` in `APlayerStart`. Prefer the pad
for symmetry with L1. **Verify** with the guard (§11): zero `[layout]` warnings, and add a per-property check
for Mover (§11.3).


## 11. The 2.32 layout guard and how to verify C++ class sizes against script sizes

### 11.1 Where the guard is in the template (quote)
It lives in **Core**, not Engine: `engine-ut99/Source/Core/Src/UnClass.cpp`. Recording (native constructor,
`:295-302`):
```cpp
{
	// Layout guard: record the native C++ sizeof (InSize) keyed by object pointer, so UStruct::Link can
	// compare it against the size derived from the LOADED .u property offsets and warn on any C++/script
	// disagreement. Plain globals (BSS) -> safe at the static-init time these native constructors run.
	extern UStruct* GLayoutStructs[]; extern INT GLayoutSizes[]; extern INT GLayoutCount;
	if( GLayoutCount < 4096 ) { GLayoutStructs[GLayoutCount]=this; GLayoutSizes[GLayoutCount]=InSize; GLayoutCount++; }
}
UStruct* GLayoutStructs[4096]; INT GLayoutSizes[4096]; INT GLayoutCount=0;
```
Checking (end of the `Props` branch of `UStruct::Link`, `:364-385`, right after `PropertiesSize = Align(PropertiesSize,4);`):
```cpp
		// Layout guard (ported from feat/v436-compat, where it was a GENERAL fix living on a
		// target-specific branch): a native class's C++ sizeof must equal the size derived from the
		// loaded .u property offsets. A mismatch means the class's own members — and any native
		// subclass's — are read from the wrong offsets.
		//
		// ⚠ IT IS NECESSARY BUT NOT SUFFICIENT, and step 2.32 is the proof: the ABrush/AMover keyframe
		// bug passed this check. C++ may place a derived class's members in the base's TAIL PADDING, so
		// sizeof(base) is unchanged while the derived member moves, and AMover re-aligned before its end
		// so its size matched too. Only a per-PROPERTY offset comparison sees that class of bug.
		{
			extern UStruct* GLayoutStructs[]; extern INT GLayoutSizes[]; extern INT GLayoutCount;
			for( INT li=0; li<GLayoutCount; li++ )
				if( GLayoutStructs[li]==this )
				{
					if( GLayoutSizes[li] != PropertiesSize )
						debugf( NAME_Warning, TEXT("[layout] native %s: C++ sizeof=%i but script layout=%i -- member-offset shift risk"),
							GetName(), GLayoutSizes[li], PropertiesSize );
					break;
				}
		}
```

### 11.2 Porting it to HP2 (NEEDS-ADAPTATION — Core file, Engine consequence)
HP2 `Core/Src/UnClass.cpp` has **two** native-class constructors, and which one runs depends on how the port
defines `IMPLEMENT_CLASS` (`Core/Inc/UnObjBas.h:348-367` MSVC and `:368-385` GCC-DLL use `EC_NativeConstructor`;
`:386-…` `__STATIC_LINK` uses `EC_StaticConstructor` + `::new UClass`):

| Insertion | HP2 location | What |
|-----------|--------------|------|
| record | `UStruct::UStruct( ENativeConstructor, … )` body, `UnClass.cpp:289-302` (currently `{}`) | the template's 3 lines |
| record | `UStruct::UStruct( EStaticConstructor, … )` body, `UnClass.cpp:304-317` (currently `{}`) | the same 3 lines — **needed if the port keeps HP2's `__STATIC_LINK` `IMPLEMENT_CLASS`** |
| check | `UStruct::Link`, immediately after `PropertiesSize = Align(PropertiesSize,PropertiesAlign);` at `UnClass.cpp:394` | the template's block, unchanged |

Note HP2's `Link` differs from v400's in two harmless ways: it inherits `PropertiesSize` unaligned
(`= GetInheritanceSuper()->PropertiesSize`, `:368`, the super's is already aligned) and aligns to
`PropertiesAlign` (4 on PC; 16 only under `__PSX2_EE__`, which the port must leave undefined). The `EStaticConstructor`
path constructs the `UClass` with `::new` at first `StaticClass()` call rather than at static-init time, so
`GLayoutStructs` is still populated before any `.u` is linked. Expected output once L1+L2 (§10.4) are applied:
**zero `[layout]` warnings** on native Linux and web; without L2 exactly one: `[layout] native PlayerStart: C++
sizeof=996 but script layout=1000`. L1 is **invisible** to this guard (that is its whole lesson).

### 11.3 A stronger check the template does not have: compile-time per-property asserts
Because the header must match a **frozen** `Engine.u`, every script offset is a constant known today. Generate
one assert per non-bitfield property (bitfield runs are covered by the member that follows them / the next
class's first member) plus one `sizeof` assert per class, from the shipped `.u`, and compile it into Engine:
```cpp
// Source/Engine/Src/EngineLayoutAsserts.cpp  (GENERATED from Assets/System/{Core,Engine}.u — 1,063 asserts)
#include "EnginePrivate.h"
#define UE_OFS(C,M) __builtin_offsetof(C,M)
static_assert(UE_OFS(AMover,MoverEncroachType)==776, "AMover::MoverEncroachType must be at script offset 776");
static_assert(UE_OFS(AMover,KeyNum)==779,            "AMover::KeyNum must be at script offset 779");
static_assert(UE_OFS(AMover,NumKeys)==781,           "AMover::NumKeys must be at script offset 781");
static_assert(UE_OFS(APlayerStart,TeamNumber)==992,  "APlayerStart::TeamNumber must be at script offset 992");
static_assert(sizeof(APlayerStart)==1000,            "sizeof(APlayerStart) must equal script PropertiesSize 1000");
/* … one line per property of all 59 classes … */
```
Validated on a reduced ABrush/AMover model: **fails** under `g++ -m32 -fpack-struct=4` and `em++` without the
pad, **passes** with it (`-Wno-invalid-offsetof` silences the "non-standard-layout" warning; `__builtin_offsetof`
is a constant expression in both GCC and clang). It needs no game data at runtime, no boot, and it also guards
MinGW (where the pad is `#ifndef`'d out). **ACTION (recommended, not template-mandated):** add the generator
(Appendix A `emit_asserts.py`) under `tools/dev/`, commit its output, compile it on every target. Keep the
runtime guard as well (it also covers Core/Fire/IpDrv/driver classes that have no generated asserts).

### 11.4 Manual one-shot verification (what 2.32 actually did)
If a symptom appears before the asserts exist: in any post-load hook, for a class `C` and property `P`,
log `(BYTE*)&((C*)Obj)->P - (BYTE*)Obj` next to `FindField<UProperty>(C::StaticClass(),TEXT("P"))->Offset`.
That is the "C++ address vs the loaded .u Offset" dump from commit `0f620a2`; it is the only runtime check that
sees tail-padding shifts. Offline, `compare.py` (Appendix A) reproduces the whole table without building.


## 12. Action checklist (ordered)
_(pending)_

## 13. Open questions / how to verify
_(pending)_

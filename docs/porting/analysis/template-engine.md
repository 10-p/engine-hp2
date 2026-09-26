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
_(pending)_

## 5. Hunk-by-hunk: Engine/Src
_(pending)_

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

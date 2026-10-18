Record — kept as history; the current state is in README.md, BUILD.md and the change inventory.

# Render, Fire, SoftDrv and the stripped Editor — template (UT99 v400 web) vs epic v400 vs HP2 build 433

Analyst key: `template-render-fire-softdrv-editor` · run 2 (Opus), 2026-09-26 · read-only analysis: no file
outside this report was modified; no cmake/make/compiler was run. Everything below is from `diff`, `grep`,
reading the sources, and two small scripts (a for-scope-leak scanner and a UE1 package-table reader, described
where used). This run supersedes the 2026-09-25 report at this path; where the two disagree, this one was
re-verified and says so ("correction").

| Alias | Path |
|-------|------|
| **T** (template, engine-ut99) | `/home/ion/engines/engine-ut99/Source` |
| **E** (epic v400, common ancestor) | `/home/ion/epic-sources/ut99-v400` |
| **H** (HP2 build 433, the port target) | `/home/ion/hp2-ufrontend/HarryPotter2/Source` |

Diffs: `diff --strip-trailing-cr -w -u -a` (E is CRLF; T and H are LF; `-a` because a few HP2 files carry
Latin-1 bytes in comments). "Δ" = number of `<`/`>` lines of a plain `diff -w`.

Classification legend used in every table: **PORT** = re-apply to HP2's text by hand · **PORT-IF** = apply
only if GCC complains / only if a precondition holds · **HAVE** = HP2 already has it or the site no longer
exists · **SKIP** = Dreamcast / LOW_MEMORY / PSX2, not wanted (PLAN §1 rule 2).

## 0. Executive summary

Run 2 finished §1-§6 below. It re-verified run 1 and supersedes it; Appendix Z (run 1's text) is kept
only as history.

1. **Render: HP2's text is the base; re-apply the template's hunks by hand (§1).** The template's Render work
   is GCC portability (for-scope hoists: 29 sites in HP2, §1.5; `Min<INT>`-style casts needed only on Dreamcast; the deleted
   MSVC/AT&T asm in `Pipe()`) plus two web fixes. The most important one is
   **`HackRaster[2880]` → `MAX_RASTER_Y` 8192 with a clamp in `SetupRaster`** (HP2 `UnRender.cpp:1977`):
   without it a tall canvas corrupts the heap on every renderer. The second, the AutoFOV swap, is needed
   only if Engine AutoFOV is ported. Every ASM/PSX2 gate in HP2 Render has a plain-C path, but only once
   the template's `UnGnuG.h` sets `ASM`, `ASMLINUX` and `ASM3DNOW` to 0 (§1.3 table). Drop `UnTest.cpp`
   from the CMake list and delete the duplicate `Src/UnSpan.h` and `Src/Amd3d.h`.
2. **New functional bug: HP2's GCC-only `FLightManager::Merge` has no dark-light branch** (`UnLight.cpp:847-1044`,
   the Loki copy behind `#else` of `#if _MSC_VER`). A GCC or clang build would make every `Darklight`
   brighten instead of darken, on all renderers. `Adv1Willow` alone has 199 Darklights. Fix: delete the
   `#if _MSC_VER` line and the whole GCC copy, and keep the MSVC copy, whose `#else` is the C loop (§1.3).
3. **Fire (§2):** all five ASM families have complete C fallbacks at `ASM=0`, and dropping the `.asm` files
   is correct. Port three things:
   - the for-scope fixes (`t`, `Y`, `Flash`);
   - the MinGW `__int64` typedef guard;
   - **mandatory:** `GCC_ALIGN(4)` on `SparkType`, `FX_Heat` and `DropType`. HP2 content loads
     `FireTexture`, `WetTexture`, `IceTexture` and `WaveTexture` at map load, and
     `VERIFY_CLASS_OFFSET`/`VERIFY_CLASS_SIZE` makes any layout mismatch fatal.
4. **SoftDrv (§3): start from the template's nine files and re-apply HP2's 433 interface** (125 changed
   lines: the 5-arg `DrawComplexSurface`, which reads the passed `PolyFlags` so Lumos surfaces work,
   `MaxVertices()` = 256, `DrawTriangles` with indexed, list and fan branches, the 4-arg `DECLARE_CLASS`,
   and the `Render.h` include). §3.4 gives exact per-file edits. Two adaptations:
   - `VolumetricLightingMeshes` becomes a SoftDrv-local config bool;
   - mesh fog is gated on `PF_RenderFog`, because HP2 particles leave `Fog` uninitialised.

   SoftDrv needs no `GetOsSurface` override (only WinDrv calls it).
5. **Editor (§4): must be linked into the game on every target.** `Entry.unr` (loaded at boot) imports
   `Editor.B_PlyrOn`. `Editor.u` then needs the native class `Editor.TransBuffer`. The Quidditch maps import
   `Editor.Bad`. I scanned all 194 packages. For the CMake list, take HP2's current list and **remove
   `UMasterCommandlet.cpp`**. `EdHook.cpp`, `TTFontImport.cpp`, `UPS2ConvertCommandlet.cpp` and the
   0-byte `UPSX2MusicCommandlet.cpp` stay out. Compile fixes:
   - the template's ED1-ED16 (§4.2);
   - two new ones: `__declspec(dllexport)` globals at `UnEdCam.cpp:49,65-69`, and a fourth `wcscmp` at
     `UnEdCam.cpp:2442`;
   - 72 scanner-listed for-scope sites (§4.6).
6. Cross-report prerequisites are in §5 (D1-D16). The critical path is: template `UnGnuG.h` (D1), then
   `appProcessSeconds` (D2), then `PUSH_HIT` (D3), then `UNREAL_STATIC` (D5). Found here but owned elsewhere:
   HP2's `NullDrv` still uses the UT99 render-device interface (D8, already in `template-drivers.md`).

## 1. Render

### 1.0 File inventory (T / E / H)

| File | E lines | T Δ vs E | H lines | H Δ vs E | Notes |
|------|--------:|---------:|--------:|---------:|-------|
| `Src/Render.cpp` | 18 | 0 | 18 | 2 | H: `#include "Render.h"` only. (Correction to run 1: `RENDER_API FMemStack GDynMem, GSceneMem;` is already in epic's `Render.cpp:12` — not a 433 addition.) |
| `Src/RenderPrivate.h` → H `Inc/Render.h` | 540 | 9 | 468 | 182 | H moved the private header to `Inc/` and renamed it (see §1.2) |
| `Src/UnLight.cpp` | 2290 | 44 | 2638 | 1160 | |
| `Src/UnMeshRn.cpp` | 671 | 6 | 760 | 323 | |
| `Src/UnMeshRnLOD.h` | 692 | 11 | 789 | 561 | |
| `Src/UnRandom.cpp` | 137 | 2 | 137 | 2 | |
| `Src/UnRender.cpp` | 3432 | 328 | 3922 | 1178 | |
| `Src/UnSoftLn.cpp` | 182 | 0 | 525 | 371 | H adds `SolveQuadratic`, `DrawCylinder`, `DrawBox(FBox,FCoords)`; Latin-1 `²`/`±` bytes in comments only (harmless) |
| `Src/UnSpan.cpp` | 711 | 2 | 711 | 2 | |
| `Src/UnSprite.cpp` | 956 | 16 | 1164 | 406 | |
| `Src/UnSpan.h`, `Src/Amd3d.h` | 115 / 515 | 0 | 115 / 515 | 0 | H has byte-identical copies in **both** `Inc/` and `Src/` (`cmp` = identical) |
| `Src/UnTest.cpp` | 30 | 0 | 30 | 2 | not built by T (see §1.4) |
| `Src/UnParticleRn.cpp` | — | — | 1275 | HP2-only | DreamWorks Interactive particle renderer; no ASM/`_MSC_VER`; only `#ifdef DEBUG_CLIPPING` (commented-out define) |
| `Inc/UnRenderNative.h` | — | — | 20 | HP2-only | `#if __STATIC_LINK … AUTO_INITIALIZE_REGISTRANTS_RENDER` — dead under the template's `UNREAL_STATIC` scheme (PLAN §2 keeps `__STATIC_LINK` undefined) |

### 1.1 Template vs epic — every hunk, what/why, and what HP2 must do

**Key insight about the `Min<INT>`/`Max<INT>`/`Clamp<INT>` hunks (R3, R7, R13, F4/F5 in Fire):** they are
not needed on any of *our* three targets. T's `Core/Inc/UnGnuG.h:167` is `typedef int32_t INT;`; on i386
glibc, wasm32 (Emscripten/musl) and i686-w64-mingw32, `int32_t` **is** `int`, so `Max(0, appFloor(x))`
deduces fine. They exist because ut99dc also targets the Dreamcast (sh-elf newlib), where `int32_t` is
`long int` and every `Max(INT-literal, INT-typed)` mix fails template deduction. Same story for R17
(`int`→`INT` in `FSpanBuffer::AllocIndex`). They are harmless, so porting them is fine, but they are
**PORT-IF** (apply where GCC/clang actually complains), not a checklist to grind through. The for-scope
hoists, by contrast, are real errors on every conforming compiler.

| # | T file:line (post-change) | What T changed | Why | HP2 site | HP2 action |
|---|---------------------------|----------------|-----|----------|-----------|
| R1 | `RenderPrivate.h:365-372` | added `#elif defined(PLATFORM_DREAMCAST)` (`frsqrt`/`fsqrt`) and `#elif defined(PLATFORM_LOW_MEMORY)` (`1.f/sqrtf`) before the generic `#else` of `DivSqrtApprox/DivApprox/SqrtApprox` | DC / LOW_MEMORY | `Inc/Render.h:234-280` (`#if ASM` POWER_ASM … `#else` 276 … `#endif` 280) | **SKIP.** At `ASM=0` HP2 already compiles epic's plain `#else`: `DivSqrtApprox = 1.0/appSqrt(F)` etc. Optional hygiene: delete the `#if ASM` MSVC `__asm` half |
| R2 | `UnLight.cpp:340`, `:393` | `INT i;` hoisted before `for( i=0; …)` in the table-setup code | MSVC6 for-scope leak (`i` came from an earlier `for( INT i…)`) | `UnLight.cpp:360`, `:412` | **PORT** (`INT i;` before each loop) |
| R3 | `UnLight.cpp:1167, 1440-42, 1460-63, 1489-92, 2097, 2205, 2210` | `Max/Min/Clamp(…)` → `…<INT>(…)` (cylinder spatial, palette build, `MinU…MaxV` clip, `Delta`, `SpecialTag` decay) | DC `int32_t==long` (see insight) | `UnLight.cpp:1762-1764, 1782-1785, 1811-1814, 2534, 2539` (same expressions); H's `Delta` (`:2427`) is already `Clamp(appRound(…),0,255)` | **PORT-IF** (harmless; do it while in the file) |
| R4 | `UnLight.cpp:1628-29, 1650-51, 2237-38`; `:2114-15` | `FLightInfo* Find;` hoisted out of the search loop `for(…;…;Find++);` whose result is used after it; `INT NewNum=0;` hoisted out of `for( INT i=0,NewNum=0; …)` | for-scope leak | `UnLight.cpp:1950/1951`, `1971/1972`, `2566/2567` (`Find`); `2444/2451` (`NewNum`) | **PORT** |
| R5 | `UnMeshRn.cpp:166, 419`; `UnMeshRnLOD.h:176, 250, 308, 600` | `INT i;` hoists | for-scope | H `UnMeshRnLOD.h:166`, `:283`; the H `UnMeshRn.cpp` equivalents are gone (the E:166 subdivision loop is inside H's `#ifdef MESH_SUBDIVIDE` 262-337, never defined; the E:419 loop was rewritten for `DrawTriangles`) | **PORT** in `UnMeshRnLOD.h` only |
| R6 | `UnMeshRn.cpp:429`; `UnMeshRnLOD.h:318` | `SortedPts[i] - (FTransform*)Samples` | pointer difference between unrelated pointee types is ill-formed (MSVC accepted it) | none — `grep MultiSkins\|SortedPts` finds no such expression in H's mesh renderers | **HAVE** (site gone) |
| R7 | `UnMeshRn.cpp:435`; `UnMeshRnLOD.h:324`; `UnSprite.cpp:770` | `Clamp<INT>( appFloor(LifeFraction()*Count), 0, Count-1 )` | DC | `UnSprite.cpp:914` | **PORT-IF** |
| R8 | `UnRandom.cpp:119` | `for( INT i=0; i<N_RANDS; i++ )` | `i` leaked from an earlier loop | `UnRandom.cpp:119` (identical) | **PORT** |
| R9 | `UnRender.cpp:95-101` | `#ifdef PLATFORM_LOW_MEMORY GDynMem.Init(32768)` | LOW_MEMORY | `UnRender.cpp:99-100` `GDynMem.Init( 65536, TEXT("GDynMem") )` (433 tagged `FMemStack::Init`) | **SKIP** |
| R10 | `UnRender.cpp:683-706` (E:678-923) | deleted the whole MSVC `__asm` body **and** both AT&T `asm volatile(" …` blocks (`ASMLINUX` clip numbers, `LINUXASM` projection) of `Pipe()`, keeping the C transform/outcode/project | no x86 asm on GCC/wasm; the AT&T blocks are pre-C++11 multi-line string literals no current GCC/clang accepts | `UnRender.cpp:888-1137` (x86 `Pipe`; `#if ASM` 897, `#if ASMLINUX` 1036, `#if LINUXASM` 1098) + PSX2 `Pipe` 871-887 | **PORT (mechanical)**: delete 871-888 (`#if __PSX2_EE__` branch + its `#else`/`#endif` 1138), 897-1034 (`#if ASM … #else`), 1036-1085 + 1090 (`#if ASMLINUX` block), 1098-1129 + 1134 (`#if LINUXASM` block). At `ASM=0/ASMLINUX=0` the C code would compile untouched anyway — deleting matches the template and removes the trap described in §1.3 |
| R11 | `UnRender.cpp:1382-83` | `FSceneNode* Frame;` hoisted in `CreateChildFrame` | `Frame` used after the search loop | `UnRender.cpp:1908` loop, use at `:1929` | **PORT** |
| R12 | `UnRender.cpp:1451-71` | `FRasterSpan HackRaster[2880]` → `HackRaster[MAX_RASTER_Y]` (`#define MAX_RASTER_Y 8192`, 480 on DC) **and** `EndY = Min<INT>( EndY, MAX_RASTER_Y - 1 );` as the first statement of `SetupRaster` | the span table is indexed by screen Y for **every** render device; a viewport taller than 1440/2880 lines overran it → heap corruption / wasm OOB (the web canvas is sized in device pixels) | `UnRender.cpp:1977` `FRasterSpan HackRaster[2880];//max y res!!`; `SetupRaster` at `:1979` has no clamp; caller `:2761` passes `Frame->Y` | **PORT** (without the DC branch). This is the most important Render hunk for the web |
| R13 | `UnRender.cpp:1491-96`, `1832-35` | `Clamp<INT>` in `SetupRaster`, `::Max<INT>/::Min<INT>` in `BoundVisible` | DC | `UnRender.cpp:2002-2007`, `2264-2267` | **PORT-IF** |
| R14 | `UnRender.cpp:2055-56, 2492-93, 2506-07, 2563, 2607-08, 2674, 2724, 2987, 3081-93` | `INT i;` / `INT i,j;` / `INT Pass;` / `FActorLink* Other;` hoists | for-scope | see §1.5 (all confirmed by the scanner) | **PORT** |
| R15 | `UnRender.cpp:3213-3255` | (a) `if( Actor && Actor->GetClass() && Frame->Viewport->Canvas && Frame->Viewport->Canvas->GetClass() )` guard around the overlay call ("happens on Dreamcast due to GCC/MSVC bitfield differences"); (b) **AutoFOV (ufront 2.29)**: while `eventRenderOverlays` runs, set `FovActor->FovAngle = GAutoFovFrameBase`, restore it afterwards and call `Frame->ComputeRenderSize()` | UT99's `Inventory.uc:239` computes the viewmodel offset as `0.9/PawnOwner.FOVAngle * PlayerViewOffset`; under AutoFOV the widened angle pulled the weapon towards the camera | `UnRender.cpp:3895-3905` (same `bBehindView ? NULL : ViewTarget ? … : Actor` selection and `eventRenderOverlays` call) | (a) **PORT** (cheap null-guard). (b) **PORT-IF the Engine AutoFOV is ported** (it needs `ENGINE_API extern FLOAT GAutoFovFrameBase;` — T `Engine/Inc/UnCamera.h:449`, `Engine/Src/UnCamera.cpp:250`; HP2 has no AutoFOV code at all today; `template-engine.md` owns the decision). HP2's `Engine/Classes/Inventory.uc:239` has the **identical** `0.9/PawnOwner.FOVAngle` line, and `Weapon.RenderOverlays` (`Engine/Classes/Weapon.uc:131`) is reachable because Harry's camera is a `PlayerCalcView` override (`HGame/Classes/Harry.uc:5369`), not `bBehindView`; so if AutoFOV lands, land (b) with it |
| R16 | `UnRender.cpp` `DrawWorld` | T keeps epic's `try { … } catch(...) { debugf("Anomalous singularity…") }` | — | H's `DrawWorld` (`:3868-3912`) has **no** `try/catch` | **HAVE (better)**: nothing to add under `-fignore-exceptions` |
| R17 | `UnSpan.cpp:37` | `int` → `INT` in `FSpanBuffer::AllocIndex` definition | DC (`INT`≠`int` there) | `UnSpan.cpp:37` | **PORT-IF** (cosmetic on our targets) |
| R18 | `UnSprite.cpp:111, 441, 454, 544, 603, 677` | `INT i;` / `FDynamicItem** Item;` / `FActorLink* Link;` hoists | for-scope | `UnSprite.cpp:164/167`, `489`, `501`, `592/593`, `615/616` (HP2-only copy for the `RenderNext` list), `651/654`, `724/727` | **PORT** |

Files T left identical to E: `Render.cpp`, `UnSoftLn.cpp`, `UnSpan.h`, `UnTest.cpp`, `Amd3d.h`.

### 1.2 HP2 vs epic — the 433 delta (why HP2's text is the merge base)

| H file | Nature of the 433 delta (all portable C++; no platform code except the gates in §1.3) |
|--------|------------------------------------------------------------------------------------------|
| `Inc/Render.h` | `+#include "UnStat.h"`; `FLightManagerBase::Light()/Fog()` → `LightAndFog()` + `LightParticleSystem(Frame, AParticleFX*)`; `FDynamicItem::Z` → `MinZ, MaxZ` (and an inline empty `FDynamicItem(INT)`); `FDynamicSprite(INT iNode, AActor*)`; new `FDynamicSysChild`/`FDynamicSysParent` (particle systems, `TArray<FDynamicSysChild*> Children`); **`FRenderStats` removed** (Render uses Core's `FStats GStat`, `Core/Inc/UnStat.h:157`, defined `Core/Src/Core.cpp:177`); 4-arg `DECLARE_CLASS(URender,URenderBase,CLASS_Config,Render)`; `BoundVisible(…, const FCoords& BoxCoords, …)`; `DrawCircle(…, UBOOL bScaleRadiusByZoom=0)`; `DrawBox(FBox,FCoords)`, `DrawCylinder`; `WireShow/BlendShow/BoneShow`, `MaxParticleSize`, `MaxParticleRenderTime`; `FTime LastEndTime/StartTime/EndTime`; `ParticleStats/EARIStats/EARIDetails/AnimStats`, `StatLine`, `AverageTime/TotalTime/PeakFrameTime`; `DynamicsCache`/`PointCache` become `static TArray<…>` + `AllocDynamicsCache(UModel*)`; `NumPostDynamics/PostDynamics` removed; `DrawParticleSystem`; `DrawMesh/DrawLodMesh(Frame, FDynamicSprite*, Owner, Coords, PolyFlags)` |
| `Src/UnRender.cpp` | stats on `FStats`/`FTime`/`appProcessSeconds()` (`:703`); adaptive `GlobalShapeLODAdjust`/`MaxParticleSize`; `REND WIRE/BONE/BLEND`; PSX2 `Pipe` + VU0 `ClipBspSurf` (dead, §1.3); `BoundVisible` rewrite; `AllocDynamicsCache`; **Lumos** surfaces (`:3453-3546`, `PF_LumosAffected = PF_HighShadowDetail\|PF_Modulated`, `PF_LumosAppear = PF_Gouraud`, `PF_LumosAlwaysOn = PF_DirtyShadows` — `Engine/Inc/UnObj.h:277-280`); sprites sorted by class and drawn behind masked geometry on span-based devices (`:3284-3300`); decals via `DrawTriangles` (`:3598`); `FDynamicSysParent` children; `try/catch` removed from `DrawWorld`; `RenderAudioGeometry` hook (`:3885-3886`); the `#if defined(LEGEND)` blocks are **dead** (no `LEGEND` define anywhere in H — correction to run 1, which called them unconditional) |
| `Src/UnLight.cpp` | `FLightInfo` gains `LightDir/MaxSpecDir/RadiusInner/LightDistTab`; distance tables replace `LightSqrt`; specular; `LightAndFog`; `LightParticleSystem`; **subtractive dark lights** (`bDarkLight`, `:603`, `:711`, `:2586`); PSX2 gates; **a Loki-era second `FLightManager::Merge` for GCC** (§1.3 — contains a real bug for us) |
| `Src/UnMeshRn.cpp`, `UnMeshRnLOD.h` | skeletal / LOD mesh rendering through `MaxVertices()` + `DrawTriangles()` batching (`UnMeshRn.cpp:117-145`), `FDynamicSprite*` plumbing, `LightAndFog` |
| `Src/UnSprite.cpp` | `FDynamicSysParent/Child`, EARI stats, `DrawParticleSystem` dispatch, `MinZ/MaxZ` sorting |
| `Src/UnParticleRn.cpp` | particle renderer: `MaxVertices()` (`:467`) + `DrawTriangles` (`:503`, `:552`) |
| `Src/UnSoftLn.cpp` | `SolveQuadratic`, `URender::DrawCylinder`, `DrawBox(FBox,FCoords)` |

Conclusion: **HP2's text is the base for every Render file; the template hunks of §1.1 are re-applied by
hand.** None of them conflicts with a 433 feature (they are one-to-three-line edits at sites that survived
unchanged). PLAN §3's dry-run conflict counts (`UnRender.cpp` 5, `UnLight.cpp` 4, `UnMeshRnLOD.h` 6) are
exactly these hunks landing next to 433 edits.

### 1.3 Every ASM / `_MSC_VER` / platform gate in HP2 Render

Enumerated with `grep -n -a -E '^\s*#\s*(if|ifdef|ifndef|elif|else|endif)' Inc/*.h Src/*.cpp Src/*.h`
(complete list; `NODECALS`, `NO_LUMOS_CODE`, `STATS`, `INDEX`, `LOD_MORPH`, `MESH_SUBDIVIDE`,
`DEBUG_CLIPPING` and include guards are feature gates, listed at the end).

**Prerequisite:** the values below assume the **template's** `Core/Inc/UnGnuG.h` (T:207-212 `ASM 0`,
`ASMLINUX 0`, `ASM3DNOW 0`, plus `__cdecl`/`STDCALL`). HP2's current `Core/Inc/UnGnuG.h:10-33` would give
`ASMLINUX 1` (and `#error Unsupported platform` for wasm/MinGW, where `__LINUX_X86__` is not defined), and
H `Core/Inc/UnBuild.h:61` defaults `ASM 1`. `template-core.md` owns that file; Render cannot build before it lands.

| H file:line | Gate | Contents | Compiled at `ASM=0 ASMLINUX=0 ASM3DNOW=0`, no `__PSX2_EE__`, no `_MSC_VER` | ACTION |
|-------------|------|----------|------------------------------------------------------------------|--------|
| `Inc/Render.h:234/276/280` | `#if ASM` / `#else` / `#endif` | `POWER_ASM` MSVC `__asm` macro; `DivSqrtApprox/DivApprox/SqrtApprox` | `#else`: `1.0/appSqrt(F)`, `1.0/F`, `appSqrt(F)` | none (optionally delete the `__asm` half) |
| `Src/UnLight.cpp:546-548` | `#if !__PSX2_EE__` | `FLOAT PointSquared(Vert.Point.SizeSquared());` | compiled | delete the gate lines, keep the body (PSX2 rule) |
| `Src/UnLight.cpp:582-595` | `#if !__PSX2_EE__` | specular term | compiled | delete the gate lines, keep the body |
| `Src/UnLight.cpp:628-632`, `1861-1869` | `#if 0` | dead code | not compiled | none |
| `Src/UnLight.cpp:655`, `:658`(`default:4799`) | `#pragma warning` | MSVC pragmas | ignored (`-Wunknown-pragmas` noise) | none / delete |
| **`Src/UnLight.cpp:661-1044`** | `#if _MSC_VER` (661) / `#else` (847) / `#endif` (1044) | **two complete definitions of `FLightManager::Merge`**: the MSVC one (662-846; inner `#if ASM` 727 / `#else` 823 / `#endif` 837) and a Loki-era GCC one (848-1043; inner `#if ASMLINUX` 905 / `#else` 1021 / `#endif` 1035) | **the GCC one** (848-1043) with its C loop 1022-1033 | **FIX — functional bug, see below** |
| `Src/UnLight.cpp:2357/2359/2361`, `2500/2502/2504` | `#if defined(LEGEND)` / `#else` | Legend-licensee variants | `#else` (LEGEND never defined in H, E or T) | none |
| `Src/UnRender.cpp:68/72/76` | `#if __PSX2_EE__` / `#else` | `check(sizeof(FVector)==16…)` vs `==12`, `sizeof(FCoords)==48` | `#else` | delete the PSX2 half |
| `Src/UnRender.cpp:249/252/255`, `605/607/609` | `#if __PSX2_EE__` / `#else` | stats font (`BigFont` vs `SmallFont`) | `#else` | delete the PSX2 half |
| `Src/UnRender.cpp:871/888/1138` | `#if __PSX2_EE__` / `#else` / `#endif` | PSX2 `Pipe()` vs x86 `Pipe()` | `#else` (888-1137) | delete PSX2 half (R10) |
| `Src/UnRender.cpp:897/1034/1136` | `#if ASM` / `#else` / `#endif` | MSVC `__asm` transform/outcode/project (898-1033) | `#else` | delete asm (R10) |
| `Src/UnRender.cpp:1036/1085/1090` | `#if ASMLINUX` / `#else` / `#endif` | AT&T clip-number asm (multi-line string literal) | `#else`: `ClipXM = Frame->PrjXM * Result.Point.Z + Result.Point.X; …` | delete asm (R10); **requires `ASMLINUX=0`** |
| `Src/UnRender.cpp:1098/1129/1134` | `#if LINUXASM` (sic — a different spelling, never defined anywhere) / `#else` | AT&T projection asm | `#else`: `Result.RZ = Frame->Proj.Z / Result.Point.Z; … IntY = appFloor(ScreenY)` | delete asm (R10) |
| `Src/UnRender.cpp:1148-1573` | `#if ASM3DNOW` | `#pragma pack(8)`, 3DNow! `TransformAndComputeOutcodeAndProject`, `AMD3DClip`, `AMD3DClipBspSurf` (`Amd3d.h` `_emit` macros) | not compiled (`ASM3DNOW` 0 in both H `UnBuild.h:66` and T `UnGnuG.h:212`) | none (or delete) |
| `Src/UnRender.cpp:1627-1630` | `#if ASM3DNOW` | `if( GIs3DNow ) return AMD3DClipBspSurf(…)` | not compiled | none |
| `Src/UnRender.cpp:1644/1732/1751` | `#if __PSX2_EE__` / `#else` | VU0 `lqc2/vmul…` inline asm in `ClipBspSurf` | `#else` (1733-1750, the `Pipe()` loop) | delete PSX2 half |
| `Src/UnRender.cpp:2129/2169/2209` | `#if 1` / `#else` | algorithm variant | `#if 1` half | none |
| `Src/UnRender.cpp:48,130,422,648,690,2540,2597,2685,3713` | `#if defined(LEGEND)` | licensee code | dead | none |
| `Src/UnTest.cpp:18` | (no gate) | `class DLL_EXPORT myclass { virtual void __stdcall xyzzy(…) }` | **does not compile**: with H's `UnGnuG.h:97` `DLL_EXPORT` is `extern "C"` → `class extern "C" myclass`; with T's it is empty but `__stdcall` is not a keyword on Linux GCC or wasm clang | **drop the file from CMake** (§1.4) |
| `Inc/UnRenderNative.h:12` | `#if __STATIC_LINK` | registrant macro | dead | none |
| `Inc/Amd3d.h` (both copies) | none | `#define pfadd(dst,src) { _asm _emit … }` macros | expand only inside `#if ASM3DNOW` | none |

Feature gates (not platform): `UnRender.cpp` `#if STATS` (36, 269, 717 — `STATS 1`, H `UnBuild.h:56`),
`#ifndef NO_LUMOS_CODE` (2478, 3453 — compiled; HP2 Lumos), `#ifndef NODECALS` (`Render.h:460`,
`UnRender.cpp:3556, 3769` — compiled); `UnMeshRn.cpp` `#if INDEX` (defined 1 at `:81`), `#ifdef MESH_SUBDIVIDE`
(262-337, dead), `#if 0` (468); `UnMeshRnLOD.h` `#ifdef LOD_MORPH` (118, 433, dead); `UnParticleRn.cpp`
`#ifdef DEBUG_CLIPPING` (dead).

#### The `FLightManager::Merge` bug (new finding — affects every renderer, not just SoftDrv)

Epic v400 has **one** `Merge` with an `#if ASM … #else C #endif` inside (E `UnLight.cpp:648`; T `:650`
likewise). HP2's 2002 tree has two: the MSVC one HP2 actually shipped, and a second copy under
`#else` of `#if _MSC_VER` (Loki's Linux port), which is the one GCC/clang will compile. They differ in one
load-bearing way — **the GCC copy has no dark-light branch**. MSVC copy, `UnLight.cpp:710-726`:

```cpp
		// Scale and merge the lighting.
		if( Info->Actor->bDarkLight )
		{
			// Subtract the color. Only non-ASM version for now.
			for( INT j=Info->MinU; j<Info->MaxU; j++ )
			{
				Dest[j] = Stream[j] - GET_COLOR_DWORD(Palette[NewSrc[j]]);
				DWORD SatMask = Dest[j] & 0x80808080;
				if( SatMask )
				{
					// Handle saturation by clamping low.
					SatMask += SatMask - (SatMask>>7);
					Dest[j] &= ~SatMask;
				}
			}
		}
		else
	#if ASM
```

GCC copy, `UnLight.cpp:1021-1035`: only the additive loop (`Dest[j] = Stream[j] + GET_COLOR_DWORD(…)`).
`bDarkLight` is HP2 content (`Engine/Classes/Darklight.uc`: "for adding shadows under trees and negative
lighting", `bDarkLight=True`; `Engine/Inc/EngineClasses.h:977`), and `Merge` builds every surface light map
at cache time **including static lights** (`UnLight.cpp:1919` reads the baked `Model->LightBits` into
`ShadowBits` → `ShadowMapGen` 2131 → `IlluminationMap` → `Merge` 2134/2248), so on a GCC build **every
Darklight would brighten instead of darken**, on SoftDrv *and* on XOpenGL/UTGLR (the light map is produced by
Render, not by the device). The shipped maps use them heavily — exported `Darklight` actors per map (package
export tables, §4.3 tool): `Adv1Willow` 199, `Adv8Forest` 15, `Adv12Chamber` 12, `Grounds_hub` 8,
`Ch6WizardCard` 5, `Grandstaircase_hub` 1; three maps also carry the `bDarkLight` property name on other
light actors. The two copies are
otherwise identical (a `diff -w` of 662-846 vs 848-1043 shows only the variable plumbing the AT&T asm needs:
`static INT Inputs[6]` with reference aliases, and the `StreamParam/DestParam` renames).

**ACTION:** delete lines 661 (`#if _MSC_VER`) and 847-1044 (`#else` … GCC copy … `#endif`) so the MSVC
copy is the only `Merge`; at `ASM=0` its `#else` (823-837) is the plain-C additive loop and the
`bDarkLight` branch in front of it is preserved. (The MSVC copy's `static INT Count` and the
`static DWORD SavedESP, SavedEBP` it references are only used inside `#if ASM`; expect unused-variable
warnings, nothing else.) Verify at runtime in `Adv1Willow` (199 Darklights): the tree shadows must be dark, as on retail Windows.

### 1.4 `Render.dsp` vs `Render/CMakeLists.txt`

`H/Render/Src/Render.dsp` `SOURCE=` entries (lines 97-183): `Render.cpp UnLight.cpp UnMeshRn.cpp
UnMeshRnLOD.h UnParticleRn.cpp UnRandom.cpp UnRender.cpp UnSoftLn.cpp UnSpan.cpp UnSprite.cpp UnTest.cpp`
+ headers `Amd3d.h ..\Inc\Render.h UnSpan.h` (the dsp compiled with `/Zp4 /D UNICODE /I ..\Inc` and a
`Render.h` precompiled header, `/Yc"Render.h"` on `Render.cpp`).

| | T `Render/CMakeLists.txt` | H `Render/CMakeLists.txt` (existing skeleton) | Recommended H list |
|-|---------------------------|-----------------------------------------------|--------------------|
| sources | `UnLight UnMeshRn UnRandom UnRender UnSoftLn UnSpan UnSprite Render` (8; **no `UnTest.cpp`**) | 10: the dsp's list incl. `UnParticleRn.cpp` **and `UnTest.cpp`** | **9: drop `Src/UnTest.cpp`** (its comment "the stock test harness the dsp always compiled" should go too) |
| DC `-O2` override for `UnLight.cpp` (T:14-17) | yes | no | no (DC-only) |
| includes | `PUBLIC Inc`, `PRIVATE Src` (T has no `Inc/` dir) | same | same — H needs `Inc` public because `SoftDrv`/`XOpenGLDrv`/`OpenGLDrv` include `Render.h` from it |
| links / defs | `Engine Core`; `RENDER_EXPORTS UPACKAGE_NAME=Render` | same | same |

Header duplicates: `Render/Src/UnSpan.h` ≡ `Render/Inc/UnSpan.h` and `Render/Src/Amd3d.h` ≡ `Render/Inc/Amd3d.h`
(`cmp` identical). `Render.h:21` includes `"Amd3d.h"`, `UnRender.h`/`Render.h` include `"UnSpan.h"`; with
`Inc` PUBLIC and `Src` PRIVATE, Render's own TUs find the `Src/` copy first and every consumer the `Inc/`
copy — identical today, a divergence trap tomorrow. **ACTION:** delete `Src/UnSpan.h` and `Src/Amd3d.h`
(keep `Inc/`); the dsp's `.\Amd3d.h`/`.\UnSpan.h` entries are stale bookkeeping.

### 1.5 MSVC6 for-scope leaks in HP2 Render (scanner output, hand-checked)

Method: `forscope2.py` (session scratchpad, `rfse-work/`) evaluates `#if` with `ASM=0` etc., strips comments/strings,
tracks `for`-init declarations and statement extents, and reports the first use of a for-declared name
after its loop where no other declaration is visible. **Validation:** on E it reports exactly the sites the
template hoisted (E `UnRender.cpp` 11 hits ↔ T's 11 hoists; `UnSprite.cpp` 5 ↔ 5; `UnMeshRnLOD.h` 4 ↔ 4;
`UnRandom.cpp` 1 ↔ 1; `UnLight.cpp` 6 real + 1 false positive) and **zero** on T's files. Each HP2 hit below
was then read by eye.

| H file | hoist needed before the loop at … (use line) | false positives dropped |
|--------|-----------------------------------------------|-------------------------|
| `Render/Src/UnLight.cpp` | `i` 360; `i` 412; `Find` 1950 (1951); `Find` 1971 (1972); `NewNum` 2444 (2451); `Find` 2566 (2567) | — |
| `Render/Src/UnRandom.cpp` | `i` 119 | — |
| `Render/Src/UnRender.cpp` | `Frame` 1908 (1929); `i` 2581 (2589); `Other` 3047 (3050); `i,j` 3060 (3062); `i` 3116; `i` 3165 (3168); `i` 3232; `Pass` 3265 (3275); **`Sprite` 3286 (3290, HP2-only sprite sort)**; `i` 3680 (3693 coronas); `i` 3786 (3788) and `j` 3797 (3799) decals | — |
| `Render/Src/UnSprite.cpp` | `i` 164 (167); `i` 489; `i` 501; `Item` 592 (593); **`Item` 615 (616, HP2-only)**; `Link` 651 (654); `Link` 724 (727) | — |
| `Render/Src/UnMeshRnLOD.h` | `i` 166; `i` 283 | `w` 650, `v` 682 — `for_count( w, … )` is a macro declaring its own `INT` (`Core/Inc/UnTemplate.h:748`) |
| `Render/Src/UnMeshRn.cpp`, `UnParticleRn.cpp`, `UnSoftLn.cpp`, `UnSpan.cpp`, `Render.cpp`, `Inc/Render.h` | none | — |

Idiom to apply (T's): declare the variable immediately before the first loop, drop the type from the
`for`-init, e.g. `INT i; for( i=0; … )`; for search loops `FLightInfo* Find; for( Find=FirstLight; …; Find++ );`.

### 1.6 Render — other things that must be true for it to compile/run (owned elsewhere)

- `appProcessSeconds()` returning `FTime`: declared `Core/Inc/UnFile.h:263`, defined **only** in
  `Core/Src/UnVcWin32.cpp:501` (MSVC-only); used by `UnRender.cpp:703`
  (`float ProcessTime = appProcessSeconds().GetFloat();`) and Engine `UnGame.cpp:843,1465`. The portable core
  (`UnUnix.cpp`) must define it (`template-core.md` / `hp2-core-engine-specifics.md`).
- `FTime` arithmetic with `appSeconds()` (`UnRender.cpp:3677-3679`: `static FTime LastTime = appSeconds();
  FLOAT Delta = 3.0f * (appSeconds() - LastTime);`) — whatever `appSeconds()` returns after the `UnGnuG.h`
  merge must keep these compiling.
- `GStat` pointer walk: `UnRender.cpp:231-232` treats `FStats` as an array of `INT` up to `EARIActorNames`
  (`(INT*)&GStat.EARIActorNames - (INT*)&GStat`). Fine under `-fpack-struct=4`; do not reorder `FStats`.
- `UnParticleRn.cpp` needs Engine's `UnParticleList.h`/`UnParticle.h` (`Engine/Inc`, present). It defines
  a file-local `template<class T> void Swap(T&,T&)`; no clash with Core (`grep Swap( Core/Inc` → none).

### 1.7 Render action checklist

- [ ] Prerequisite: template `Core/Inc/UnGnuG.h` in place (`ASM/ASMLINUX/ASM3DNOW = 0`).
- [ ] `Render/CMakeLists.txt`: remove `Src/UnTest.cpp` (and its comment).
- [ ] Delete `Render/Src/UnSpan.h`, `Render/Src/Amd3d.h` (duplicates of `Inc/`).
- [ ] `UnLight.cpp`: remove `#if _MSC_VER` (661) and the whole GCC `Merge` (847-1044) — keeps `bDarkLight`.
- [ ] `UnLight.cpp`: drop the `#if !__PSX2_EE__` lines at 546/548, 582/595 (keep bodies).
- [ ] `UnRender.cpp`: `HackRaster[MAX_RASTER_Y]` with `#define MAX_RASTER_Y 8192` + `EndY = Min<INT>( EndY, MAX_RASTER_Y - 1 );` at the top of `SetupRaster` (R12).
- [ ] `UnRender.cpp`: delete the PSX2 halves (68, 249, 605, 871, 1644) and the asm of `Pipe()` (R10).
- [ ] `UnRender.cpp` `DrawWorld`: add the `Actor->GetClass() && Canvas && Canvas->GetClass()` guard; add the AutoFOV base-FOV swap only together with the Engine AutoFOV port (R15).
- [ ] For-scope hoists at every site of §1.5 (UnLight 6, UnRandom 1, UnRender 13, UnSprite 7, UnMeshRnLOD.h 2).
- [ ] `Min<INT>`/`Max<INT>`/`Clamp<INT>`/`INT` (R3, R7, R13, R17) only where the compiler asks.
- [ ] Acceptance: `cmake --build --preset native --target Render` clean; later, a map with `Darklight` actors looks like retail.

## 2. Fire

### 2.0 Why Fire matters for HP2 specifically

HP2 content instantiates Fire classes at load (export tables of the shipped packages, §4.3 tool):
`FireTexture` ×52 in `System/HPParticle.u` (and the same 52 in `Textures/HP_FX.utx`), `WetTexture` in
`Liquids.utx` (14), `HPParticle.u`/`HP_FX.utx` (10), `SpellShapes.utx` (14), `chamber.utx`, `SlytherinCR.utx`;
`IceTexture` ×5 in `HPParticle.u`; `WaveTexture` in `Detail.utx`. `HPParticle.u` is on the boot path of every
level with particle effects, and **`UFireTexture::PostLoad` runs `VERIFY_CLASS_SIZE(UBitmap)`,
`(UTexture)`, `(UFireTexture)`, `(UWetTexture)`, `(UWaveTexture)`, `(UFractalTexture)`** (H
`UnFractal.cpp:4132-4137`, each a `check()` → `appFailAssert` on mismatch), and the `Init`s run
`VERIFY_CLASS_OFFSET` on `FractalTexture.UMask` (`:3994`), `FireTexture.SparkType` (`:4080`),
`WaterTexture.DropType` (`:4358`), `WaveTexture.BumpMapAngle` (`:4551`) → `appErrorf("Class %s Member %s
problem: Script=%i C++=%i")`. So a C++/script layout mismatch in Fire is a hard crash at the
first map load, not a cosmetic glitch — which is exactly why the template's `GCC_ALIGN(4)` edits (F8) are
mandatory.

### 2.1 Template vs epic — all hunks

T files: `Src/UnFractal.cpp` (Δ vs E: 8 hunks), `Src/FractalPrivate.h` (3 hunks), `Inc/FireClasses.h`
(identical to E and to H), `Classes/*.uc` (identical). T drops `Fire.dsp`, `makefile` and the three `.asm`
files; `T/Fire/CMakeLists.txt` compiles `Src/UnFractal.cpp` only, `PUBLIC Inc`, `PRIVATE Src`, links
`Engine Core`, `FIRE_EXPORTS UPACKAGE_NAME=Fire`.

| # | T file:line | Change | Why | H site | H action |
|---|-------------|--------|-----|--------|----------|
| F1 | `UnFractal.cpp:148-149` | `INT t;` hoisted; `for( t=0; t<256; t++ )` | `t` reused by the next two loops (for-scope) | `UnFractal.cpp:175` (`for( INT t=0…`), reuses at `180`, `186` | **PORT** |
| F2 | `UnFractal.cpp:279`, `:404` | `Y = (Ydimension - 2);` → `DWORD Y = (Ydimension - 2);` (and `-1`) | the loop `for (DWORD Y=0; …)` above had scoped `Y` away | `UnFractal.cpp:305` (loop at 268), `:430` (loop at 395) | **PORT** |
| F3 | `UnFractal.cpp:1293`, `:1313` | `INT Flash;` before `for( Flash=0; …)` (two sites) | for-scope (`Flash` from the loop at E:1248) | `UnFractal.cpp:1319`, `:1338` (loop at 1275) | **PORT** |
| F4 | `UnFractal.cpp:3913-3927` | 12× `Min(…,255)` → `Min<INT>(…,255)` in the `UWaveTexture` palette | DC `int32_t==long` (see §1.1 insight); on our targets `INT==int` | `UnFractal.cpp:3947-3961` | **PORT-IF** |
| F5 | `UnFractal.cpp:4270`, `4584`, `4746` | `Clamp(…)` → `Clamp<INT>(…)` (`WaveTable`, two `RenderTable`s) | DC | `UnFractal.cpp:4342`, `4657`, `4823` | **PORT-IF** |
| F6 | `FractalPrivate.h:18-22` | `#if !_MSC_VER && !defined(__MINGW32__)` around `typedef SQWORD __int64;` | on MinGW `__int64` is a builtin keyword; `typedef … __int64` is a syntax error (ufront 2.33) | `Inc/UnFractal.h:19-21` (`#if !_MSC_VER`) | **PORT** (required for the Windows target) |
| F7 | `FractalPrivate.h:110-116` | `PLATFORM_LOW_MEMORY` spark limits (128/1024) | LOW_MEMORY | `Inc/UnFractal.h` `MAXSPARKSINIT/LIMIT` | **SKIP** |
| F8 | `FractalPrivate.h:197`, `:202`, `:315` | `BYTE SparkType GCC_ALIGN(4);` · `BYTE FX_Heat GCC_PACK(4)` → `GCC_ALIGN(4)` · `BYTE DropType GCC_ALIGN(4);` | **ABI**: `UFractalTexture` ends with `BYTE DrawPhase; BYTE AuxPhase;` (H `UnFractal.h:69-70`) → 2 bytes of tail padding. Under the Itanium C++ ABI (GCC and clang, *including* MinGW g++ and wasm clang) the first member of a derived class may be placed in a non-POD base's tail padding; MSVC never does, and the UnrealScript compiler laid `Fire.u`'s properties out MSVC-style (`SparkType`/`DropType` at the 4-aligned end of the parent). `GCC_ALIGN(4)` forces the MSVC offset. The `FX_Heat` change is layout-neutral as far as I can tell (`packed,aligned(4)` vs `aligned(4)` on a `BYTE` that follows a `BITFIELD … GCC_PACK(4)`) and most likely silences clang's "packed attribute ignored" warning — *unverified*; the layout guard settles it | `Inc/UnFractal.h:174` (`SparkType`), `:179` (`FX_Heat GCC_PACK(4)`), `:292` (`DropType`) — H's `UFractalTexture` members are unchanged from E (only `Prime()` added), so the arithmetic is identical | **PORT (mandatory)** |

### 2.2 HP2 vs epic — the 433 delta (Δ 138 lines, all portable)

- `FractalPrivate.h` renamed/moved to **`Inc/UnFractal.h`**, `+#include "UnFireNative.h"` (`__STATIC_LINK`
  registrants, dead under `UNREAL_STATIC`); 4-arg `DECLARE_[ABSTRACT_]CLASS(…,Fire)`; `Lock(…, FTime Time, …)`
  for `UWetTexture`/`UIceTexture`; the `__int64 TotalTime*`/`TotalFrames` globals and `CycleCount()` moved
  from the header into `UnFractal.cpp:83-109` with `extern` declarations in the header (an improvement: epic
  defined globals in a header); `extern "C" { FTime LTimeTotal1, LTimeTotal2, LinePixels; }`.
- `UnFractal.cpp`: `FakeAtan` `double`→`FLOAT`; `(BYTE)-128`; `appRound(X/Y)` in `MousePosition`'s
  `TempDrawSpark` calls; **`UFractalTexture::Prime()`** (`:4015-4043`, warms 48 frames when
  `NoFractalAnim`); `UFireTexture/UWaveTexture/…::ConstantTimeTick` bodies wrapped in
  `if( __Client && (! __Client->NoFractalAnim) )` (`:4226`, `:4620`, `:4790`, `:4991`, `:5027`) — `__Client` is `UBitmap::__Client`
  (`Engine/Inc/UnTex.h:252`), set in `UClient::UClient()` (`Engine/Src/UnCamMgr.cpp:24`), so the new
  `NSDLDrv` client must keep deriving from `UClient` and running its constructor (it does in the template);
  `NoFractalAnim` is a `UClient` config bool (`Engine/Src/UnCamMgr.cpp:64`), absent from H's `Default.ini`
  (→ 0, fractals animate); extra `VERIFY_CLASS_OFFSET(U,Bitmap,MaxColor)`, `(U,Texture,BumpMap)` in
  `UFireTexture::Init`, and the six `VERIFY_CLASS_SIZE` in `PostLoad` (§2.0).
- The `catch(...)` recovery (`PrimeCurrent=255; bRealtime=0; __Client->NoFractalAnim=1`) is inside
  `#if WATERASM` (`UnFractal.cpp:2136-2169`) → **not compiled at `ASM=0`**; nothing Fire compiles uses
  `try/catch` on the web.

### 2.3 ASM families and their C fallbacks (`H/Fire/Src/UnFractal.cpp`)

`UnFractal.cpp:48-53`: `#define RANDASM ASM`, `FIREASM ASM`, `WATERASM ASM`, `WETASM ASM`, `ICEASM ASM`
("Define these to be either 0 or ASM, never 1"). With `ASM=0` all five are 0. `__asm` occurs 9 times, all
inside these gates.

| Gate | lines (`#if` / `#else` / `#endif`) | ASM side | C side compiled at `ASM=0` | OK? |
|------|-------------------------------------|----------|-----------------------------|-----|
| `ASM` | 93 / 106 / 108 | `CycleCount()` via `rdtsc` `_emit` | `return 0;` | yes |
| `RANDASM` | 128 / 146 / 151 | `SpeedRand()` | `SpeedRindex=(SpeedRindex+1)&63; return SpeedRandArr[…]^=…` | yes |
| `RANDASM` | 191 / 194 / 196 | index init | `SpeedRindex=0` | yes |
| `FIREASM` | 210 / 218 / 463 | `extern "C" __cdecl CalcWrapFire/CalcSlowFire(P2)` from `UnFire.asm`/`UnFireP2.asm` | C `CalcWrapFire`/`CalcSlowFire` (the F2 sites) | yes |
| `ASM && 0` | 981 / 1058 / 1084, 1087 / 1168 / 1194 | dead `__asm` spark movers | C versions (always) | yes |
| `WATERASM` | 2136 / 2170 / 2612 | `CalcWaterASM` (from `UnWater.asm`) in `try/catch` | `Output4Pix` macro C water | yes |
| `WETASM` | 2644 / 2716 / 2736 | per-line `__asm` | C loop | yes |
| `ICEASM` | 2811-2813, 3231-3233 | `INT TempUMask = UMask;` (only the asm uses it) | omitted | yes |
| `ICEASM` | 2828 / 3192 / 3204, 3248 / 3410 / 3421 | PPro + Pentium `__asm` ice displacement | C `for( INT u=0; u<Xdimension; u+=2 )` loops | yes |
| `FIREASM` | 4234 / 4245 / 4248 | `GIsPentiumPro` dispatch to the `…P2` asm | calls the C `CalcWrapFire/CalcSlowFire` | yes |
| `COPYPALETTE` | 4144/4155, 4578/4589 | (feature, `#define COPYPALETTE 1` at :56) | compiled | yes |

Leftovers that compile but warn at `ASM=0`: `static DWORD ESPStorage, EBPStorage` in the two ice functions
(unused); the `extern "C" … __cdecl` prototypes are inside `#if FIREASM` so `__cdecl` never matters.
`UnFire.asm`, `UnFireP2.asm`, `UnWater.asm` stay in `Src/` as reference only — correct.

### 2.4 `Fire.dsp` vs `Fire/CMakeLists.txt`

`Fire.dsp` `SOURCE=`: `FractalPrivate.h` (**stale** — the file is now `Inc/UnFractal.h`), the three `.asm`
(custom-build `ml`), `UnFractal.cpp`, the `.uc`/`.upkg`. H's existing `Fire/CMakeLists.txt` = T's
(`Src/UnFractal.cpp` only) plus a comment — **correct as is**. `Fire.dsw` and `makefile` are unused.

### 2.5 For-scope leaks (scanner, hand-checked)

`forscope2.py` on H `UnFractal.cpp`: `t` 180 (and 186), `Y` 305, `Y` 430, `Flash` 1319 (and 1338) — the
same four families as E (and zero on T). No HP2-only additions.

### 2.6 Fire action checklist

- [ ] `Fire/Inc/UnFractal.h:19`: `#if !_MSC_VER && !defined(__MINGW32__)` (F6).
- [ ] `Fire/Inc/UnFractal.h:174`: `BYTE SparkType GCC_ALIGN(4);` · `:179`: `BYTE FX_Heat GCC_ALIGN(4);` · `:292`: `BYTE DropType GCC_ALIGN(4);` (F8). `GCC_ALIGN` already exists in both `UnGnuG.h` versions (H `:87`, T `:83`).
- [ ] `Fire/Src/UnFractal.cpp`: `INT t;` before 175 (drop the type there); `DWORD Y =` at 305 and 430; `INT Flash;` before 1319 and before 1338.
- [ ] `Min<INT>`/`Clamp<INT>` at 3947-3961, 4342, 4657, 4823 only if the compiler complains.
- [ ] Acceptance: `--target Fire` clean; at the first map load no `check(sizeof(UFireTexture)==…)`/`Class FireTexture Member SparkType problem` assertion (the template reported **zero** layout warnings with these edits: T `BUILD.md` "Verified at 2.33").

## 3. SoftDrv

Retail HP2 shipped SoftDrv: `Assets/System/SoftDrv.dll` + `SoftDrv.int`, and `Config/System/Default.ini:31`
`WindowedRenderDevice=SoftDrv.SoftwareRenderDevice` (section `[SoftDrv.SoftwareRenderDevice]` at `:213-221`).
So SoftDrv is a real HP2 device, not a template extra.

### 3.0 File inventory and diff sizes (re-measured this run)

`diff --strip-trailing-cr -w -a`, counting `<`/`>` lines. E = `epic-sources/ut99-v400/SoftDrv/Src`.

| File | E lines | T lines | H lines | T vs E | H vs E | T vs H |
|------|--------:|--------:|--------:|-------:|-------:|-------:|
| `DrawPoly.cpp` | 4614 | 4739 | 4603 | 189 | 87 | 276 |
| `DrawSurf.cpp` | 21548 | 22317 | 21547 | 795 | 25 | 820 |
| `DrawTile.cpp` | 3610 | 3615 | 3610 | 11 | **0** | 11 |
| `Line2D.cpp` | 217 | 222 | 217 | 5 | 2 | 7 |
| `SoftDrv.cpp` | 234 | 239 | 234 | 7 | **0** | 7 |
| `SoftDrvPrivate.h` | 238 | 301 | 239 | 71 | 11 | 80 |
| `Hit.cpp`, `Line.h`, `Line1.h` | 123/172/32 | = | = | 0 | 0 | 0 |
| total | | | | **1 078** | **125** | 1 201 |

(Correction to run 1: it said "1 612" T-vs-E lines and "127" H-vs-E lines; the re-count is 1 078 and 125.
The conclusion is the same.) `__asm` counts are identical in T and H (`DrawPoly` 32, `DrawSurf` 104,
`DrawTile` 19; `SoftDrvPrivate.h` T 2 = 1 code + 1 comment, H 1). H also has `Src/SoftDrv.dsp` (sources
`DrawPoly DrawSurf DrawTile Hit Line.h Line1.h Line2D SoftDrv SoftDrvPrivate.h`, lines 93-126) = the six
`.cpp` of the CMake list.

### 3.1 Template vs epic — what engine-ut99 did to SoftDrv, hunk by hunk

| # | T file:line | Change | Kind | Needed to **compile** at ASM=0 on GCC/clang? |
|---|-------------|--------|------|------|
| S1 | `SoftDrvPrivate.h:10` | `#include "..\..\Render\Src\RenderPrivate.h"` → `#include "RenderPrivate.h"` (path from CMake) | portability | **yes** — backslash paths do not resolve on Linux/wasm |
| S2 | `SoftDrvPrivate.h:12-24` | `#ifdef PLATFORM_WIN32 #undef IN #undef OUT` (ufront 2.33) | MinGW | yes on MinGW (`DrawSurf.cpp` bilinear filter has a local `IN`) |
| S3 | `SoftDrvPrivate.h:27-33` | `#if !ASM && __INTEL__ #define GIsMMX false #endif` | behaviour | yes in practice: otherwise native x86 picks `MMX*` ColorModes whose bodies are `#if ASM`'d out (S9) → black screen |
| S4 | `SoftDrvPrivate.h:35-54` | `SOFTDRV_MAX_SCREEN` (default 4096) → `MaximumX/YScreenSize` (were 2048/1200) | web robustness (2.12, 2.19f, 2.71) | no, but mandatory for the web (canvas = device pixels) |
| S5 | `SoftDrvPrivate.h:62-82` | `_rotl/_rotr`: `<stdlib.h>` on `PLATFORM_WIN32`, `<x86intrin.h>` on `PLATFORM_X86`, inline fallback (s==0 guarded) elsewhere (wasm) | portability | **yes** |
| S6 | `SoftDrvPrivate.h:185-189`, `SoftDrv.cpp:25` | `BITFIELD BilinearFiltering` + its `UBoolProperty` | feature (true bilinear vs knight's-move dither) | no |
| S7 | `SoftDrv.cpp:45-49` | `SupportsFogMaps = 1` instead of `= GIsMMX` | feature (portable fog) | no — but without it the portable path renders no fog at all |
| S8 | `DrawPoly.cpp:62` | `static SavedEBP,SavedESP;` → `static int …` | C++ (no implicit int) | **yes** |
| S9 | `DrawPoly.cpp:1476/2419`, `2426/3370`, `3414/3967` | `#if ASM … #endif` around the whole bodies of `InnerGouraudMMX15`, `InnerGouraudMMX16`, `MMXFlashTriangle` (pure `__asm`; `InnerGouraudMMX32` already had `#if ASM` at 725/1470 in epic) | portability | **yes** |
| S10 | `DrawPoly.cpp:4663-4665` | `TriangleFunc = &USoftwareRenderDevice::MMXFlashTriangle` / `PentiumFlashTriangle` | C++ (pointer-to-member needs `&Class::`) | **yes** |
| S11 | `DrawPoly.cpp:56, 91, 125, 142-681` | per-vertex **mesh volumetric fog** on the Pentium Gouraud path: `FTexSetup::FR/FG/FB`, `FPolyCSetup::FogR/G/B, DFogR/G/B`, `static UBOOL GMeshFog`, `+ GPolyC.FogR/G/B` in all nine span kernels | feature | no |
| S12 | `DrawPoly.cpp:4135-4164, 4221-4242, 4393-4416, 4442-4466` | fog edge/pixel setup in `PentiumFlashTriangle` (`TriVertex[v].Fog`, `dFRdX…`, `FogScaleR/G/B` per colour depth; zero for `PF_Translucent\|PF_Modulated`) | feature | no |
| S13 | `DrawPoly.cpp:4205-4217` | **`DR/DG/DB` edge-gradient fix** (green was stepped by blue's gradient on the Pentium path) | bug fix | no — but visible at ASM=0 under coloured light |
| S14 | `DrawPoly.cpp:4491-4494` | `GMeshFog = VolumetricLighting && VolumetricLightingMeshes && SupportsFogMaps;` at the top of `DrawGouraudPolygon` | feature | no — **but it does not compile on HP2** (§3.3 S14) |
| S15 | `DrawSurf.cpp` (≈770 lines) | portable **fog maps** (`FogPhoton[MaximumXScreenSize]`, `FogShade`, `LightPassPentiumFog`, 12 `MergePass*Fog` twins, post-pass swap at T:21308-21323 gated on `Surface.FogMap && Surface.LightMap`); `TexturePassFiltered` + `InitTexPentiumFiltered` + `TexFilterMode/KnightRow/KnightU/V`; `InitBilinearKernel(UBits, iMip)` | feature | no |
| S16 | `DrawSurf.cpp:905-913` | `+0x010101` rounding bias in the C `LightPentium` fallback (every non-x86 build was 1 LSB dark; verified bit-exact vs the asm by `tools/asmport`) | bug fix | no |
| S17 | `DrawSurf.cpp` `@@ -498`, `@@ -607` | `for( INT i=0,j=3; …)`, `for( INT i=0; i<512; …)`, `for (INT i = 0; i<16; …)` | for-scope | **yes** |
| S18 | `DrawSurf.cpp` `@@ -2152` (T:2855-2900) | extra empty MMX stubs (`MMX15MaskedRender8A/B`, `MMX15Fog…`, `MMX16…`, `MMX32Render8Unrolled[Faster]`, `MMX32FogStippledRender8`) in the `!ASM` stub block | link | **yes** (the dispatch tables name them) |
| S19 | `DrawSurf.cpp:22195` | `InitLightPentiumDelta( appRound(DUF*…), appRound(DVF*…) )` | narrowing float→INT | warning / correctness |
| S20 | `DrawTile.cpp:3075-3092` | `SpanIndex[1200]` → `[MaximumYScreenSize]` + `SpanY = Min( SizeY, (INT)MaximumYScreenSize )` clamp (the >1200-tall-viewport self-loop hang) | web robustness | no, but mandatory for the web |
| S21 | `Line2D.cpp:63-66, 144` | `#ifdef PLATFORM_MSVC` around the depth-cued line rasteriser (`Line.h`/`Line1.h` need MSVC-only token pasting) → no 2D lines on GCC/wasm | portability | **yes** |
| S22 | `CMakeLists.txt`, `WEB-NOTES.md` | `ASM=0`, `SOFTDRV_MAX_SCREEN` cache var (wasm preset 8192), include `../Render/Src`; the web-vs-native notes | build | — |

Hit.cpp, Line.h, Line1.h: untouched.

### 3.2 HP2 vs epic — the 433 delta (all of it)

| # | H file:line | Change | Needed in the merged file? |
|---|-------------|--------|----------------------------|
| H1 | `SoftDrvPrivate.h:10` | `#include "..\..\Render\Inc\Render.h"` (HP2 renamed `RenderPrivate.h` → `Render/Inc/Render.h`) | yes, as `#include "Render.h"` (forward slashes / include path) |
| H2 | `SoftDrvPrivate.h:~83` | `DOUBLE D;` removed from `union FMMX` | **no** — it was removed only because HP2's Loki `Core/Inc/UnGnuG.h` has **no `DOUBLE` typedef** (typedef list `:124-152`; `DOUBLE` exists only in `UnVcWin32.h:86`). The template's `UnGnuG.h:179` has `typedef double DOUBLE;`, and T's `DrawSurf.cpp:419,566` and the template's `Game/Src/Launch.cpp` (already in HP2: `:74,478,637`) use `DOUBLE`. Nothing reads `FMMX::D` (grep). Harmless either way; keep T's text |
| H3 | `SoftDrvPrivate.h:117` | `DECLARE_CLASS(USoftwareRenderDevice,URenderDevice,CLASS_Config,SoftDrv)` | **yes** (433 4-arg macro) |
| H4 | `SoftDrvPrivate.h:155-158` | `DrawComplexSurface(…, DWORD PolyFlags, BYTE cAlpha)`, `INT MaxVertices()`, `DrawTriangles(Frame, Info, Pts, NumPts, _WORD* Indices, INT NumIndices, PolyFlags, Span)`; **no `DrawGouraudPolygon`** | **yes** — these are pure virtuals in H `Engine/Inc/UnRenDev.h:75-77` |
| H5 | `DrawPoly.cpp:3682-3683` | MMX edge setup: `DFG = …Fog.Y`, `DFB = …Fog.Z` (epic had them swapped) | cosmetic for us — the site is inside `MMXFlashTriangle`, which is `#if ASM`'d out (T:3414-3967; the site is **T:3719-3720**, correction to run 1's "T:3679") |
| H6 | `DrawPoly.cpp:4366-4596` | `DrawGouraudPolygon` → `MaxVertices(){ return 256; }` + `DrawTriangles(…)`: same body, plus (a) `if( (PolyFlags & (PF_Highlighted\|PF_Translucent)) == (PF_Highlighted\|PF_Translucent) && (PolyFlags & PF_Masked) ) PolyFlags &= ~(PF_Highlighted\|PF_Translucent);`, (b) three draw loops replacing epic's hard-coded 3..7-point fan (quoted below) | **yes** |
| H7 | `DrawSurf.cpp:20373` | `DrawComplexSurface( …, DWORD PolyFlags, BYTE cAlpha )` | **yes** |
| H8 | `DrawSurf.cpp:20384-86, 20402, 20864, 21484` | `Surface.PolyFlags` → `PolyFlags` (IsModulated/IsTranslucent/IsMasked, `PF_FlatShaded`, `PF_NoSmooth`, `PF_Selected`) | **yes — functionally load-bearing**, see below |
| H9 | `DrawSurf.cpp:361-367`, `20846`; `Line2D.cpp:40` | `DOUBLE`→`double`, dropped parentheses, `0.67f`, `0.56f` | no (cosmetic; `DOUBLE` exists with the template `UnGnuG.h`) |

H6's loops, verbatim (H `DrawPoly.cpp:4546-4593`, indentation compressed):

```cpp
	// Allow indexed or non-indexed drawing.
	if( Indices )
		// Indexed tri-list.
		for( INT i=0; i<NumIndices; i+=3 )
		{ Point0 = Pts[ Indices[i] ]; Point1 = Pts[ Indices[i+1] ]; Point2 = Pts[ Indices[i+2] ];
		  (this->*TriangleFunc)( Frame, Texture, PolyFlags, SpanBuffer ); }
	else if( NumIndices != NumPts )
		// Non-indexed tri list.
		for( INT i=0; i<NumPts; i+=3 )
		{ Point0 = Pts[i]; Point1 = Pts[i+1]; Point2 = Pts[i+2];
		  (this->*TriangleFunc)( Frame, Texture, PolyFlags, SpanBuffer ); }
	else
	{	// Tri fan.
		Point0 = Pts[0];
		for( INT i=0; i<NumPts-2; i++ )
		{ Point1 = Pts[i+1]; Point2 = Pts[i+2];
		  (this->*TriangleFunc)( Frame, Texture, PolyFlags, SpanBuffer ); }
	}
```

Who calls which case (so the three branches are all live):

| Caller (H) | Indices / NumIndices | Branch |
|------------|---------------------|--------|
| `Render/Src/UnMeshRn.cpp:129-134` (`#if INDEX`, `INDEX`=1 at `:81`) — meshes, batched up to `MaxVertices()` verts / `VBSize*6` indices | index buffer | indexed tri-list |
| `Render/Src/UnMeshRn.cpp:138-143` (`#else`, dead) | `NULL, 0` | non-indexed tri-list |
| `Render/Src/UnParticleRn.cpp:503, 552` — particle systems | index buffer | indexed tri-list |
| `Render/Src/UnRender.cpp:3598` — decals (`// NumIndices=NumPoints => single polygon`) | `NULL, DecalNumPoints` | fan |
| `Engine/Src/UnCamera.cpp:199-203` — the base `URenderDevice::DrawGouraudPolygon` (`DrawTriangles( Frame, Info, Pts, NumPts, NULL, NumPts, PolyFlags, SpanBuffer )`) | `NULL, NumPts` | fan |

So SoftDrv **must not** declare its own `DrawGouraudPolygon`: the non-pure base (`UnRenDev.h:78`, note its
`int NumPts`) already forwards to `DrawTriangles`' fan branch, and HP2's own SoftDrv omits it.

**Why H8 is load-bearing, not cosmetic:** Render passes flags that differ from `Surface.PolyFlags` for HP2
**Lumos** surfaces. `PF_LumosAffected = PF_HighShadowDetail | PF_Modulated`, `PF_LumosAppear = PF_Gouraud`,
`PF_LumosAlwaysOn = PF_DirtyShadows` (`Engine/Inc/UnObj.h:277-280`). On a span-based device
(`UnRender.cpp:3469-3485`) Render ORs in `PF_Translucent` for always-on Lumos surfaces and **strips the three
Lumos bits — including `PF_Modulated`** — before calling
`DrawComplexSurface( Frame, Surface, Facet, LumosPolyFlags, (char)iAlpha )` (`:3541`); non-Lumos surfaces get
`Surface.PolyFlags, 255` (`:3550`). A SoftDrv that kept reading `Surface.PolyFlags` would draw every Lumos
surface as *modulated* (and never translucent). Note the side effect this has in retail too: the gate is
`(PolyFlags & PF_LumosAffected)` (`:3456`), which is non-zero for any plain `PF_Modulated` surface, so in
software mode HP2 draws ordinary modulated BSP surfaces unmodulated — that is retail behaviour and must be
**preserved, not fixed** (screenshot comparisons against XOpenGL will differ there). `cAlpha` on this path is
only ever 0 or 255 (`iAlpha = (LumosPolyFlags & PF_LumosAppear) ? 255 : 0`, `:3465`; the graded alpha is the
hardware branch only), and HP2's SoftDrv ignores it; the merged file ignores it too (never treat `cAlpha==0`
as "invisible" here).

### 3.3 Merge analysis — base on the template's files, re-apply HP2's 433 face

Direction: T is +1 078 lines of mostly-tested ufront work over epic; H is +125 lines of interface over epic,
and the two sets touch disjoint regions except `DrawGouraudPolygon`→`DrawTriangles`. **Take T's nine files,
re-apply H1, H3, H4, H6, H7, H8.** (The reverse — H as base, re-applying S1-S22 — is ~9× more text and
would re-open the four ufront bugs S13/S16/S20/S4.)

Three things in T's text do not fit HP2 and need a decision:

| # | T site | Problem on HP2 | ACTION |
|---|--------|----------------|--------|
| S14 | `DrawPoly.cpp:4494` `GMeshFog = VolumetricLighting && VolumetricLightingMeshes && SupportsFogMaps;` | **`VolumetricLightingMeshes` does not exist in HP2's `URenderDevice`** (H `Engine/Inc/UnRenDev.h:40-60`: `…VolumetricLighting; ShinySurfaces; …`; the template added it to Engine at T `UnRenDev.h:48` + `UnCamera.cpp:27`). Compile error | Do **not** add it to Engine's `URenderDevice` (that changes the layout every in-tree and forked driver compiles against). Make it a **SoftDrv-local** config bool: `BITFIELD VolumetricLightingMeshes;` in `USoftwareRenderDevice` next to `BilinearFiltering`, plus `new(GetClass(),TEXT("VolumetricLightingMeshes"),RF_Public)UBoolProperty(CPP_PROPERTY(VolumetricLightingMeshes),TEXT("Options"),CPF_Config);` in `SoftDrv.cpp:StaticConstructor`, default 1 (set in `Init` or via `Default.ini [SoftDrv.SoftwareRenderDevice] VolumetricLightingMeshes=True`) |
| S14b | same statement | **uninitialised `Fog` on HP2 particles.** T's mesh fog reads `P->Fog` of every triangle. HP2 feeds `DrawTriangles` from three producers; meshes get `Vert.Fog` from `FLightManager::LightAndFog` (H `UnLight.cpp:538` `Vert.Fog = FPlane(0)`, filled under `PF_RenderFog` at `:635`), decals set `DecalPoints[i]->Fog = FPlane(0,0,0,0)` (`UnRender.cpp:3857`), but **particles never write `.Fog`** (`grep -c Fog UnParticleRn.cpp` = 0; the verts come from `New<FTransTexture>(GMem, MaxVerts)`, `:475`, uninitialised; `SetupColors` writes `.Light` only, `:425-428`, `:773-786`). Particle PolyFlags are `PF_Unlit` + `PF_Masked` for `STY_Masked` (`GetPolyFlags`, `:361-372`) — not translucent/modulated, so T's `FogScale*` is **not** zeroed and garbage (possibly NaN; NaN×0 is NaN anyway) fog would be added to masked particles | gate on the flag Render uses to mean "this batch carries fog": `GMeshFog = VolumetricLighting && VolumetricLightingMeshes && SupportsFogMaps && (PolyFlags & PF_RenderFog);`. Meshes in a fog zone get `PF_RenderFog` (`UnLight.cpp:2565` `Result \|= PF_RenderFog`, returned into the mesh `PolyFlags`), particles do not, decals only with `STY_Modulated` (zero fog, zeroed scale). Identical behaviour for meshes, safe for particles |
| S3 | `SoftDrvPrivate.h:31` `#if !ASM && __INTEL__` | `__INTEL__` comes from the template `UnGnuG.h:16`; HP2's own `UnGnuG.h` spells its platform macros differently | none once the template `UnGnuG.h` lands (prerequisite, `template-core.md`). On wasm `__INTEL__` is undefined, so `GIsMMX` is the Core global: the Unix core must leave `GIsMMX=0` there (it does in the template) |

### 3.4 File-by-file plan (exact edits)

Start: `cp /home/ion/engines/engine-ut99/Source/SoftDrv/Src/{DrawPoly.cpp,DrawSurf.cpp,DrawTile.cpp,Hit.cpp,Line.h,Line1.h,Line2D.cpp,SoftDrv.cpp,SoftDrvPrivate.h} Source/SoftDrv/Src/`
and `cp …/engine-ut99/Source/SoftDrv/WEB-NOTES.md Source/SoftDrv/`. Keep `Src/SoftDrv.dsp` for reference.
Then:

| File | Edits on top of T's text | Verify |
|------|--------------------------|--------|
| `Src/SoftDrvPrivate.h` | (1) `:10` `#include "RenderPrivate.h"` → `#include "Render.h"` (resolves to `Render/Inc/Render.h` through the Render target's `PUBLIC Inc` and SoftDrv's own `PRIVATE ../Render/Inc`). (2) `:176` → `DECLARE_CLASS(USoftwareRenderDevice,URenderDevice,CLASS_Config,SoftDrv)`. (3) `:219-220` replace `void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet );` and `void DrawGouraudPolygon( … );` by HP2's three lines (`H:156-158`, quoted in H4). (4) add `BITFIELD VolumetricLightingMeshes;` after `BilinearFiltering` (`:189`) (S14). Keep everything else of T (IN/OUT undef, `GIsMMX false`, `SOFTDRV_MAX_SCREEN`, `_rotl` shim, `DOUBLE D`) | `grep -n 'DrawGouraudPolygon' Src/*.h` → only comments; the class has no pure virtual left unimplemented (link error `undefined reference to vtable for USoftwareRenderDevice` otherwise) |
| `Src/SoftDrv.cpp` | add the `VolumetricLightingMeshes` `UBoolProperty` after T:25 (`BilinearFiltering`) and `VolumetricLightingMeshes = 1;` in the constructor/`Init` defaults (T sets its flags at `:43-50`); nothing else — H's `SoftDrv.cpp` is byte-identical to epic, so T's file carries no HP2 loss. `IMPLEMENT_PACKAGE(SoftDrv)`/`IMPLEMENT_CLASS` (T:12-13) stay | `diff E/SoftDrv.cpp H/SoftDrv.cpp` = empty (confirmed) |
| `Src/DrawSurf.cpp` | (1) T:21098 signature → `void USoftwareRenderDevice::DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD PolyFlags, BYTE cAlpha )`. (2) the **six** `Surface.PolyFlags` → `PolyFlags` inside that function: T:21109, 21110, 21111, 21127, 21627, 22254 (`grep -n 'Surface\.PolyFlags'` lists exactly these six and nothing outside the function). Leave `Surface.FogMap`/`Surface.LightMap` (T:20664, 20846, 20998, 21308, 21467-21468, 21516) alone — they are pointers, not flags. (3) optional cosmetic `0.67f` (T:21608-ish, `MipMult *= 0.67`) | after the edit `grep -c 'Surface\.PolyFlags' Src/DrawSurf.cpp` = 0; `diff -w E/DrawSurf.cpp new/DrawSurf.cpp` must contain every H hunk of §3.2 H7/H8 |
| `Src/DrawPoly.cpp` | (1) T:4484-4488: rename `void USoftwareRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* SpanBuffer )` → `DrawTriangles( FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* SpanBuffer )`, `guard(USoftwareRenderDevice::DrawTriangles)`, and insert before it `INT USoftwareRenderDevice::MaxVertices() { return 256; }` (H:4366-4370). (2) T:4494 → `GMeshFog = VolumetricLighting && VolumetricLightingMeshes && SupportsFogMaps && (PolyFlags & PF_RenderFog);` (S14b). (3) replace T's commented-out `//#debug /* if (PolyFlags & PF_Translucent) … */` block right after the `SizeY` check with HP2's `PF_Highlighted\|PF_Translucent` masked-preference block (H:4379-4385). (4) T:4667-4731 (`Point0 = Pts[0]; … if (NumPts == 7) …`) → HP2's three loops (H:4546-4593, quoted in §3.2), keeping T's `TriangleFunc = &USoftwareRenderDevice::…` lines (T:4662-4665) above them. (5) optional: H5 swap at T:3719-3720 (dead code at ASM=0). Leave T's S13 fix (T:4205-4217) — a different site | `grep -n 'DrawGouraudPolygon' Src/DrawPoly.cpp` → none; the only `DFG`/`DFB` lines read `Fog.Y`→DFG, `Fog.Z`→DFB |
| `Src/Line2D.cpp` | none (T's `#ifdef PLATFORM_MSVC` gate is required; `0.56f` optional) | — |
| `Src/DrawTile.cpp`, `Hit.cpp`, `Line.h`, `Line1.h` | none (H = E) | — |
| `CMakeLists.txt` (H, existing) | already right: six `.cpp`, `ASM=0`, `SOFTDRV_MAX_SCREEN` cache var, `PRIVATE ../Render/Inc ../Render/Src`, links `Render Engine Core`. Only nit: `../Render/Src` is unnecessary once `Src/UnSpan.h`/`Src/Amd3d.h` are deleted (§1.4) — keep or drop, harmless | the wasm preset already sets `"SOFTDRV_MAX_SCREEN": "8192"` (H `Source/CMakePresets.json:36`, = T `CMakePresets.json:30`) — keep it |
| `WEB-NOTES.md` | copy T's verbatim; it refers to `NSDLViewport.cpp`'s `SoftFB` and `softdrv-matrix.cjs`, which are template/NSDLDrv/frontend facts that apply unchanged (HP2 takes NSDLDrv from the template). Change `DM-Codex` in the §6 example to an HP2 map (e.g. `Adv1Willow`, which also exercises the Darklight fix of §1.3) | — |
| `Config/System/Default.ini` `[SoftDrv.SoftwareRenderDevice]` (`:213-221`) | add `BilinearFiltering=False`, `VolumetricLightingMeshes=True` (optional — `CPF_Config` defaults from the constructor otherwise); the web ini set is `hp2-web-config`'s | — |

Resulting ASM picture (identical to how T builds today): `DrawPoly.cpp` — `InnerGouraudMMX32/15/16` and
`MMXFlashTriangle` bodies behind `#if ASM`, the `#if !ASM` Pentium kernels compiled; `DrawSurf.cpp` — the
104 `__asm` all inside `#if ASM` or the MSVC-only stubs, `!ASM` stub block (with S18's extra names) compiled;
`DrawTile.cpp` — 19 `__asm` inside `#if ASM`; `SoftDrvPrivate.h` — `FastSqrt` `__asm` inside `#if ASM`.

Dependencies from other reports: template `Core/Inc/UnGnuG.h` (`ASM 0`, `__INTEL__`, `DOUBLE`, empty
`DLL_EXPORT` — with HP2's `UnGnuG.h:97` `DLL_EXPORT` = `extern "C"`, T's `class DLL_EXPORT
USoftwareRenderDevice` would not parse); `PLATFORM_X86` (H top `CMakeLists.txt:156`) / `PLATFORM_WIN32`
(`:161`) / `PLATFORM_MSVC` (`:188`) — present; the template NSDLDrv's `SoftFB` path (`NSDLViewport.cpp`) for
the web present; Render's `Inc/Render.h` (this report §1).

### 3.5 SoftDrv acceptance / verification

- [ ] `--target SoftDrv` builds clean on native, web and MinGW with only the known unused-variable warnings.
- [ ] `nm -C libSoftDrv.a | grep 'USoftwareRenderDevice::\(DrawTriangles\|MaxVertices\|DrawComplexSurface\)'` — all three defined; no `DrawGouraudPolygon` symbol from SoftDrv.
- [ ] Native `./Game -SOFTWARERENDERER -log`: menu, then a level at 640×480, 1920×1080 and a >1200-tall
      size (WEB-NOTES §6) — the `SizeY`-indexed tables are the classic overrun.
- [ ] Lumos: in a level with Lumos surfaces (grep `bLumosOn` users in `HGame/Classes`), cast Lumos in
      software mode — appearing surfaces must appear, always-on ones draw translucent (tests H8).
- [ ] Particles (any `HPParticle` effect, e.g. spell FX) in a **fog zone** show no colour garbage (tests S14b).
- [ ] Meshes in a fog zone are fogged (S11/S12 through HP2's `LightAndFog`).
- [ ] `Adv1Willow` tree shadows dark (depends on the Render `Merge` fix, §1.3).

## 4. Editor

(Re-verified this run. Where it differs from Appendix Z's §4 — the run-1 text — it says "correction"/"new".)

### 4.0 Bottom line

**The Editor package must be linked into the HP2 game binary, on every target.** HP2 content needs it at
**boot**, not only in some maps: `Entry.unr` (loaded unconditionally by `UGameEngine::Init`,
H `Engine/Src/UnGame.cpp:169`) imports the texture `Editor.B_PlyrOn` → the engine loads `System/Editor.u` →
the linker's `Verify()` (H `Core/Src/UnLinker.cpp:293-296`, `for( INT i=0; i<Summary.ImportCount; i++ )
VerifyImport( i );`) must resolve `Editor.u`'s own import `Class Editor.TransBuffer`, which is **not an
export of `Editor.u`** but the native C++ class `UTransBuffer` (H `Editor/Src/UnEdTran.cpp:375-377`,
`IMPLEMENT_CLASS(UTransBuffer)` at `:598`). It is referenced from script by `EditorEngine.uc:31`
`var const transbuffer Trans;`. Without the native Editor the result is exactly the template's documented
failure (T `Source/CMakeLists.txt:24-39`, ufront 2.39): `Failed to load 'Editor': Can't find Class
Editor.TransBuffer in file '../System/Editor.u'` — for HP2 that happens on the Entry map, i.e. before the menu.
HP2's `Source/CMakeLists.txt:35` already has `option(BUILD_EDITOR … ON)`, `:65` forces it ON, `:377-379`
`add_subdirectory(Editor)` + `list(APPEND INSTALL_TARGETS Editor)` — **keep all three**.

### 4.1 What the template builds (facts, T `Editor/CMakeLists.txt`)

- 27 sources: `CoolBsp UnBsp UnEdAct UnEdCam UnEdClick UnEdCnst UnEdCsg UnEdFact UnEdRend UnEdSrv UnEdTran
  UnEditor UnMeshEd UnScrCom UnShadow UnSyntax UnTopics UnVisi Editor UBatchExportCommandlet UBrushBuilder
  UConformCommandlet UMakeCommandlet UMergeDXTCommandlet UnEdExp UnMeshLP UnParams` (+ `NullEdHook.cpp` when
  `NOT TARGET_IS_WINDOWS OR NOT BUILD_WINDRV`; `Win32EdHook.cpp` otherwise — **that file does not exist in
  T's tree** (`ls T/Editor/Src/Win32EdHook.cpp` → no such file), so T's GUI branch is dead/unfinished; HP2 must
  not copy the `else()` branch; `LOW_MEMORY` → `UnEdTran.cpp Editor.cpp` only, SKIP).
- Excluded from epic's set: `EdHook.cpp` (Win32 GUI glue), `TTFontImport.cpp` (GDI TrueType rasteriser),
  `UMasterCommandlet.cpp` (`../../Setup/Inc/Setup.h`). **TTFontImport: excluded, not ported** — the task's
  "TTFontImport?" is answered: T's copy is byte-identical to epic (Δ 0) and is not in the CMake list.
- Links `Engine Core` (+ `Window` only on the dead GUI branch); `EDITOR_EXPORTS UPACKAGE_NAME=Editor`;
  `PUBLIC Inc`, `PRIVATE Src`.
- `NullEdHook.cpp` (T, 99 lines): **HP2 already has a byte-identical copy** (`cmp` → identical,
  `H/Editor/Src/NullEdHook.cpp`). It supplies `NotifyDestroy/NotifyPreChange/NotifyPostChange/NotifyExec`,
  `UpdatePropertiesWindows`, `HookExec`, `EdCallback`, `GTopic/GItem/GValue`, `UEngine* Engine`.

### 4.2 Template vs epic — every Editor hunk and its HP2 counterpart

T's own Editor hunks are small (per-file Δ vs E: `UnMeshLP` 52, `UnScrCom` 20, `UnVisi` 18, `UnEditor` 17,
`UnParams` 11, `UnBsp` 8, `UnEdAct`/`UnEdCsg` 6, `UnEdCam` 5, `UConform`/`UnEdExp`/`UnEdFact` 4,
`UMake`/`UnEdSrv` 2, `UnEdTran`/`UnShadow` 1, `EditorClasses.h` 2; all others 0). One more fix lives in
**Engine**: `PUSH_HIT`.

| # | T site | Change | Why | H site (verified this run) | H action |
|---|--------|--------|-----|-----------------------------|----------|
| ED1 | `Engine/Inc/UnCamera.h:306-310` | `PushHit(expr,sizeof(expr))` → `sizeof(decltype(expr))` | T's comment: "sizeof(expr) where expr is a temporary hit-proxy (e.g. `HMenuToggleButton()`) parses under clang (Emscripten) as sizeof of a FUNCTION TYPE" | H `Engine/Inc/UnCamera.h:323` (old form). Users: Editor `UnEdCam.cpp:1863,1942,2017,2022,2086,2156,2233,2456,2503,2537`, `UnEdRend.cpp:85,223,231,246,310,339,439,725,732`, `UnMatinee.cpp:174`; **also Render** `UnRender.cpp:3448`, `UnSprite.cpp:878` | **PORT** (one-line macro change in Engine; owned by `template-engine.md`, but Editor *and* Render depend on it — `UnEdCam.cpp:2017 PUSH_HIT(Frame,HMenuToggleButton())` is exactly the clang failure) |
| ED2 | `UnScrCom.cpp:1351` | `Min(1024, Input+InputPos-Start+1)` → `Min<INT>(…)` | `INT` vs `ptrdiff_t` (pointer difference is `long` on LP64 / `int` on i386+wasm32; deduction fails where they differ) | `UnScrCom.cpp:1425` | **PORT** (cheap; needed if anyone ever builds 64-bit) |
| ED3 | `UnScrCom.cpp:1618-1661` | `TFieldIterator<UProperty> It(Function);` hoisted, `for( ; It && …` ×2 | `It` used by later loops | `UnScrCom.cpp:1697` (decl in `for`), reused at **`:1732` and `:1770`** (HP2 has one more reuse than epic, inside the 433 optional-parm code) — careful: `:1686` declares a *different* `It` in an inner block (iterator-cast check); do not merge them | **PORT**: `TFieldIterator<UProperty> It(Function);` right before `:1697`; `for( ; It && …` at 1697, 1732, 1770 |
| ED4 | `UnScrCom.cpp:1741` | `UClass* Test;` hoisted in `ConversionCost` | `check(Test!=NULL)` after the loop | `UnScrCom.cpp:1879` loop, use `:1881` | **PORT** |
| ED5 | `UnScrCom.cpp:2918` | `FLabelRecord* LabelRecord;` hoisted | used after the loop | `UnScrCom.cpp:2893` and **`:3058`** (use `:3066`; HP2 has a second copy) | **PORT** at both |
| ED6 | `UnScrCom.cpp:4544`, `4775`, `4846` | `INT i;` / `INT iNest;` hoisted | used after the loop | `UnScrCom.cpp:4703` (use `:4710`), `:5042`, `:5112` | **PORT** |
| ED7 | `UnEdCam.cpp:547` | `FPolyVertex::FPolyVertex(…)` → `FPolyVertex(…)` inside the struct | extra qualification is an error in GCC/clang | `UnEdCam.cpp:622` | **PORT** |
| ED8 | `UnVisi.cpp:599, 634, 1230` | `AddPortal` → `&FEditorVisibility::AddPortal` (also `BlockPortal`, `TagZonePortalFragment`) | pointer-to-member needs `&Class::` | `UnVisi.cpp:599`, `:634`, `:1236` | **PORT** |
| ED9 | `UnVisi.cpp:726, 960, 1084, 1202, 1308, 1345, 1506, 1824` | `FActorLink* Link;`, `FPortal* ClipPortal;`, `INT i;`, `INT iActor, i;`, `INT j;` hoists | for-scope | `UnVisi.cpp:727`(use 730), `962`(972), `1114`(1131), `1173`(1187), `1244`(1248), `1316`(1321), `1358`(1363), `1515`(1523), `1835`(1838) | **PORT** (§4.5 list) |
| ED10 | `UMakeCommandlet.cpp:98` | `for( It=TObjectIterator<UClass>(); …)` → `for( TObjectIterator<UClass> It; …)` | `It` was for-scoped; also `TObjectIterator` has no usable copy-assignment | `UMakeCommandlet.cpp:204` | **PORT** |
| ED11 | `EditorClasses.h:78` | `#endif NAMES_ONLY` → `#endif` | "extra tokens at end of #endif" | `Editor/Inc/EditorClasses.h:80` (and Engine `EngineClasses.h`, other report) | **PORT** |
| ED12 | `UnParams.cpp:22-31, 171` | `#ifndef UNREAL_STATIC … #endif` around `GetFVECTOR`×2, `GetSUBSTRING`, `GetFROTATOR`×2 | Engine's `UnParams.cpp` defines the same five; whole-archive static link → duplicate symbols | H `Editor/Src/UnParams.cpp:25,55,86,105,145` (`EDITOR_API`) vs H `Engine/Src/UnParams.cpp:25,55,86,105,145` (`ENGINE_API`) — same five, same signatures; H has **no guard** | **PORT** (copy T's guard + comment). `GetBEGIN/GetEND/SetFVECTOR/GetFSCALE` (H `:166,183,198,209`) stay: Editor-only |
| ED13 | `UnEditor.cpp:44-46` | removed `: EditPackages( E_NoInit )` | under GCC the `E_NoInit` ctor left the config array as garbage before `LoadConfig` | `UnEditor.cpp:46` | **PORT** (the ctor change only) |
| ED14 | `UnEditor.cpp:99-116` | "maximqad: bruh fix" 4-package fallback if `EditPackages.Num()` is absurd, and a `printf` per package | ut99dc workaround | `UnEditor.cpp` `Init` | **SKIP** — HP2 has 11 `EditPackages` (`Default.ini:327-337`); the fallback would silently drop `HGame` etc. ED13 removes the cause |
| ED15 | `UnEdExp.cpp:173, 195` | `for( INT i…` / `ItE->Names.Num()` instead of out-of-scope `i` | for-scope | `UnEdExp.cpp:190`, `:211` | **PORT** |
| ED16 | `UConformCommandlet.cpp:173, 195` | `for( INT i=0;i<NumFlags;…)` | for-scope | `UConformCommandlet.cpp:191`, `:213` (the `for( INT i` at `:182`) — plus HP2-only `:109/113` (`Pages[i]` after the loop) | **PORT** |
| ED17 | `UnBsp.cpp` (7), `UnEdAct.cpp` (6), `UnEdCam.cpp` (3), `UnEdCsg.cpp` (6 incl. `FStaticBrushIterator It(Level);` in `csgRebuild`), `UnEdFact.cpp` (4), `UnEdSrv.cpp` (2), `UnEdTran.cpp` (1), `UnShadow.cpp` (1), `UnMeshLP.cpp` (26 loops given their own `INT`) | function-top `INT i;` etc. | for-scope | see §4.5 (HP2 lines) | **PORT** |

### 4.3 HP2 Editor inventory (H vs E; Δ = changed lines, `diff -w`)

| H file | E / T / H lines | Δ H vs E | Portability verdict |
|--------|-----------------|---------:|---------------------|
| `EdHook.cpp` | 563 / 563 / 438 | 257 | **Windows-only**: `:13 <windows.h>`, `:14 <shlobj.h>`, `:16 "Window.h"`, `:18 "..\..\Core\Inc\UnMsg.h"`, `:41-68` `FMallocWindows.h … UnEngineWin.h`. Exclude (NullEdHook replaces it) |
| `TTFontImport.cpp` | 302 / 302 / 367 | 71 | **Windows-only** (`:13 <windows.h>`, GDI `CreateFont`). Exclude. `UTrueTypeFontFactory` stays declared in `EditorPrivate.h` without `IMPLEMENT_CLASS` — same as T, links because nothing references `StaticClass()` |
| `UMasterCommandlet.cpp` | 458 / 458 / 640 | 186 | UMOD/installer builder: `:10 "FFileManagerArc.h"`, `:11 "../../Setup/Inc/Setup.h"`, `FArchiveHeader GArc` (`:24`). HP2 does ship `Source/Setup/Inc/Setup.h` (no `#include`s, declares `USetup*`), so it *might* compile header-only — **unverified and not wanted** (Setup is "Windows-only … reference", `docs/SOURCE-LAYOUT.md:15`). **Exclude — correction: HP2's current `Editor/CMakeLists.txt` lists it (line `"Src/UMasterCommandlet.cpp"`); remove it.** Its three classes (`MasterCommandlet`, `UpdateUModCommandlet`, `ChecksumPackageCommandlet`) are named only in `Assets/System/Editor.int:2,10,11` (a `ucc` listing), never by content |
| `UPS2ConvertCommandlet.cpp` | — / — / 70 | HP2-only | PS2 sound conversion; portable but PSX2-purpose → exclude (already excluded) |
| `UPSX2MusicCommandlet.cpp` | — / — / 0 | HP2-only | **0-byte file**, not in `Editor.dsp` → ignore (already excluded) |
| `UPkgCommandlet.cpp` | — / — / 254 | HP2-only | includes only `EditorPrivate.h`; portable → build |
| `UnMatinee.cpp` (+ `Inc/UnMatinee.h`, `#pragma once`) | — / — / 296 | HP2-only | includes `EditorPrivate.h`, `UnRender.h` (Engine), `UnMatinee.h`; needs ED1; for-scope `iActor` 194/210 → build |
| `Inc/UnSkeletalImport.h` | — / — / 105 | HP2-only | `.psk/.psa` structs for `UnMeshEd.cpp`/`UnMeshLP.cpp` |
| `UnEdCam.cpp` | 1984 / 1987 / 2893 | 1135 | vertex editing, bezier, labels. **New findings (not in run 1):** (a) `:49 __declspec(dllexport) INT GLastScroll=0;` and `:65-69 __declspec(dllexport) bool GPawnsLabel / GTriggersLabel / GLightsLabel / GMoversLabel / GSlabelLabel;` — `__declspec` is **not defined** by the template's `UnGnuG.h` (only `UnVcWin32.h:45-47` maps `DLL_EXPORT` to it) → hard error on Linux GCC and wasm clang. The importers are the Windows-only `UnrealEd` (`UnrealEd/Src/Main.cpp:80-84`, `UnrealEd/Inc/BrowserTexture.h:15`) and `EdHook.cpp:31`. ACTION: replace `__declspec(dllexport)` with `DLL_EXPORT` (empty on GCC, `__declspec(dllexport)` on MSVC) at 49, 65-69. (b) `wcscmp(tName,TEXT(""))` at **2313, 2345, 2377, 2442** (run 1 listed three; 2442 is the fourth; 2410 is commented out). `TCHAR` is `char` here (T `Core/Inc/Core.h:61` `typedef ANSICHAR TCHAR;`, no `_UNICODE`), so `wcscmp(const char*, const char*)` does not compile → `appStrcmp(tName,TEXT(""))!=0` (or `*tName`); drop `#include <wchar.h>` (`:14`). (c) ED1, ED7 |
| `UnEdSrv.cpp` | 2574 / 2576 / 4015 | 1515 | 433 server: brush clip, skeletal import, audio; `#include "../../Engine/Src/UnPath.h"` (relative, resolves); `HWND=` parsed into a `DWORD` (`:3348`, portable); `#pragma DISABLE_OPTIMIZATION` (`:19`) → with T's `UnGnuG.h:87` `DISABLE_OPTIMIZATION` is a `_Pragma("GCC push_options")…` macro, so `#pragma DISABLE_OPTIMIZATION` expands to a nested pragma — T compiles epic's identical lines in `UnEdAct/UnEdRend/UnEdSrv`, so it is known-good. 13 for-scope sites |
| `UnMeshEd.cpp`, `UnMeshLP.cpp` | 351→1372, 1330→2098 | 1063 / 930 | skeletal import + LOD; includes `UnSkeletalMesh.h` (Engine), `UnSkeletalImport.h`; 1 + 11 for-scope sites |
| `UnScrCom.cpp` (+ `UnScrCom.h` Δ15) | 5724 / 5730 / 6044 | 518 | 433 compiler; ED2-ED6 |
| `UnEdClick.cpp`, `UnEdRend.cpp` | 321→564, 539→870 | 293 / 355 | bezier/matinee clicks and drawing; `UnEdClick.cpp:515/525` `PosInList` for-scope; ED1 users |
| `UnEditor.cpp` | 807 / 820 / 1040 | 265 | ED13; `:212/221` for-scope `i` |
| `UnEdCsg.cpp`, `UnEdAct.cpp`, `UConformCommandlet.cpp`, `UMakeCommandlet.cpp` | | 221 / 205 / 112 / 141 | 433 features; for-scope per §4.5; ED10, ED16 |
| `UnBsp.cpp`, `UnShadow.cpp`, `UnVisi.cpp`, `UnEdExp.cpp`, `UnEdFact.cpp`, `UBrushBuilder.cpp`, `Editor.cpp`, `CoolBsp.cpp`, `UBatchExport`, `UMergeDXT`, `UnEdTran.cpp/.h` | | 55 / 48 / 26 / 18 / 26 / 12 / 5 / 2 / 2 / 2 / 2+2 | near-epic; T's hunks apply by analogy. `UnEdFact.cpp` already `#if 0`s `UMusicFactory` (`:926-973`, "TG ALPHA", matching `EditorPrivate.h:167-176`) — consistent, nothing to do. `UnEdFact.cpp:980-1074` `#pragma pack(push,1)`/`pop` around PCX/BMP headers — GCC honours it, keep |
| `UnEdCnst.cpp`, `UnSyntax.cpp`, `UnTopics.cpp/.h` | | 0 | identical to epic |
| `EditorPrivate.h` | 268 / 268 / 299 | 75 | 4-arg `DECLARE_CLASS(…,Editor)`; `UTrueTypeFontFactory` gains fields; `UMusicFactory` `#if 0` |
| `Inc/Editor.h` | 504 / 504 / 563 | 75 | 433 `UEditorEngine` API |
| `Inc/EditorClasses.h` | 82 / 82 / 84 | 4 | `UBrushBuilder` gains `BitmapFilename`, `ToolTip` (must match `Assets/System/Editor.u`; **never regenerate** from `.uc`); ED11; `#if _MSC_VER #pragma pack(push,4)` (`:6-7`, `:82-83`) → on GCC the pack comes from `-fpack-struct=4`/the template's global flags, same as T |
| `Classes/` | 12 `.uc` + `Editor.upkg` | — | reference only (`Editor.u` ships in `Assets/System`) |
| `Src/Editor.dsp` | | | 37 `SOURCE=` lines (`:96-327`): every `.cpp` above **except** `NullEdHook.cpp` and `UPSX2MusicCommandlet.cpp`, i.e. it built `EdHook`, `TTFontImport`, `UMasterCommandlet`, `UPS2ConvertCommandlet` |

### 4.4 Content analysis — which Editor classes HP2 content needs at runtime

Method (re-run this session): `scratchpad/pkgnames.py` — a UE1 package-header reader (name, import and
export tables; compact-index aware; handles v64+ length-prefixed names, so `strings -e l` is not needed and
would miss the import graph) — over **all 194 packages** in `Assets/` (`*.u *.unr *.utx *.uax *.umx`:
`System` 11, `Maps` 48, `Maps/Studies` 41, `Maps/Oldfiles` 7, `Maps/templates` 4, `Textures` 52, `Sounds` 29,
`PackageSources/HPDialog/SOUNDS` 2). Every file parsed (194 × `ver=`). Output filtered to non-empty
Editor imports:

| Package | Editor import | Shipped/boot relevance |
|---------|---------------|------------------------|
| `System/Editor.u` (v79) | `Class Editor.TransBuffer` (class package `Core`) | **native class** — resolved only through the linked C++ Editor |
| `Maps/Entry.unr` (v76) | `Texture Editor.B_PlyrOn` | **boot**: `UnGame.cpp:169` `LoadMap( FURL(TEXT("Entry")) … )`, kept resident |
| `Maps/Quidditch.unr`, `Maps/Quidditch_Intro.unr` (v79) | `Texture Editor.Bad` | shipped Quidditch levels |
| `Maps/Studies/TrenchRun.unr`, `Maps/Studies/QuidditchStadium.unr`, `Maps/Studies/EntryCV1.unr` | `Texture Editor.Bad` | **new this run** (run 1 scanned only `Maps/*.unr`): developer study maps, not in the shipped map set — no action, but they would need Editor too |
| everything else (incl. all `Textures/*.utx`, `Sounds/*.uax`, `System/{Core,Engine,Fire,HGame,HPModels,HPParticle,HProps,HPSounds,UnrealShare,UWindow}.u`) | none | — |

Conclusions and ACTIONS:

1. **Link the native Editor into the game** (all three targets; static web build whole-archives it via
   `INSTALL_TARGETS`, same as T). The runtime-required native classes are `UTransBuffer` (+ its base
   `UTransactor`, `UnEdTran.cpp:599`), and the two `RF_Native` script classes exported by `Editor.u`,
   `EditorEngine` (`UnEditor.cpp:23`) and `BrushBuilder` (`UBrushBuilder.cpp:206`): when `Editor.u`
   loads, their exports are bound to the registered C++ classes, so those two files must be in the build too.
   The ten `*Builder` subclasses are pure script.
2. `Editor.B_PlyrOn` / `Editor.Bad` are ordinary textures exported by `Editor.u` — no C++ needed beyond (1).
3. **`Editor.u` must be in the web boot set** (it is fetched when `Entry.unr` resolves its imports) —
   `ufrontend-target-contract.md` / `precache.json` owner.
4. Nothing in the content names commandlets, factories, exporters or `UnrealEd` classes; they are built only
   because they sit in the same files / support `ucc make`. They add no runtime cost beyond code size.
5. Verification: native `./Game -log` must show Entry loading with no `Failed to load 'Editor'` line; with
   `-DBUILD_EDITOR=OFF` (negative test) the same log must show the TransBuffer failure.

### 4.5 CMake for HP2 Editor

Recommended `Source/Editor/CMakeLists.txt` `SRC_FILES` = the current HP2 file **minus
`Src/UMasterCommandlet.cpp`** (30 files):

```cmake
set(SRC_FILES
  "Src/CoolBsp.cpp"            "Src/Editor.cpp"              "Src/UBatchExportCommandlet.cpp"
  "Src/UBrushBuilder.cpp"      "Src/UConformCommandlet.cpp"  "Src/UMakeCommandlet.cpp"
  "Src/UMergeDXTCommandlet.cpp" "Src/UPkgCommandlet.cpp"     "Src/UnBsp.cpp"
  "Src/UnEdAct.cpp"            "Src/UnEdCam.cpp"             "Src/UnEdClick.cpp"
  "Src/UnEdCnst.cpp"           "Src/UnEdCsg.cpp"             "Src/UnEdExp.cpp"
  "Src/UnEdFact.cpp"           "Src/UnEditor.cpp"            "Src/UnEdRend.cpp"
  "Src/UnEdSrv.cpp"            "Src/UnEdTran.cpp"            "Src/UnMatinee.cpp"
  "Src/UnMeshEd.cpp"           "Src/UnMeshLP.cpp"            "Src/UnParams.cpp"
  "Src/UnScrCom.cpp"           "Src/UnShadow.cpp"            "Src/UnSyntax.cpp"
  "Src/UnTopics.cpp"           "Src/UnVisi.cpp"              "Src/NullEdHook.cpp"
)
```

Keep the rest of HP2's file (`PUBLIC Inc`, `PRIVATE Src`, `target_link_libraries(Editor Engine Core)`,
`EDITOR_EXPORTS UPACKAGE_NAME=Editor`). Engine's `Inc` must be reachable for `UnRender.h`,
`UnSkeletalMesh.h` (it is, through `Engine`'s `PUBLIC` include); `UnEdSrv.cpp:16` reaches
`../../Engine/Src/UnPath.h` by relative path. Update the header comment: it currently omits
`UMasterCommandlet` from the exclusion list. Do not add T's `LOW_MEMORY`/`Win32EdHook` branches.

**Windows-only / unwanted files to exclude** (with the proving line):

| File | Evidence | Reason class |
|------|----------|--------------|
| `Src/EdHook.cpp` | `:13 #include <windows.h>`, `:16 #include "Window.h"`, `:68 #include "UnEngineWin.h"`, `NE_EdInit( HWND, HWND )` export | Win32 GUI |
| `Src/TTFontImport.cpp` | `:13 #include <windows.h>`, GDI `CreateFont` | Win32 GDI |
| `Src/UMasterCommandlet.cpp` | `:11 #include "../../Setup/Inc/Setup.h"` | Setup/UMOD (Windows tool) |
| `Src/UPS2ConvertCommandlet.cpp` | PS2 sound conversion | PSX2 (policy) |
| `Src/UPSX2MusicCommandlet.cpp` | 0 bytes | empty |
| `Src/Editor.dsp` | MSVC6 project | reference only |

### 4.6 Compile fixes to apply to HP2's Editor (complete checklist, HP2 line numbers)

Structural fixes (ED1-ED16 of §4.2 plus the two new ones):

- [ ] ED1 `Engine/Inc/UnCamera.h:323` `PUSH_HIT` → `sizeof(decltype(expr))` (Engine owner; blocks Editor and Render).
- [ ] ED12 `Editor/Src/UnParams.cpp`: `#ifndef UNREAL_STATIC` before `:22` (first helper comment), `#endif` after the second `GetFROTATOR` (`:160`).
- [ ] ED13 `UnEditor.cpp:46` remove `: EditPackages( E_NoInit )`.
- [ ] ED7 `UnEdCam.cpp:622` drop `FPolyVertex::`.
- [ ] **new** `UnEdCam.cpp:49, 65-69` `__declspec(dllexport)` → `DLL_EXPORT`.
- [ ] **new/extended** `UnEdCam.cpp:2313, 2345, 2377, 2442` `wcscmp` → `appStrcmp`; remove `:14 #include <wchar.h>`.
- [ ] ED8 `UnVisi.cpp:599, 634, 1236` `&FEditorVisibility::…`.
- [ ] ED10 `UMakeCommandlet.cpp:204` `for( TObjectIterator<UClass> It; It; ++It )`.
- [ ] ED11 `Inc/EditorClasses.h:80` `#endif`.
- [ ] ED2 `UnScrCom.cpp:1425` `Min<INT>`.
- [ ] ED3 `UnScrCom.cpp:1697/1732/1770` hoist `It` (leave the inner-block `It` at `:1686` alone).
- [ ] ED4-ED6 `UnScrCom.cpp:1879`, `2893`, `3058`, `4703`, `5042`, `5112`.
- [ ] ED15 `UnEdExp.cpp:190`, `:211` (`ItE->Names.Num()`).
- [ ] `UnEdCsg.cpp:209/239` (`csgRebuild(Level, bVisibleOnly)`): add `FStaticBrushIterator It(Level);` right after `guard(UEditorEngine::csgRebuild);` — exactly T's fix (T `UnEdCsg.cpp:259`); the `for( FStaticBrushIterator It(Level); …)` at `:209` may stay (it shadows), and the `for( It=FStaticBrushIterator(Level); …)` at `:239` then assigns to the function-level `It` (T compiles the identical line at T `:305`, so the assignment is known to work).

For-scope reuse sites (hoist-aware scanner `scratchpad/forscope3.py` = run 1's line scanner + a filter
that drops a hit when the name is already declared at function level; calibration: **1** residual hit on T
(a false positive in `UMakeCommandlet`), **54** on E, i.e. it finds what T fixed; it does **not** see
iterator-object reuse (`It`), which ED3/ED10 cover). HP2, 72 hits — format `decl-line→use-line`:

| File | Sites (`for` declaring the name → first out-of-scope use) |
|------|--------------------------------------------------------------|
| `UConformCommandlet.cpp` | `i` 109→113; `i` 182→191 (and 213) |
| `UnBsp.cpp` | `i` 695→700; 799→806; 901→908; 2278→2282; 2342→2345; 2448→2465; 2524→2628 |
| `UnEdAct.cpp` | `i` 96→101; 154→160; 209→214; 260→265; 401→414; 567→572 |
| `UnEdCam.cpp` | `i` 398→404; `i` 642→659; `x` 1112→1143; `vertex` 2782→2786; `i` 2841→2859 |
| `UnEdClick.cpp` | `PosInList` 515→525 |
| `UnEdCsg.cpp` | `i` 612→631; `brush` 620→625; `i` 838→852; `i` 1033→1040 |
| `UnEdExp.cpp` | `i` 187→190; `i` 203→211 (ED15) |
| `UnEdFact.cpp` | `j` 321→324; `i` 1007→1011; `i` 1388→1399; `i` 1494→1498 |
| `UnEditor.cpp` | `i` 212→221 |
| `UnEdSrv.cpp` | `i` 208→214; `poly` 228→241; `actor` 1187→1234; `i` 1925→1928; 2401→2413; 2750→2776; 3456→3467; 3504→3518; 3533→3549; 3606→3609; 3859→3865; 3881→3887; 3919→3929 |
| `UnEdTran.cpp` | `i` 311→316 |
| `UnMatinee.cpp` | `iActor` 194→210 |
| `UnMeshEd.cpp` | `i` 939→972 |
| `UnMeshLP.cpp` | `i` 225→237; `i` 325→331; `p` 831→864; `i` 1039→1081; `w` 1098→1107; `f` 1225→1244; `t` 1500→1513; `i` 1643→1686; `w` 1698→1707; `f` 1822→1840; `s` 2054→2063 |
| `UnScrCom.cpp` | `Test` 1879→1881; `LabelRecord` 3058→3066; `i` 4703→4710 (+ ED3/ED5/ED6 sites the scanner cannot see: 1697, 2893, 5042, 5112) |
| `UnShadow.cpp` | `i` 455→463 |
| `UnVisi.cpp` | `Link` 727→730; `ClipPortal` 962→972; `i` 1114→1131; 1173→1187; 1244→1248; 1316→1321; `iActor` 1358→1363; `i` 1515→1523; `j` 1835→1838 |

Idiom: either T's `INT i;` at the top of the function (leaving `for( INT i…)` shadows as they are — T's
choice in `UnBsp/UnEdAct/UnEdCsg/UnVisi`), or give each later loop its own `for( INT i=0; …)` (T's choice in
`UnMeshLP`, `UConformCommandlet`, `UnEdExp`). Where the variable's **value** is read after the loop
(`if( i==Level->Actors.Num() )`, `check(Test!=NULL)`, `if( !Link )`, `if( j >= i )`, `UnEdSrv` `appSprintf(…, i)`),
only the hoist is correct. Expect the compiler to report a handful the scanner missed (macros, `while`-scoped
names); fix them the same way. Everything else in the Editor compiles unchanged on T's toolchain, by analogy
with T's near-identical files.

## 5. Cross-report dependencies

Things this subject cannot compile or run without, and who owns them (report file names in
`docs/porting/analysis/`). "Blocks" = the first target that fails without it.

| # | Needed by | Symbol / header / fact | Must come from | Owner | Blocks |
|---|-----------|------------------------|----------------|-------|--------|
| D1 | Render, Fire, SoftDrv, Editor | `ASM 0`, `ASMLINUX 0`, `ASM3DNOW 0`, `GCC_ALIGN`, empty `DLL_EXPORT`/`DLL_IMPORT`, `__INTEL__` on x86, `typedef double DOUBLE`, `DISABLE_OPTIMIZATION` as `_Pragma` | template `Core/Inc/UnGnuG.h` (T:82-95, 179, 206-212) replacing HP2's Loki one (H `:18` `ASMLINUX 1`, `:97` `DLL_EXPORT extern "C"`) | `template-core.md` | Render (`UnRender.cpp:1036` AT&T asm), SoftDrv (`class DLL_EXPORT USoftwareRenderDevice`) |
| D2 | Render `UnRender.cpp:703` | `FTime appProcessSeconds()` | declared `Core/Inc/UnFile.h:263`, only defined in `UnVcWin32.cpp:501` → write it in `UnUnix.cpp` | `template-core.md` / `hp2-core-engine-specifics.md` (§ "absent") | link of `Game` |
| D3 | Render, Editor | `PUSH_HIT` with `sizeof(decltype(expr))` | Engine `Inc/UnCamera.h:323` (T:310) | `template-engine.md` | Editor `UnEdCam.cpp:2017` on clang/wasm |
| D4 | Render `DrawWorld` (R15b) | `ENGINE_API extern FLOAT GAutoFovFrameBase` | Engine `UnCamera.h/.cpp` (T:449 / :250), only if AutoFOV is ported | `template-engine.md` | nothing (optional) |
| D5 | Editor (static link) | `UNREAL_STATIC` defined for every static build | top-level `CMakeLists.txt` (H `:63`, `:87`) | `template-core.md` | duplicate `GetFVECTOR`… at link (ED12) |
| D6 | Editor, Render, SoftDrv, Fire | 4-arg `DECLARE_CLASS(…, Pkg)` (H `Core/Inc/UnObjBas.h:326,336`) | HP2 Core (present); T's files copied into HP2 (SoftDrv) must be converted | this report §3.4 (SoftDrv), `template-drivers.md` (NullDrv etc.) | compile |
| D7 | SoftDrv | the 433 `URenderDevice` face (`Engine/Inc/UnRenDev.h:75-78`): 5-arg `DrawComplexSurface`, pure `MaxVertices`/`DrawTriangles`, non-pure `DrawGouraudPolygon` forwarding to the fan branch (`Engine/Src/UnCamera.cpp:199-203`), `GetOsSurface` (`:93`, default `return NULL`; only WinDrv calls it, `WinViewport.cpp:939,1005` — SoftDrv/NSDLDrv need **no** override) | HP2 Engine (present) | this report §3 | SoftDrv vtable |
| D8 | (not this subject, found here) | **HP2's `NullDrv` still has the UT99 face** (`NullDrv/Src/NullDrvPrivate.h:22` 3-arg `DECLARE_CLASS`, `:35` 3-arg `DrawComplexSurface`, `:36` `DrawGouraudPolygon`, no `MaxVertices`/`DrawTriangles`) | fix per `template-drivers.md` row "NullDrv" (it already lists this, incl. `MaxVertices()` = 256) | `template-drivers.md` | NullDrv compile |
| D9 | Editor `UnEdCam.cpp` | `TCHAR` = `char` (no `UNICODE`/`_UNICODE`) — makes `wcscmp` a hard error | template `Core.h:61`, HP2 build flags | `template-core.md` / `hp2-core-engine-specifics.md` (it lists the same `wcscmp` class of bug in `UnClass.cpp:1439`) | Editor |
| D10 | Render | `FStats GStat`, `STATS 1`, `FTime` arithmetic with `appSeconds()` (`UnRender.cpp:3677-3679`) | HP2 Core (`UnStat.h:157`, `UnBuild.h:56`) with template `UnGnuG.h`'s FTime-returning `appSeconds` re-added | `template-core.md` | Render |
| D11 | Fire | `UBitmap::__Client` set by `UClient::UClient()` (`Engine/Src/UnCamMgr.cpp:24`); `NoFractalAnim` config | NSDLDrv's client must derive from `UClient` and run its ctor (the template's does) | `template-drivers.md` | fractals frozen / NULL deref in `ConstantTimeTick` |
| D12 | Fire | layout: `-fpack-struct=4` (wasm) / MSVC bitfields (MinGW) + F8 `GCC_ALIGN(4)` | top-level flags (T `BUILD.md` "Two ABI differences") | `template-core.md`, `windows-mingw-template.md` | first map with a `FireTexture` (`VERIFY_CLASS_OFFSET`) |
| D13 | SoftDrv (web) | `SoftFB` blit path in `NSDLViewport.cpp`; `SOFTDRV_MAX_SCREEN=8192` in the wasm preset (H `CMakePresets.json:36`, present) | template NSDLDrv | `template-drivers.md`, `web-layer-template.md` | software rendering on the web |
| D14 | Editor content | `System/Editor.u` and `Maps/Entry.unr` in the web boot set / precache | frontend target (`precache.json`, boot closure) | `ufrontend-target-contract.md`, `hp2-game-boot-and-content.md` | first `LoadMap(Entry)` |
| D15 | Render (Darklight) | nothing external — but the `Merge` fix (§1.3) changes lighting for **every** renderer, so XOpenGL/UTGLR screenshot baselines must be taken *after* it lands | — | `renderer-forks.md` (awareness) | visual regression tests |
| D16 | MinGW | `PLATFORM_WIN32`/`__MINGW32__` for SoftDrv `IN/OUT` undef and Fire `__int64` typedef guard; `PLATFORM_MSVC` for `Line2D.cpp` | top-level CMake (H `:156,161,188`) | `windows-mingw-template.md` | MinGW build |

## 6. Open questions / verification

Resolved this run (were open in run 1, Appendix Z §6):

- **AutoFOV (R15b)** — HP2 `Engine/Classes/Inventory.uc:239` has the same `0.9/PawnOwner.FOVAngle` offset and
  `Weapon.RenderOverlays` is reachable (Harry's `PlayerCalcView`, not `bBehindView`), so the base-FOV swap is
  needed **iff** Engine AutoFOV is ported; decision belongs to `template-engine.md` (§1.1 R15).
- **`URenderDevice::DrawGouraudPolygon`** — the base forwards to `DrawTriangles( …, NULL, NumPts, … )`
  (`Engine/Src/UnCamera.cpp:199-203`), so SoftDrv must not define it (§3.2).
- **`cAlpha` on SoftDrv** — the only callers of `DrawComplexSurface` are `UnRender.cpp:3541` (Lumos,
  `iAlpha` 0/255 on span devices) and `:3550` (255); nothing else passes a graded alpha. SoftDrv may ignore it.
- **Is Editor needed at runtime?** — yes, at boot (§4.0/§4.4). Scan widened to all 194 packages; three extra
  hits are developer study maps only.
- **`UnTest.cpp`** — drop it (§1.4).

Still open — how to verify each:

1. **`FLightManager::Merge` dark-light fix (§1.3)**: confirm visually in `Adv1Willow` (199 `Darklight`
   actors) after the fix; before the fix, the tree shadows should appear as bright patches. If retail
   screenshots are available, compare. Also confirm the MSVC copy's `static` asm-only variables only warn.
2. **`GCC_ALIGN(4)` on `SparkType`/`FX_Heat`/`DropType` (F8)**: confirm at first boot through
   `VERIFY_CLASS_OFFSET`/`VERIFY_CLASS_SIZE` in `UFireTexture::PostLoad`/`Init` (a mismatch is an
   `appErrorf`, so it cannot pass silently), on all three targets (wasm `-fpack-struct=4`, MinGW
   `-mms-bitfields` change the arithmetic).
3. **`FX_Heat GCC_PACK(4)` → `GCC_ALIGN(4)`**: believed layout-neutral; verified by (2).
4. **SoftDrv S14b (`PF_RenderFog` gate on mesh fog)**: check with a particle system inside a fog zone
   in software mode (no colour garbage) and a mesh in the same zone (fogged).
5. **`UMasterCommandlet.cpp` header-only compile**: it may actually compile against HP2's
   `Setup/Inc/Setup.h` + `Core/Inc/FFileManagerArc.h`; irrelevant for the game — keep it excluded unless
   someone wants `ucc master` on Linux.
6. **Editor for-scope list (§4.6)** is heuristic (72 hits, calibrated on T/E); the compiler is the
   authority — expect a few extra sites (iterator objects, `while`-scoped names) and a few benign hits.
7. **`#pragma DISABLE_OPTIMIZATION`** in `UnEdAct.cpp:11`, `UnEdRend.cpp:11`, `UnEdSrv.cpp:19` with the
   template's `_Pragma` macro: T compiles epic's identical lines, so expected fine; if clang warns
   "unknown pragma", ignore.
8. **UnParticleRn.cpp** was never compiled by GCC/clang (HP2 shipped MSVC only); the scanners found no
   for-scope leaks, but expect template-deduction / narrowing warnings (`Min`/`Max` on mixed `INT`/`FLOAT`).
   Build `--target Render` early and fix as reported.
9. **Content packages imported by `HGame.u`/`HPParticle.u`** (`HPEdit`, `3DWizardCards`, `SpellShapes`): **correction to run 1** (Appendix Z §4.3/§6.5 called them missing) — they exist as `Assets/Textures/HPEdit.utx`, `3DWizardCards.utx`, `SpellShapes.utx`; they are texture packages, found through `Paths=../Textures/*.utx`. Nothing to do here; `hp2-game-boot-and-content.md` owns the boot closure.

---

## Appendix Z — previous run's report (2026-09-25), kept verbatim for history (SUPERSEDED by §0-§6; UNVERIFIED)

### Render, Fire, SoftDrv and the stripped Editor — template vs epic v400 vs HP2 build 433

Analyst key: `template-render-fire-softdrv-editor` · written 2026-09-25 · read-only analysis (no file under
`Source/` was touched; the CMake skeleton that already exists in the HP2 repo is reviewed, not rewritten).

Paths used throughout:

| Alias | Path |
|-------|------|
| **T** (template) | `/home/ion/engines/engine-ut99/Source` |
| **E** (epic v400, the common ancestor) | `/home/ion/epic-sources/ut99-v400` |
| **H** (HP2 build 433, the port target) | `/home/ion/hp2-ufrontend/HarryPotter2/Source` |

Every diff below was produced with `diff --strip-trailing-cr -w -u` (E is CRLF, T and H are LF). "Changed
lines" = count of `<`/`>` lines of a plain `diff -w`.

---

#### 0. Executive summary (read this if nothing else)

1. **The Editor package is a boot-time dependency, not an option.** `H/Engine/Src/UnGame.cpp:169` loads the
   `Entry` map unconditionally (`if( !LoadMap( FURL(TEXT("Entry")), NULL, NULL, Error ) ) appErrorf(...)`);
   `Assets/Maps/Entry.unr` imports the texture `Editor.B_PlyrOn`; loading `Editor.u` resolves its own import
   `Editor.TransBuffer` (a **native-only** class: `H/Editor/Src/UnEdTran.cpp:377`
   `DECLARE_CLASS(UTransBuffer,UObject,CLASS_Transient,Editor)`, not exported by `Editor.u`). Without the native
   Editor package linked the engine dies at the first `LoadMap`. Additionally `Quidditch.unr` and
   `Quidditch_Intro.unr` import `Editor.Bad`. The PLAN's decision "Editor ON unless content proves otherwise"
   is therefore settled: **ON**, and the content analysis is in §4.3.
2. **Render:** the template's Render hunks are (a) GCC portability (`INT i;` hoists, `Min<INT>`/`Clamp<INT>`,
   a `(FTransform*)` cast, the deleted MSVC/AT&T asm in `Pipe`), (b) two ufront fixes worth porting
   (`HackRaster` 2880 → `MAX_RASTER_Y` 8192 + clamp; the AutoFOV base-FOV swap around `eventRenderOverlays`),
   (c) Dreamcast/LOW_MEMORY branches to ignore. HP2's Render is heavily 433 (skeletal meshes, particle
   systems, Lumos surfaces, dynamic `TArray` caches, `FStats`-based stats) — **HP2's text is authoritative;
   port (a)+(b) by hand.** HP2 Render has 16 preprocessor ASM/platform gates (table §1.3); every one has a
   plain-C `#else` or is dead once `ASM=0`, `ASMLINUX=0`, `ASM3DNOW=0` and `__PSX2_EE__` undefined — which
   **requires taking the template's `UnGnuG.h`** (HP2's own `UnGnuG.h:18` still `#define ASMLINUX 1`, and the
   `asm volatile ("` multi-line string blocks it would enable are not valid C++ on any modern GCC).
3. **Fire:** the template's `UnFractal.cpp` is epic + 8 tiny GCC fixes; HP2's is epic + 433 (`FTime`,
   `NoFractalAnim` gating, `Prime()`, `GetLastUpdateTime()`, `catch(...)` recovery inside the WATERASM path
   only). All five ASM families (`RANDASM FIREASM WATERASM WETASM ICEASM` = `ASM`) have complete C fallbacks;
   the `.asm` files are correctly dropped. Port: the `Y`/`t`/`Flash` for-scope hoists, `Min<INT>`/`Clamp<INT>`,
   the `!defined(__MINGW32__)` guard on the `__int64` typedef (`H/Fire/Inc/UnFractal.h:19`), and the
   `GCC_ALIGN(4)` on `SparkType` / `FX_Heat` / `DropType` (GCC reuses base-class tail padding; MSVC does not —
   HP2's own `VERIFY_CLASS_OFFSET(U,FireTexture,SparkType)` will catch a mismatch at runtime).
4. **SoftDrv:** the template's SoftDrv is epic + ~1 100 lines of ufront work (portable fog maps, bilinear /
   dither filtering, the `SOFTDRV_MAX_SCREEN` table bound, `#if ASM` around the three MMX-only bodies, the
   `_rotl` shim, the `DR/DB/DG` edge-gradient bug fix, the `+0x010101` rounding fix, `IN`/`OUT` undef for
   MinGW, no 2D lines on GCC). HP2's SoftDrv is epic + **only 127 changed lines** (the 433 face:
   `DrawComplexSurface(…,PolyFlags,cAlpha)`, `MaxVertices()`, `DrawTriangles()`; a 4-arg `DECLARE_CLASS`;
   `Render/Inc/Render.h` include; one `DFG/DFB` swap fix in the MMX edge setup). **Plan: start from the
   template's nine files and re-apply HP2's 127 lines** (§3.3 gives the exact per-file recipe) — the
   opposite direction is ~10× more work. HP2's `SoftDrvPrivate.h:10` includes
   `"..\..\Render\Inc\Render.h"` with backslashes: it cannot compile on Linux as is.
5. **Editor:** the template builds 27 epic files + `NullEdHook.cpp`; excludes `EdHook.cpp` (Win32 GUI),
   `TTFontImport.cpp` (GDI) and `UMasterCommandlet.cpp` (includes `../../Setup/Inc/Setup.h`, a Win32-only
   package). **HP2's current `Editor/CMakeLists.txt` lists `UMasterCommandlet.cpp` — it must come out**
   (`H/Editor/Src/UMasterCommandlet.cpp:10-11` include `FFileManagerArc.h` and `../../Setup/Inc/Setup.h`).
   `UPSX2MusicCommandlet.cpp` is a 0-byte file. The rest of HP2's list (incl. the 433-only `UnMatinee.cpp`,
   `UPkgCommandlet.cpp`) is portable. Compile fixes to port: `PUSH_HIT sizeof→decltype` (it lives in
   **Engine** `UnCamera.h:323`), `Min<INT>` (`UnScrCom.cpp:1425`), the `UNREAL_STATIC` duplicate-symbol guard
   in `UnParams.cpp` (HP2's still has none — its Engine and Editor both define `GetFVECTOR/GetFROTATOR/
   GetSUBSTRING`), `FPolyVertex::FPolyVertex` (`UnEdCam.cpp:622`), member-pointer args in `UnVisi.cpp`
   (599/634/1236), `for( It=TObjectIterator…` (`UMakeCommandlet.cpp:204`), `wcscmp` on ANSI `TCHAR`
   (`UnEdCam.cpp:2313/2345/2377`), `#endif NAMES_ONLY` (`EditorClasses.h:80`), and ~150 MSVC6 for-scope
   variable reuses (heuristic counts per file in §4.5).

Cross-cutting prerequisites this subject depends on (owned by other reports): template `UnGnuG.h`
(`ASM/ASMLINUX/ASM3DNOW = 0`, `GCC_ALIGN`, `__cdecl`), `UnCamera.h` `PUSH_HIT` (Engine), `GAutoFovFrameBase`
(Engine `UnCamera.h:449` / `UnCamera.cpp:250` in the template), `appProcessSeconds()` returning `FTime`
(HP2 `Core/Inc/UnFile.h:263` — implemented only in `UnVcWin32.cpp:501` today; the Unix core must provide it,
Render calls it in `URender::PostRender`), `FStats GStat` (`Core/Inc/UnStat.h:157`).

---

#### 1. Render

##### 1.1 Template (T) vs epic v400 (E) — every hunk, classified for HP2

Files identical between T and E: `Render.cpp`, `UnSoftLn.cpp`, `UnSpan.h`, `UnTest.cpp`, `Amd3d.h`.
`RenderPrivate.h`, `UnLight.cpp`, `UnMeshRn.cpp`, `UnMeshRnLOD.h`, `UnRandom.cpp`, `UnRender.cpp`,
`UnSpan.cpp`, `UnSprite.cpp` differ. Classification legend: **PORT** = apply to HP2's text by hand;
**HAVE** = HP2 already has it or the site no longer exists; **SKIP** = Dreamcast/LOW_MEMORY, not wanted.

| # | T file:line | What the template changed | Why | HP2 |
|---|-------------|---------------------------|-----|-----|
| R1 | `RenderPrivate.h:365-372` | Added `#elif defined(PLATFORM_DREAMCAST)` (`frsqrt/fsqrt`) and `#elif defined(PLATFORM_LOW_MEMORY)` (`1.f/sqrtf`) branches to `DivSqrtApprox/DivApprox/SqrtApprox` before the generic `#else` | DC/LOW_MEMORY | **SKIP** — HP2's `Render/Inc/Render.h:234-276` keeps epic's `#if ASM … #else { 1.0/appSqrt(F) … }` — the `#else` is exactly what compiles at `ASM=0` |
| R2 | `UnLight.cpp:340,393` | `INT i;` hoisted before `for( i=0; …)` loops in `SetupTable`-style init code | MSVC6 for-scope leak | **PORT** — HP2 has the same shape at `UnLight.cpp:360,412,427,432` (heuristic §1.5) |
| R3 | `UnLight.cpp:1167,1440-1442,1460-1463,1489-1492,2097,2205,2210` | `Max/Min/Clamp(...)` → `Max<INT>/Min<INT>/Clamp<INT>(...)` | GCC cannot deduce `T` from `(INT, unsigned/enum/…)` mixes | **PORT by analogy** — the same expressions exist in HP2's `UnLight.cpp` (Palette build, `MinU/MaxU` clipping, `SpecialTag` decay). Let the compiler name them |
| R4 | `UnLight.cpp:1628-1629,1650-1651,2237-2238`; `UnLight.cpp:2114-2115` | `FLightInfo* Find;` hoisted out of the search `for(…;…;Find++);` (the value is used after the loop); `INT NewNum=0;` hoisted out of `for( INT i=0,NewNum=0; …)` | for-scope leak / mixed-type init | **PORT** — same idiom in HP2 (`for( FLightInfo* Find=FirstLight; … ); Find->IsVolumetric = 1;`) |
| R5 | `UnMeshRn.cpp:166,419`; `UnMeshRnLOD.h:176,250,308,600` | `INT i;` hoists | for-scope | **PORT** — HP2 `UnMeshRn.cpp:327`, `UnMeshRnLOD.h:166,283` |
| R6 | `UnMeshRn.cpp:429`; `UnMeshRnLOD.h:318` | `Owner->MultiSkins[appCeil((SortedPts[i] - (FTransform*)Samples)/3.f)%8]` — cast so the pointer subtraction is between the same type | GCC rejects `FTransTexture* - FTransSample*` | **HAVE (site gone)** — `grep 'SortedPts\[i\]-Samples'` finds nothing in HP2's 433 mesh renderer |
| R7 | `UnMeshRn.cpp:435`; `UnMeshRnLOD.h:324`; `UnSprite.cpp:770` | `Clamp<INT>( appFloor(...), 0, Count-1 )` | as R3 | **PORT by analogy** if the expression survives in HP2 (`LifeFraction()*Count` sites) |
| R8 | `UnRandom.cpp:119` | `for( INT i=0; i<N_RANDS; i++ )` | `i` was never declared in scope on GCC | **PORT** — HP2 `UnRandom.cpp:119` identical |
| R9 | `UnRender.cpp:95-100` | `#ifdef PLATFORM_LOW_MEMORY GDynMem.Init(32768)` | LOW_MEMORY | **SKIP** — HP2 uses `GDynMem.Init( 65536, TEXT("GDynMem") )` (433 tagged mem stacks) |
| R10 | `UnRender.cpp:681-897` (E) → `686-703` (T) | **Deleted the whole MSVC `__asm` block and both AT&T `asm volatile` blocks of `Pipe()`**, keeping only the plain-C transform / clip-number / projection code | no x86 asm on GCC/wasm; the AT&T blocks use the pre-C++11 multi-line string literal (`asm volatile ("\n …` split across lines) that GCC ≥ 4 rejects | **PORT (mechanically) or leave gated** — HP2 `UnRender.cpp:897-1136` has the identical structure `#if ASM {__asm} #else … #if ASMLINUX {asm volatile} #else {C} #endif … #if LINUXASM … #endif #endif`. With `ASM=0`, `ASMLINUX=0`, `LINUXASM` undefined the C paths compile untouched. Deleting the asm (as T did) is optional hygiene. HP2 additionally wraps a PSX2 `Pipe()` at `868-886` in `#if __PSX2_EE__` — dead, may be deleted (owner: no PSX2 deltas) |
| R11 | `UnRender.cpp:1382-1383` | `FSceneNode* Frame;` hoisted in `CreateChildFrame` | for-scope | **PORT** |
| R12 | `UnRender.cpp:1451-1471` | `FRasterSpan HackRaster[2880]` → `HackRaster[MAX_RASTER_Y]` with `#define MAX_RASTER_Y 8192` (480 on DC) **and** `EndY = Min<INT>( EndY, MAX_RASTER_Y - 1 );` at the top of `SetupRaster` | any viewport taller than 1200 px overran the span table → heap corruption / wasm OOB; the web canvas is device-pixel sized | **PORT** — HP2 `UnRender.cpp:1977` still `FRasterSpan HackRaster[2880];//max y res!!`, `SetupRaster` at 1979 has no clamp. Drop the DC branch |
| R13 | `UnRender.cpp:1491-1496,1832-1835` | `Clamp<INT>` / `::Max<INT>` / `::Min<INT>` in `SetupRaster` and `BoundVisible` | as R3 | **PORT by analogy** — HP2's `BoundVisible` was rewritten for 433 (`const FCoords& BoxCoords`, `Result.MaxZ`) but keeps `::Max(BoxMinX,0)` etc. |
| R14 | `UnRender.cpp:2055,2492,2506,2563,2607,2674,2724,2987,3081-3093` | `INT i;` / `INT i,j;` / `INT Pass;` / `FActorLink* Other;` hoists | for-scope | **PORT** — HP2 sites `UnRender.cpp:3116,3232,3236,3275,3296,3693,3788,3799,3825` (heuristic) |
| R15 | `UnRender.cpp:3213-3251` | **AutoFOV (ufront 2.29):** around `Actor->eventRenderOverlays(...)` swap `FovActor->FovAngle` to `GAutoFovFrameBase` and restore it afterwards (`Frame->ComputeRenderSize()`), inside an `if( Actor && Actor->GetClass() && Canvas && Canvas->GetClass() )` guard | UT99's `Inventory.uc` divides `PlayerViewOffset` by `FOVAngle` — with a widened effective FOV the weapon crept forward; script must see the base FOV | **PORT — conditionally.** HP2's `DrawWorld` (`UnRender.cpp:3874-3905`) has the same `eventRenderOverlays` call. Whether HP2's HUD/weapon script divides by `FOVAngle` is a **content question** (grep `FOVAngle` in `Source/HGame/Classes`); apply the swap only if it does, otherwise the guard alone. The globals come from Engine (`T/Engine/Inc/UnCamera.h:449`, `UnCamera.cpp:250`, `UnGame.cpp:1272`) — coordinate with `template-engine.md` |
| R16 | `UnRender.cpp:3215-3218` | `try { … } catch (...)` around `DrawWorld` body kept (E has it) | — | **HAVE (better)** — HP2 already removed the `try/catch` (`UnRender.cpp` hunk `@@ -3363,9 +3874,6 @@`), good for `-fno-exceptions`/Emscripten |
| R17 | `UnSpan.cpp:37` | `int` → `INT` in `FSpanBuffer::AllocIndex` signature | consistency with the declaration | **PORT** (HP2 `UnSpan.cpp` = epic, 2 changed lines) |
| R18 | `UnSprite.cpp:111,441,454,544,603,677` | `INT i;` / `FDynamicItem** Item;` / `FActorLink* Link;` hoists | for-scope | **PORT** — HP2 `UnSprite.cpp:170,490,502` + the `Link`/`Item` searches |

##### 1.2 HP2 (H) vs epic v400 — what build 433 changed (so the merge direction is clear)

| H file | changed lines (E→H) | Nature of the 433 delta |
|--------|--------------------:|-------------------------|
| `Src/Render.cpp` | 2 | `#include "Render.h"`; `GDynMem/GSceneMem` now defined here (`RENDER_API FMemStack GDynMem, GSceneMem;`) |
| `Inc/Render.h` (was `Src/RenderPrivate.h`) | 182 | `FLightManagerBase::Light/Fog` → `LightAndFog()` + `LightParticleSystem()`; `FDynamicItem::MinZ,MaxZ`; `FDynamicSysChild/FDynamicSysParent` (particle systems); `FRenderStats` **removed** (replaced by Core `FStats GStat`, `#include "UnStat.h"`); `URender` gets `WireShow/BlendShow/BoneShow`, `MaxParticleSize`, `FTime` timers, `ParticleStats/EARIStats/EARIDetails/AnimStats`, `AverageTime`; `BoundVisible(…, const FCoords& BoxCoords, …)`; `DrawBox(FBox,FCoords)`, `DrawCylinder`; `PointCache/DynamicsCache` become `static TArray<…>` + `AllocDynamicsCache(UModel*)`; `DrawMesh/DrawLodMesh(Frame, FDynamicSprite*, Owner, Coords, PolyFlags)`; `DrawParticleSystem`; 4-arg `DECLARE_CLASS(URender,URenderBase,CLASS_Config,Render)` |
| `Src/UnRender.cpp` | 1178 | stats rewrite on `FStats`/`FTime`/`appProcessSeconds()`; adaptive `GlobalShapeLODAdjust`/`MaxParticleSize`; `REND WIRE/BONE/BLEND`; `STAT Particle/EARI/Anim/Average`; PSX2 `Pipe`/VU0 clip (dead); `BoundVisible` rewrite; `AllocDynamicsCache` (no `MAX_POINTS` cap any more); **Lumos** surface pass (`PF_LumosAffected/Appear/AlwaysOn`, `#ifndef NO_LUMOS_CODE`, calls `DrawComplexSurface(Frame,Surface,Facet,LumosPolyFlags,(char)iAlpha)`); editor sky-zone lookup; sprites sorted by class and drawn behind masked geometry on span-based devices; decals via `DrawTriangles(…, NULL, DecalNumPoints, …)`; `FDynamicSysParent` children drawn; `try/catch` removed; `LEGEND` blocks unconditional (`Player->GetViewZone`, `IsSurfVisible`) |
| `Src/UnLight.cpp` | 1160 | `FLightInfo` gains `LightDir/MaxSpecDir/RadiusInner/LightDistTab`; `LightDist/LightDistInc` tables replace `LightSqrt`; specular; `LightAndFog`; `LightParticleSystem`; `ToByte`; `FMemCache::FCacheItem`; `spatial_CloudCast` dropped; PSX2 gates |
| `Src/UnMeshRn.cpp` | 323 | skeletal-mesh rendering path, `FDynamicSprite*` plumbing, `DrawTriangles` batching |
| `Src/UnMeshRnLOD.h` | 561 | LOD mesh renderer reworked for the `DrawTriangles`/`MaxVertices` face + `LightAndFog` |
| `Src/UnSprite.cpp` | 406 | `FDynamicSysParent/Child` (particle systems, `bFilterByVolume`), EARI stats, `DrawParticleSystem` dispatch |
| `Src/UnParticleRn.cpp` | HP2-only, 1275 lines | the 433 particle renderer (`#include "UnParticleList.h"`, `"UnParticle.h"` from `Engine/Inc`); **no ASM, no `_MSC_VER`**; only `#ifdef DEBUG_CLIPPING` |
| `Src/UnSoftLn.cpp` | ~343 added | `SolveQuadratic`, `URender::DrawCylinder`, `DrawBox(FBox,FCoords)` — pure C++ |
| `Inc/UnRenderNative.h` | HP2-only, 20 lines | `#if __STATIC_LINK` `AUTO_INITIALIZE_REGISTRANTS_RENDER` — **dead** under the template's `UNREAL_STATIC` scheme (PLAN §2: HP2's `__STATIC_LINK` stays undefined). Harmless; keep or delete |
| `Inc/UnSpan.h`, `Inc/Amd3d.h` | 0 | byte-identical duplicates of `Src/UnSpan.h`, `Src/Amd3d.h` (`diff` = identical). `Render.h:21,27` includes `"Amd3d.h"`/`"UnSpan.h"`; with both `Inc/` (PUBLIC) and `Src/` (PRIVATE) on the include path either copy resolves. **ACTION:** delete the `Src/` copies (the `.dsp` lists `.\Amd3d.h`, `.\UnSpan.h` and `..\Inc\Render.h`, i.e. the drop was already inconsistent) |
| `Src/UnSpan.cpp`, `UnRandom.cpp`, `UnTest.cpp` | 2 each | include rename only |

Conclusion for the merge: for every Render file, **HP2's text is the base**; the template hunks R2–R8,
R11–R15, R17–R18 are re-applied by hand (none of them conflicts with a 433 feature; they are one-liners).

##### 1.3 Every ASM / `_MSC_VER` / platform gate in HP2 Render (and what compiles at ASM=0)

| H file:line | Gate | Contents | At `ASM=0 ASMLINUX=0 ASM3DNOW=0`, `__PSX2_EE__` undefined | Action |
|-------------|------|----------|------------------------------------------------------------|--------|
| `Inc/Render.h:234` | `#if ASM` … `#else` (271) … `#endif` (275) | MSVC `POWER_ASM` macro + `__asm` `SqrtApprox` | `#else`: `DivSqrtApprox = 1.0/appSqrt(F)`, `DivApprox = 1.0/F`, `SqrtApprox = appSqrt(F)` | none (optionally delete the asm) |
| `Src/UnLight.cpp:546`, `:582` | `#if !__PSX2_EE__` | `PointSquared` + specular lighting | compiled (gate is true) | delete the gates (PSX2 delta) |
| `Src/UnLight.cpp:661` … `847` … `1044` | `#if _MSC_VER` / `#else` / `#endif` | two definitions of `FLightManager::Merge`: MSVC one (contains `#if ASM __asm … #else C #endif` at 727) and GCC one (contains `#if ASMLINUX asm volatile(" … #else C #endif` at 905/1022) | the GCC `Merge` at 849 with its plain-C loop at 1022-1033 (`Dest[j] = Stream[j] + GET_COLOR_DWORD(Palette[NewSrc[j]]); … saturation`) | none; **but `ASMLINUX` must be 0** — the `asm volatile ("` block at 909-1020 is a multi-line string literal and does not parse on modern GCC |
| `Src/UnLight.cpp:655,1846` | `#pragma warning` | MSVC pragmas | ignored (`-Wunknown-pragmas` noise only) | none |
| `Src/UnRender.cpp:68` | `#if __PSX2_EE__` | `sizeof(FVector)==16` checks | `#else` branch: `check(sizeof(FVector)==12); check(sizeof(FCoords)==48)` | delete PSX2 branch |
| `Src/UnRender.cpp:249,605` | `#if __PSX2_EE__` | `BigFont` for stats | `#else`: `SmallFont` | delete |
| `Src/UnRender.cpp:871-886` | `#if __PSX2_EE__` | PSX2 `Pipe()` | `#else`: the x86 `Pipe()` (below) | delete |
| `Src/UnRender.cpp:897` | `#if ASM` (MSVC `__asm`, 898-1030) / `#else` (1031) | FPU transform/outcode/project | `#else` path | none (T deleted the asm; optional) |
| `Src/UnRender.cpp:1036` | `#if ASMLINUX` (AT&T, 1038-1085) / `#else` (1086) | clip numbers | `#else`: `ClipXM = Frame->PrjXM * Result.Point.Z + Result.Point.X; …` | none, requires `ASMLINUX=0` |
| `Src/UnRender.cpp:1098` | `#if LINUXASM` (sic) / `#else` | projection | `LINUXASM` is never defined anywhere → `#else`: `Result.RZ = Frame->Proj.Z / Result.Point.Z; …` | none |
| `Src/UnRender.cpp:1148-1620` | `#if ASM3DNOW` | `#pragma pack(8)`, `TransformAndComputeOutcodeAndProject`, `AMD3DClip`, `AMD3DClipBspSurf` (3DNow! `__asm` via `Amd3d.h` `_emit` macros) | not compiled (`UnBuild.h:66` `ASM3DNOW 0`; template `UnGnuG.h:212` also 0) | none |
| `Src/UnRender.cpp:1627` | `#if ASM3DNOW if( GIs3DNow ) return AMD3DClipBspSurf(…)` | dispatch | not compiled | none |
| `Src/UnRender.cpp:1644-1727` | `#if __PSX2_EE__` | VU0 `lqc2/vmul` inline asm in `ClipBspSurf` | `#else` (1728): the `Pipe()` loop | delete (PSX2) |
| `Inc/Amd3d.h` (515 lines) | none | pure `#define pfadd(dst,src) { _asm _emit … }` macros | macros only expand inside the `ASM3DNOW` block → harmless | keep (or delete with the 3DNow block) |
| `Src/UnTest.cpp:18` | none | `virtual void __stdcall xyzzy(...)` | **`__stdcall` is not a keyword on Linux GCC** — HP2's `UnGnuG.h` defines `__cdecl` (line 104) but not `__stdcall`; the template's `UnGnuG.h:104` defines `STDCALL` and leaves `__stdcall` alone because MinGW owns it | **drop `UnTest.cpp` from `Render/CMakeLists.txt`** (the template's list omits it; it is a 1997 compiler-test stub with no code the engine calls) |

Verification: `grep -n -E '#\s*(if|elif).*(ASM|_MSC_VER|PSX2|LINUX)' H/Render/Src/*.cpp H/Render/Inc/*.h`
gives exactly the 16 lines above (plus `UnParticleRn.cpp`'s `DEBUG_CLIPPING`, which is not a platform gate).

##### 1.4 `Render.dsp` vs `Render/CMakeLists.txt`

`H/Render/Src/Render.dsp` `SOURCE=` lines 97-183: `Render.cpp UnLight.cpp UnMeshRn.cpp UnMeshRnLOD.h
UnParticleRn.cpp UnRandom.cpp UnRender.cpp UnSoftLn.cpp UnSpan.cpp UnSprite.cpp UnTest.cpp Amd3d.h
..\Inc\Render.h UnSpan.h`. The existing `H/Render/CMakeLists.txt` compiles all ten `.cpp` (T's compiles
eight: no `UnParticleRn.cpp` — HP2-only — and **no `UnTest.cpp`**). Differences to act on:

- **Remove `Src/UnTest.cpp`** (see `__stdcall` above).
- Keep `Src/UnParticleRn.cpp` (433 particle renderer; portable).
- `target_include_directories(PUBLIC Inc PRIVATE Src)` is right; SoftDrv's CMake already adds `../Render/Inc`.
- `T/Render/CMakeLists.txt:14-17` has a Dreamcast `-O2` override for `UnLight.cpp` — not wanted.

##### 1.5 MSVC6 for-scope reuse heuristic (Render + Fire)

A scan for `for( v=` where the nearest declaration of `v` in the same function was a `for( INT v=` header
(script: `scratchpad/forscope.py`; false positives possible where an outer `INT v;` exists in a nested block,
false negatives for non-`for` reuses such as Fire's `Y = (Ydimension - 2);`):

| H file | hits (line numbers) |
|--------|---------------------|
| `Render/Src/UnLight.cpp` | 360, 412, 427, 432 |
| `Render/Src/UnMeshRn.cpp` | 327 |
| `Render/Src/UnMeshRnLOD.h` | 166, 283 |
| `Render/Src/UnRandom.cpp` | 119 |
| `Render/Src/UnRender.cpp` | 3116, 3232, 3236, 3275, 3296, 3693, 3788, 3799, 3825 |
| `Render/Src/UnSprite.cpp` | 170, 490, 502 |
| `Fire/Src/UnFractal.cpp` | 180, 186, 1319, 1338 (+ non-`for` reuses `Y =` at 305, 342, 430) |

These are exactly the sites the template hoisted (`INT i;` etc.). GCC reports each as
"'i' was not declared in this scope" — fix them as they come.

##### 1.6 Render action checklist

- [ ] Take the template's `Core/Inc/UnGnuG.h` first (`ASM 0 / ASMLINUX 0 / ASM3DNOW 0`, `GCC_ALIGN`) — prerequisite for every file below.
- [ ] `Render/CMakeLists.txt`: drop `Src/UnTest.cpp`.
- [ ] Delete `Render/Src/UnSpan.h` and `Render/Src/Amd3d.h` (identical duplicates of `Inc/`).
- [ ] `UnRender.cpp:1977`: `HackRaster[2880]` → `HackRaster[MAX_RASTER_Y]` (8192) and clamp `EndY` in `SetupRaster` (template hunk R12, without the DC branch).
- [ ] `UnRender.cpp` `DrawWorld` (~3890): the `Actor && Actor->GetClass() && Canvas && Canvas->GetClass()` guard; AutoFOV base swap only if HP2 script divides by `FOVAngle` (R15).
- [ ] Delete every `#if __PSX2_EE__` block in `UnRender.cpp` (68, 249, 605, 871, 1644) and `UnLight.cpp` (546, 582) — keep the `#else` text.
- [ ] Hoist for-scope variables at the 20 lines in §1.5; `Min<INT>/Max<INT>/Clamp<INT>` where GCC complains (R3, R7, R13); `INT` in `UnSpan.cpp:37`.
- [ ] Confirm Core provides `appProcessSeconds()` (`UnRender.cpp` PostRender: `float ProcessTime = appProcessSeconds().GetFloat();`) and `FStats GStat` with the fields `UnRender.cpp` `DrawStats` reads (`EARIActorNames`, `ParticleRasterTime`, …) — `Core/Inc/UnStat.h` has them; `STATS 1` is `UnBuild.h:56`.
- [ ] `Inc/UnRenderNative.h`: leave (dead under `UNREAL_STATIC`).

---

#### 2. Fire

##### 2.1 Template vs epic — all hunks

| # | T file:line | Change | HP2 |
|---|-------------|--------|-----|
| F1 | `UnFractal.cpp:148-149` | `INT t;` hoisted before the sine-table loops | **PORT** — HP2 `UnFractal.cpp:177-186` (`for( INT t=0…` then `for( t=0…` twice) |
| F2 | `UnFractal.cpp:279,404` | `Y = (Ydimension - 2);` → `DWORD Y = …` (the loop variable had gone out of scope) | **PORT** — HP2 `UnFractal.cpp:305,342,430` |
| F3 | `UnFractal.cpp:1293,1313` | `INT Flash;` hoisted | **PORT** — HP2 `1319,1338` |
| F4 | `UnFractal.cpp:3913-3927` | `Min(…,255)` → `Min<INT>` in the wave palette | **PORT** — same code in HP2 (`UWaveTexture` palette) |
| F5 | `UnFractal.cpp:4270,4584,4746` | `Clamp(…)` → `Clamp<INT>` (`WaveTable`, `RenderTable`) | **PORT** |
| F6 | `FractalPrivate.h:18-22` | `#if !_MSC_VER && !defined(__MINGW32__) typedef SQWORD __int64;` | **PORT** to `H/Fire/Inc/UnFractal.h:19` (`#if !_MSC_VER typedef SQWORD __int64;` — a syntax error under MinGW where `__int64` is a keyword) |
| F7 | `FractalPrivate.h:110-116` | `PLATFORM_LOW_MEMORY` spark limits | **SKIP** |
| F8 | `FractalPrivate.h:197,202,315` | `BYTE SparkType GCC_ALIGN(4);`, `BYTE FX_Heat GCC_PACK(4)` → `GCC_ALIGN(4)`, `BYTE DropType GCC_ALIGN(4);` | **PORT** to `UnFractal.h:174,179,292` — see §2.4 |

`FireClasses.h` is identical in all three trees.

##### 2.2 HP2 vs epic — the 433 delta (all portable)

`#include "UnFractal.h"` (+ `UnFireNative.h`, `__STATIC_LINK`-only, dead); `FTime LTimeTotal1…` and
the `__int64 TotalTime*` globals moved from the header into the `.cpp` (better than epic: no globals defined
in a header); `FLOAT FakeAtan`; `(BYTE)-128`; **`catch(...)` in `UWaterTexture::CalculateWater` sets
`PrimeCurrent=255; bRealtime=0; __Client->NoFractalAnim=1`** — this `try/catch` is *inside `#if WATERASM`*
(`UnFractal.cpp:2136-2170`), so it is not compiled at `ASM=0` (good: no `try` under Emscripten's
`-fignore-exceptions`); `UFractalTexture::Prime()` (warms up 48 frames when `NoFractalAnim`);
`ConstantTimeTick()`/`Tick()` bodies gated on `__Client && !__Client->NoFractalAnim`; `GetLastUpdateTime()`;
extra `VERIFY_CLASS_OFFSET(U,Bitmap,MaxColor)`, `VERIFY_CLASS_OFFSET(U,Texture,BumpMap)` and
`VERIFY_CLASS_SIZE(UBitmap/UTexture)` — **these run at load and will `appErrorf` on any layout mismatch**, a
free layout guard for Fire.

##### 2.3 ASM families and their C fallbacks (HP2 `Fire/Src/UnFractal.cpp`)

`UnFractal.cpp:48-52`: `#define RANDASM ASM`, `FIREASM ASM`, `WATERASM ASM`, `WETASM ASM`, `ICEASM ASM`
("Define these to be either 0 or ASM, never 1"). With `ASM=0` all five are 0.

| Gate | `#if` at | C fallback | Verified |
|------|----------|-----------|----------|
| `ASM` | 93 (`CycleCount`) | `return 0;` (line 110) | yes |
| `RANDASM` | 128 (`SpeedRand`) | 149-152: `SpeedRindex = (SpeedRindex + 1) & 63; return( SpeedRandArr[(SpeedRindex+31)&63] ^= SpeedRandArr[SpeedRindex] );` | yes |
| `RANDASM` | 191 (index init) | `SpeedRindex=0;` both branches | yes |
| `FIREASM` | 210 (`extern "C" __cdecl CalcWrapFire/CalcSlowFire/…P2` from `UnFire.asm`/`UnFireP2.asm`) | `#else`: the C `CalcWrapFire`/`CalcSlowFire` (lines ~225-460, the code F2 fixes) | yes |
| `FIREASM` | 4234 (`ConstantTimeTick` dispatch on `GIsPentiumPro`) | `#else`: calls the C versions | yes |
| `ASM && 0` | 981, 1087 (`MoveSparkXY` etc.) | always the C version (dead `__asm`, same as T:955/1061) | yes |
| `WATERASM` | 2136 (`CalcWaterASM` from `UnWater.asm`, wrapped in `try/catch`) | `#else` 2175+: `Output4Pix` macro C water | yes |
| `WETASM` | 2644 (`__asm` per line) | `#else`: C loop | yes |
| `ICEASM` | 2811/2828 (`GIsPentiumPro` PPro + Pentium `__asm`) → `#else` 3192-3204; 3231/3248 → `#else` 3410 | C ice displacement | yes |

The `.asm` files (`UnFire.asm`, `UnFireP2.asm`, `UnWater.asm`) stay in `Src/` as reference; the CMake list
(`H/Fire/CMakeLists.txt`: `Src/UnFractal.cpp` only) matches the template's exactly. **Nothing in HP2 Fire
references `__stdcall`; `__cdecl` is only in the `FIREASM` externs** (and HP2's `UnGnuG.h:104` defines it away).

##### 2.4 Header: `H/Fire/Inc/UnFractal.h` vs `E/T FractalPrivate.h`

- `UnFractal.h:19` `#if !_MSC_VER typedef SQWORD __int64;` → add `&& !defined(__MINGW32__)` (F6). The
  `__int64` globals at `UnFractal.cpp:82-87` and the `extern`s at `UnFractal.h:39-44` then compile on all three.
- **Layout (F8).** `UFractalTexture` (`UnFractal.h:60-84`) ends with `BYTE DrawPhase; BYTE AuxPhase;` → the
  base object has 2 bytes of tail padding. Under the Itanium ABI GCC places a derived class's first member
  in that padding (`UTexture` is non-POD); MSVC never does, and UnrealScript's `Engine.u`/`Fire.u` layout
  (which `VERIFY_CLASS_OFFSET(U,FireTexture,SparkType)` at `UnFractal.cpp:4078` checks against) starts
  `SparkType` at the 4-aligned base size. Hence the template's `BYTE SparkType GCC_ALIGN(4);` (T:197) and
  `BYTE DropType GCC_ALIGN(4);` (T:315). Apply both to `UnFractal.h:174` and `:292`, and change
  `FX_Heat GCC_PACK(4)` (`:179`) to `GCC_ALIGN(4)` (T:202; `packed` on a scalar is meaningless and warns
  on clang/MinGW). HP2's `UnGnuG.h:85-87` already defines `GCC_PACK`/`GCC_ALIGN`. If in doubt, boot and
  read the `Render size mismatch` / `VERIFY_CLASS_OFFSET` errors — they name the offending class.
- `MAXSPARKSINIT/LIMIT` unchanged (no LOW_MEMORY).

##### 2.5 Fire action checklist

- [ ] `Fire/Inc/UnFractal.h:19` MinGW guard; `:174/:179/:292` `GCC_ALIGN(4)`.
- [ ] `Fire/Src/UnFractal.cpp`: hoists at 177-186 (`t`), 305/342/430 (`DWORD Y`), 1319/1338 (`Flash`); `Min<INT>`/`Clamp<INT>` at the wave palette / `WaveTable` / `RenderTable` sites.
- [ ] Optionally delete the `#if __PSX2_EE__`-free but MSVC-only `#pragma warning` lines (noise only).
- [ ] Build `--target Fire` with `-DASM=0`; the only expected errors are the for-scope ones above.

---

#### 3. SoftDrv

##### 3.1 What the template's SoftDrv is (T vs E, 1 612 diff lines)

| T file | Δ vs E | Content of the template's change |
|--------|-------:|----------------------------------|
| `SoftDrv.cpp` | +6 | `BilinearFiltering` config property (`:25`); `SupportsFogMaps = 1` unconditionally (`:45-50`) instead of `= GIsMMX` (the portable path now has fog) |
| `SoftDrvPrivate.h` | +60 | `#include "RenderPrivate.h"` (via include path, no `..\..\`); `#ifdef PLATFORM_WIN32 #undef IN/OUT` (MinGW: `<windows.h>` defines `IN` as empty and `DrawSurf.cpp`'s bilinear filter has a local named `IN`); **`#if !ASM && __INTEL__ #define GIsMMX false`** (force the Pentium path on native x86 when asm is off); **`SOFTDRV_MAX_SCREEN`** (default 4096, `MaximumX/YScreenSize` both) replacing 1200/2048; the `_rotl/_rotr` shim (`<stdlib.h>` on Win32, `<x86intrin.h>` on `PLATFORM_X86`, inline fallback elsewhere — wasm takes the fallback); `BITFIELD BilinearFiltering;` |
| `DrawPoly.cpp` | +180 | `static int SavedEBP,SavedESP;` (implicit `int` is an error); `FTexSetup` gains `FR,FG,FB`; `FPolyCSetup` gains `FogR/G/B, DFogR/G/B`; **per-vertex volumetric fog on the Pentium Gouraud path** (all 9 span kernels add `+ GPolyC.FogR/G/B`, `GMeshFog` static, `FogScale` per depth, fog edge interpolation in `PentiumFlashTriangle`); **`#if ASM` … `#endif` around the bodies of `InnerGouraudMMX15`, `InnerGouraudMMX16`, `MMXFlashTriangle`** (pure `__asm`; the functions stay declared so the dispatch compiles); the **`DR/DB/DG` edge-gradient swap fix** at 4202-4215 (green stepped by blue's gradient — visible only at `ASM=0`); `TriangleFunc = &USoftwareRenderDevice::MMXFlashTriangle` (GCC needs the qualified member pointer) |
| `DrawSurf.cpp` | +1 060 | `InitTexPentiumFiltered`; `TexFilterMode/KnightRow/KnightU/V`; **portable fog maps** (`FogPhoton[MaximumXScreenSize]`, `FogShade`, `FogPentium`, `FogAttenuate`, `FogAvg`, `LightPassPentiumFog`, twelve `MergePass*Fog` twins and the post-pass swap in `DrawComplexSurface`); **`TexturePassFiltered`** (knight's-move dither or true bilinear); the **`+0x010101` rounding bias** in `LightPentium` (every non-x86 build was 1 LSB dark); `InitBilinearKernel(UBits, iMip)`; `for( INT i…` hoists (`:646,768,771`); `appRound` on `InitLightPentiumDelta` args (`:22195`); the extra MMX stub names (`MMX15MaskedRender8A/B`, `…Fog…`, `MMX32Render8Unrolled…`) so the `!ASM` build links |
| `DrawTile.cpp` | +5 | `static FSpan *SpanIndex[1200]` → `[MaximumYScreenSize]` + `SpanY = Min(SizeY, MaximumYScreenSize)` clamp (the >1200-tall viewport self-loop hang) |
| `Line2D.cpp` | +5 | `#ifdef PLATFORM_MSVC` around the depth-cued line rasteriser — `Line.h`/`Line1.h` rely on MSVC-only macro token-pasting; on GCC/wasm SoftDrv draws no 2D lines (only `Draw2DLine`, used by the editor/debug) |
| `Hit.cpp`, `Line.h`, `Line1.h` | 0 | identical |

Plus `T/SoftDrv/CMakeLists.txt`: `-DASM=0`, `SOFTDRV_MAX_SCREEN` cache var (wasm preset raises it to 8192),
include `../Render/Src`; and `WEB-NOTES.md` (why native and web take different code paths, why `SoftFB` is
sized in `Lock`, the `.bss` cost of the static tables, the `-O3`-only overrun, the `softdrv-matrix.cjs` test).

##### 3.2 HP2's SoftDrv vs epic — 127 changed lines, all of them the 433 face

| H file | Δ | Exact change |
|--------|--:|--------------|
| `SoftDrvPrivate.h` | 11 | `:10` `#include "..\..\Render\Inc\Render.h"` (**backslashes — cannot resolve on Linux/wasm**); FMMX union loses `DOUBLE D;` (nothing uses it — `SoftDrv.cpp:222` `SetupFastSqrt` uses its own local `union {FLOAT F; DWORD D;}`); `:118` `DECLARE_CLASS(USoftwareRenderDevice,URenderDevice,CLASS_Config,SoftDrv)`; `:156-158` `void DrawComplexSurface( …, DWORD PolyFlags, BYTE cAlpha ); INT MaxVertices(); void DrawTriangles( FSceneNode*, FTextureInfo&, FTransTexture** Pts, INT NumPts, _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* Span );` — **`DrawGouraudPolygon` is no longer declared** (HP2's `URenderDevice::DrawGouraudPolygon` at `Engine/Inc/UnRenDev.h:78` is non-pure and presumably fans out to `DrawTriangles`) |
| `DrawPoly.cpp` | 87 | `:3679-3683` the MMX15 edge setup's `DFB/DFG` swap fixed (`MMX14Load(EdgeSetupPtr->DeltaFogRGB,DFR,DFG,DFB)` now gets the right components); `:4366-4600` `DrawGouraudPolygon` → `MaxVertices() { return 256; }` + `DrawTriangles(...)` = the old body plus a `PF_Highlighted|PF_Translucent` masked preference and three draw loops (indexed tri-list / non-indexed tri-list when `NumIndices != NumPts` / tri-fan otherwise) replacing epic's hard-coded 3..7-vertex fan |
| `DrawSurf.cpp` | 25 | `:361` `double Scale`; `:20373` signature with `DWORD PolyFlags, BYTE cAlpha`; five `Surface.PolyFlags` → `PolyFlags` (`:20384-20386,20402,20864,21484`); `0.67f`; **`cAlpha` is accepted and ignored** (Render passes `(char)iAlpha` only for Lumos surfaces in hardware mode — for a span-based device Render already folds Lumos into `PF_Translucent`; the Lumos block is `UnRender.cpp:3453-3547`, the two `DrawComplexSurface` calls are `:3541` (`LumosPolyFlags, (char)iAlpha`) and `:3550` (`Surface.PolyFlags, 255`). Note `iAlpha` stays 0 or 255 on the span-based branch, so SoftDrv may receive `cAlpha==0` and must not treat it as 'invisible') |
| `Line2D.cpp` | 2 | `0.56f` |
| `SoftDrv.cpp`, `DrawTile.cpp`, `Hit.cpp`, `Line.h`, `Line1.h` | 0 | identical to epic |

So the two deltas are almost disjoint: the template's touches kernels and tables, HP2's touches the device
face. The only overlapping region is `DrawPoly.cpp` `DrawGouraudPolygon`/`DrawTriangles` (T adds `GMeshFog =
…` at its top and the `&USoftwareRenderDevice::` member pointers; H renames it and adds the loops).

##### 3.3 File-by-file plan (base = **template's** file, then re-apply HP2's 433 lines)

| File | Take | Then apply from HP2 | Notes |
|------|------|---------------------|-------|
| `CMakeLists.txt` | HP2's existing one (already mirrors T: `ASM=0`, `SOFTDRV_MAX_SCREEN`, `../Render/Inc` + `../Render/Src`) | — | fine as is; keep `SOFTDRV_MAX_SCREEN=8192` in the wasm preset like T (`cacheVariables`) |
| `WEB-NOTES.md` | copy T's | s/`Source/SoftDrv`/same; s/`NSDLViewport.cpp` refs unchanged | the 2.13.2 lessons apply verbatim (same SDL/Emscripten path) |
| `Src/SoftDrvPrivate.h` | **T** | (1) `#include "Render.h"` instead of `"RenderPrivate.h"` (path resolves via `../Render/Inc`); (2) `DECLARE_CLASS(USoftwareRenderDevice,URenderDevice,CLASS_Config,SoftDrv)`; (3) replace the two lines `void DrawComplexSurface(Frame,Surface,Facet); void DrawGouraudPolygon(…);` with HP2's three (`DrawComplexSurface(…,DWORD PolyFlags,BYTE cAlpha)`, `INT MaxVertices()`, `DrawTriangles(…)`); (4) optionally drop `DOUBLE D;` from `FMMX` (HP2 did; nothing uses it) | keep T's `GIsMMX false` force, `SOFTDRV_MAX_SCREEN`, `_rotl` shim, `IN/OUT` undef, `BilinearFiltering` |
| `Src/SoftDrv.cpp` | **T** (HP2's = epic) | nothing | PLAN §3 "OURS-UNCHANGED (take theirs)" — correct |
| `Src/DrawTile.cpp` | **T** (HP2's = epic) | nothing | idem |
| `Src/Hit.cpp`, `Line.h`, `Line1.h` | any (identical) | — | |
| `Src/Line2D.cpp` | **T** | `0.56f` (cosmetic) | T's `PLATFORM_MSVC` gate is required: `Line.h` token-pasting does not preprocess on GCC |
| `Src/DrawSurf.cpp` | **T** | HP2's 25 lines: the `DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD PolyFlags, BYTE cAlpha )` signature at **T:21098** (H:20373) and the six `Surface.PolyFlags`→`PolyFlags` substitutions **inside that function only** — T:21109-21111 (`IsModulated/IsTranslucent/IsMasked`), T:21127 (`PF_FlatShaded`), T:21627 (`PF_NoSmooth`), T:22254 (`PF_Selected`); these are the only `Surface.PolyFlags` reads in T's file (`grep -n 'Surface\.PolyFlags'`). ⚠ T's post-pass fog swap reads `Surface.FogMap && Surface.LightMap` — leave those as `Surface.` (they are not flags). `double Scale` and `0.67f` cosmetic | verify with `diff -w E/DrawSurf.cpp H/DrawSurf.cpp` (the 25-line list) after the merge: every HP2 hunk must still be present |
| `Src/DrawPoly.cpp` | **T** | (1) rename T's `DrawGouraudPolygon` (T:4486) to `DrawTriangles( Frame, Texture, Pts, NumPts, _WORD* Indices, INT NumIndices, PolyFlags, SpanBuffer )` keeping T's `GMeshFog = VolumetricLighting && VolumetricLightingMeshes && SupportsFogMaps;` (T:4494) as the first statement; (2) add `INT USoftwareRenderDevice::MaxVertices() { return 256; }`; (3) add HP2's `PF_Highlighted|PF_Translucent` block; (4) replace T's `Point0/1/2 = Pts[0..2]; (this->*TriangleFunc)(…)` + the `NumPts>3/4/5/==7` fan with HP2's three loops (`DrawPoly.cpp:4546-4593` in H); (5) apply HP2's `DFG/DFB` fix at the MMX15 edge setup (T:3679-3683, inside the `#if ASM` region — harmless but correct) | T's `DR/DB/DG` Pentium fix (T:4202-4215) is a *different* site and stays |

After the merge the ASM inventory should read: `DrawPoly.cpp` 3 `#if ASM` (the three MMX bodies) + the
pre-existing `#if !ASM` kernels; `DrawSurf.cpp` unchanged gate count (3/6); `DrawTile.cpp` 19/15;
`SoftDrvPrivate.h` 1 (`FastSqrt`). `grep -c __asm` stays 32/104/19/1 — all inside `#if ASM` or the
MSVC-only stubs, none reachable at `ASM=0` (this is how T builds today).

Where `GIsMMX` comes from: HP2 `Core/Inc/UnGnuG.h:325` `extern CORE_API UBOOL GIsMMX;` (T:347 same) — the
`#define GIsMMX false` in `SoftDrvPrivate.h` shadows it for SoftDrv only; `ColorMode` then selects
`Pentium15/16/32` (`SoftDrvPrivate.h:96-104`).

##### 3.4 Verification for SoftDrv

- Native: `./Game -SOFTWARERENDERER -log` must reach the menu (Phase B acceptance). Then a map, at **several
  resolutions** (WEB-NOTES §6) — the `SizeY`-indexed tables are the classic overrun.
- Compare against XOpenGL screenshots for the fog/lightmap brightness; the template calibrated the Pentium
  fog path against the original MMX kernels (`tools/asmport/oracle` — not vendored here).
- `-fpack-struct=4` (wasm/MinGW): `FMMX` is a union of 8-byte members; unaffected.

---

#### 4. Editor

##### 4.1 The template's stripped Editor (facts)

`T/Editor/CMakeLists.txt`: 27 files — `CoolBsp UnBsp UnEdAct UnEdCam UnEdClick UnEdCnst UnEdCsg UnEdFact
UnEdRend UnEdSrv UnEdTran UnEditor UnMeshEd UnScrCom UnShadow UnSyntax UnTopics UnVisi Editor
UBatchExportCommandlet UBrushBuilder UConformCommandlet UMakeCommandlet UMergeDXTCommandlet UnEdExp UnMeshLP
UnParams` + `NullEdHook.cpp` (when `NOT TARGET_IS_WINDOWS OR NOT BUILD_WINDRV`; `Win32EdHook.cpp` otherwise;
`LOW_MEMORY` → only `UnEdTran.cpp Editor.cpp`). Links `Engine Core` only.

Excluded from the epic set: **`EdHook.cpp`** (`<windows.h>`, `Window.h`, `WProperties`, `NE_EdInit`
export), **`TTFontImport.cpp`** (`<windows.h>`, `CreateFont`/GDI rasterisation), **`UMasterCommandlet.cpp`**
(`#include "../../Setup/Inc/Setup.h"` — the Setup package is not in the template tree at all).

`NullEdHook.cpp` (T, 99 lines; **HP2's copy is byte-identical**): defines the six hooks the rest of the
Editor calls into the GUI — `NotifyDestroy/NotifyPreChange/NotifyPostChange/NotifyExec`,
`UpdatePropertiesWindows() {}`, `HookExec() { return 0; }`, `EdCallback() {}` — plus the `GTopic/GItem/GValue`
thread-exchange globals and `UEngine* Engine;`. `NotifyExec` casts `Src` to `WProperties*` for `UseDest`
without including `Window.h` (`UseDest` is a `WProperties*` forward-declared in `Editor.h`).

Compile fixes the template carries (all needed by HP2 too):

| Fix | T site | Reason |
|-----|--------|--------|
| `PUSH_HIT` `sizeof(expr)` → `sizeof(decltype(expr))` | **`T/Engine/Inc/UnCamera.h:310`** (not in Editor!) | `HBrowserTexture(Texture)` etc. are temporaries; GCC rejects `sizeof` on a braced/temporary expression in this macro form. HP2 `Engine/Inc/UnCamera.h:323` has the old form; Editor users: `UnEdCam.cpp:1863,1942,2017,2022,2086,2156,2233,2456`, `UnMatinee.cpp:174` |
| `Min<INT>(1024, Input+InputPos-Start+1)` | `UnScrCom.cpp:1351` | `INT` vs `ptrdiff_t` | 
| `UnParams.cpp` `#ifndef UNREAL_STATIC … #endif` around `GetFVECTOR` (2 overloads), `GetSUBSTRING`, `GetFROTATOR` (2) | `UnParams.cpp:22-171` | Engine's `UnParams.cpp` defines the same five with `ENGINE_API`; a whole-archive static link double-defines them |
| `TFieldIterator<UProperty> It(Function);` hoisted; `for( ; It && …` | `UnScrCom.cpp:1618-1661` | `It` is used by a second loop after the first |
| `UClass* Test;` hoisted; `FLabelRecord* LabelRecord;`; `INT i;`/`iNest` | `UnScrCom.cpp:1741,2918,4544,4775,4846` | for-scope |
| `FPolyVertex( INT i, INT j )` (no `FPolyVertex::` qualifier inside the class) | `UnEdCam.cpp:547` | extra qualification is an error on GCC |
| `&FEditorVisibility::AddPortal` / `BlockPortal` / `TagZonePortalFragment` | `UnVisi.cpp:599,634,1230` | pointer-to-member needs `&Class::` |
| `for( TObjectIterator<UClass> It; …` (second loop declares its own `It`) | `UMakeCommandlet.cpp:98` | `It` was for-scoped |
| `#endif` (drop the trailing `NAMES_ONLY`) | `EditorClasses.h:78` | "extra tokens at end of #endif" (warning; error with `-Werror`) |
| `UEditorEngine::UEditorEngine() {}` (no `EditPackages( E_NoInit )`) + a guard that resets `EditPackages` to `Core Engine Editor Fire` if `Num()` is absurd + a `printf` per package | `UnEditor.cpp:44-115` | ut99dc ("maximqad: bruh fix") — `E_NoInit` left the config array garbage under GCC value-init. HP2's `UnTemplate.h:387` has `TArray( ENoInit )`; **apply the ctor change, skip the 4-package fallback** (HP2's `Default.ini:327-337` lists 11 `EditPackages`) |
| `Ar.Logf( …, Temp, ItE->Names.Num() )` instead of `i` | `UnEdExp.cpp:195` | `i` out of scope; HP2 `UnEdExp.cpp:211` same bug |
| `INT i;` hoists | `UnBsp.cpp`(7), `UnEdAct.cpp`(7), `UnEdCam.cpp`(3), `UnEdCsg.cpp`(6, incl. `FStaticBrushIterator It(Level);`), `UnEdFact.cpp`(4), `UnEdSrv.cpp`(2), `UnEdTran.cpp`(1), `UnShadow.cpp`(1), `UnVisi.cpp`(6), `UConformCommandlet.cpp`(2), `UMeshLP.cpp`(24) | for-scope |

##### 4.2 HP2's Editor inventory vs epic and the template

| H file | Δ vs E | Status for the port |
|--------|-------:|---------------------|
| `Src/EdHook.cpp` | 257 (438 lines) | **Windows-only** (`<windows.h>`, `<shlobj.h>`, `Window.h`, `UnEngineWin.h`, `HWND`, `SendMessageX`) → excluded, replaced by `NullEdHook.cpp` ✔ (already in HP2's list) |
| `Src/TTFontImport.cpp` | 71 | **Windows-only** (`<windows.h>`, `CreateFont`) → excluded ✔ |
| `Src/UMasterCommandlet.cpp` | 186 | **Windows-only by include**: `:10 #include "FFileManagerArc.h"` (exists, `Core/Inc`), `:11 #include "../../Setup/Inc/Setup.h"` (the Win32 Setup package, not built) → **must be removed from HP2's CMake list** ✘ (currently listed) |
| `Src/UPSX2MusicCommandlet.cpp` | HP2-only, **0 bytes** | not in `Editor.dsp` either; delete or ignore |
| `Src/UPS2ConvertCommandlet.cpp` | HP2-only, 70 lines | portable (`Sound->PS2Convert()`), but PS2-only purpose; HP2's list excludes it ✔ (owner: no PSX2 deltas) |
| `Src/UPkgCommandlet.cpp` | HP2-only, 254 lines | texture/sound import-export commandlet; `EditorPrivate.h` only, portable ✔ listed |
| `Src/UnMatinee.cpp` + `Inc/UnMatinee.h` | HP2-only | matinee path drawing (`PUSH_HIT`, `UnRender.h`), portable ✔ listed |
| `Inc/UnSkeletalImport.h` | HP2-only, 105 lines | plain structs for `.psk/.psa` import (used by `UnMeshEd.cpp`, `UnMeshLP.cpp`) ✔ |
| `Src/UnEdSrv.cpp` | 1515 (4015 lines) | 433 editor server (brush clipping, `Exec_Audio`, skeletal import commands); includes `../../Engine/Src/UnPath.h` (relative, resolves) and `UnSkeletalMesh.h`; `hWndParent` is a `DWORD` parsed from the command line (`:3347`) — portable |
| `Src/UnEdCam.cpp` | 1135 (2893 lines) | vertex editing, bezier paths, box select; **`wcscmp(tName,TEXT(""))` at `:2313,2345,2377`** — `TCHAR` is `char` on this port (`UnGnuG.h:184 TCHAR_TO_ANSI(str) str`), so `wcscmp(char*, char*)` does not compile → `appStrcmp`; `#include <wchar.h>` then unnecessary |
| `Src/UnMeshEd.cpp`, `UnMeshLP.cpp` | 1063 / 930 | skeletal mesh import + LOD; no Windows-isms; ~65 for-scope reuses between them (§4.5) |
| `Src/UnScrCom.cpp` | 518 (6044 lines) | 433 script compiler (`EX_DebugInfo` etc.); same fix sites as T at `:1425` (`Min(1024,…)`), `:1697/1732/1770` (`It` reused by **two** later loops), `:1879` (`Test` used after the loop by `check(Test!=NULL)`), label/`iNest` loops |
| `Src/UnEdFact.cpp` | 26 | `UMusicFactory` **already** `#if 0`'d (`:926-973`, "TG ALPHA") and its class in `EditorPrivate.h:165-176` likewise — consistent |
| `Src/EditorPrivate.h` | 75 | 4-arg `DECLARE_CLASS(...,Editor)` everywhere, `EJSMeshTriType`, `MSB_CSGOper`, `UTrueTypeFontFactory` gains `RenderNative/Wildcard/Path` (class stays declared even though `TTFontImport.cpp` is not built — same as T; the `.u` has no such class, `Editor.u` exports only `BrushBuilder`, `EditorEngine` and the builders, so nothing looks it up) |
| `Inc/Editor.h` | 75 | `UEditorEngine` 433 API (`AddActor` returns `AActor*`, `csgRebuild(Level,bVisibleOnly)`, skeletal/animation import virtuals, `edactBoxSelect`, matinee draws); `#define USE_SUB_DIRECTORIES` |
| `Inc/EditorClasses.h` | 4 | `UBrushBuilder` gains `BitmapFilename`, `ToolTip` (must match `Assets/System/Editor.u` — never regenerate); `:80 #endif NAMES_ONLY` |
| `Src/UnEdTran.cpp` | 2 | `DECLARE_CLASS(UTransBuffer,UObject,CLASS_Transient,Editor)` at `:377`, `IMPLEMENT_CLASS(UTransBuffer)` at `:598` — **the class the content needs** |
| `Src/UnParams.cpp` | 12 | `EDITOR_API` on the five helpers; **no `UNREAL_STATIC` guard yet**; `Engine/Src/UnParams.cpp:25-157` defines the same five → duplicate symbols in the static link |
| `Src/Editor.cpp` | 5 | `GBoxSelStart/End`, `GbIsBoxSel` globals; standard `IMPLEMENT_PACKAGE(Editor)` |
| `Classes/` | — | `Editor.upkg` + 12 `.uc` (BrushBuilder, EditorEngine, ten builders incl. `VolumetricBuilder`) — matches the `Editor.u` export list |
| everything else (`CoolBsp UnEdCnst UnSyntax UnTopics UnTopics.h UBatchExport UMergeDXT UBrushBuilder UnEdClick UnEdRend UnEditor UnShadow UnVisi UnBsp UnEdAct UnEdCsg UnEdExp UMake UConform`) | 0–355 | 433 feature deltas, no platform code; the template's for-scope hoists apply by analogy |

##### 4.3 Content analysis — which Editor classes HP2 content needs at runtime

Method: a small parser of the UE1 package header (name table, import table, export table; script
`scratchpad/pkgnames.py`, `pkgexports.py`) run over `Assets/System/*.u` (11 packages, all file version 79)
and `Assets/Maps/*.unr` (51 maps; 76–79). `strings` is not usable here: v64+ name tables are
length-prefixed and the object graph lives in the import table.

| Package / map | Imports from `Editor` | Notes |
|---------------|-----------------------|-------|
| `System/Editor.u` | **`Class Editor.TransBuffer`** (import, class-package `Core`) | `Editor.u` exports 377 objects: classes `BrushBuilder` (flags `0x40f0004` = `RF_Native` set — `UnObjBas.h:157 RF_Native = 0x04000000`), `EditorEngine` (native), ten `*Builder` script classes (`0xf0004`, not native), 13 textures (`B_LiteOn B_PlyrOn B_CollOf HandleLeft HandleRight B_MenuUp B_MenuDn B_CollOn BkgndHi B_PlyrOf Bkgnd Bad B_LiteOf`), palettes, text buffers, properties. `TransBuffer` is **not** an export — it is resolved through the native package, exactly the UT99 `Can't find Class Editor.TransBuffer in file '../System/Editor.u'` failure |
| `Maps/Entry.unr` (v76) | **`Texture Editor.B_PlyrOn`** | the entry level `UGameEngine::Init` loads first (`UnGame.cpp:169-170`) and keeps resident (`:1129,1151,1410`) |
| `Maps/Quidditch.unr`, `Maps/Quidditch_Intro.unr` | **`Texture Editor.Bad`** | Quidditch levels |
| all other 48 maps, `Core.u Engine.u Fire.u HGame.u HPModels.u HPParticle.u HProps.u HPSounds.u UnrealShare.u UWindow.u` | none | (`HGame.u` imports from `HPEdit` and `SpellShapes`, `HPParticle.u` from `3DWizardCards` — packages **not present** in `Assets/System`; that is a `hp2-game-boot-and-content.md` matter, noted here only because the same parser found it) |

Consequences:

1. Loading `Entry.unr` → `Editor.u` → import `Editor.TransBuffer` → **native `UTransBuffer` must be registered**
   (`IMPLEMENT_CLASS(UTransBuffer)` via `STATIC_EXPORT` in the `UNREAL_STATIC` scheme, `UnEdTran.cpp:598`).
   The two `RF_Native` script classes `EditorEngine` and `BrushBuilder` also need their C++ halves
   (`UnEditor.cpp`, `UBrushBuilder.cpp`) or the linker warns/fails on those exports.
2. **Therefore the Editor package must be linked into the game binary** (`H/Source/CMakeLists.txt:35`
   `BUILD_EDITOR ON`, `:377-379 add_subdirectory(Editor); list(APPEND INSTALL_TARGETS Editor)`;
   `Game/CMakeLists.txt:31-34` whole-archives `INSTALL_TARGETS` in the static build — same mechanism as T).
   This is the template's ufront 2.39 finding reproduced on HP2 content, with the difference that in HP2
   the dependency is hit **at boot** (Entry), not at the first bot-map load.
3. Nothing in the HP2 content references the commandlets, factories or exporters by name; those are
   compiled only because the files carry other things and because `ucc make` needs them.
4. The stripped Editor stays stripped: no `EdHook` GUI, no TTF import, no Master/Setup.

Verification command (from `Assets/`): `python3 <scratchpad>/pkgnames.py System/*.u Maps/*.unr | grep -v
"editor-imports=\[\]"` — prints exactly the four lines above.

##### 4.4 CMake for HP2 Editor — source list and exclusions

Recommended `H/Editor/CMakeLists.txt` `SRC_FILES` (HP2's current list **minus `UMasterCommandlet.cpp`**):

```
Src/CoolBsp.cpp  Src/Editor.cpp  Src/UBatchExportCommandlet.cpp  Src/UBrushBuilder.cpp
Src/UConformCommandlet.cpp  Src/UMakeCommandlet.cpp  Src/UMergeDXTCommandlet.cpp  Src/UPkgCommandlet.cpp
Src/UnBsp.cpp  Src/UnEdAct.cpp  Src/UnEdCam.cpp  Src/UnEdClick.cpp  Src/UnEdCnst.cpp  Src/UnEdCsg.cpp
Src/UnEdExp.cpp  Src/UnEdFact.cpp  Src/UnEditor.cpp  Src/UnEdRend.cpp  Src/UnEdSrv.cpp  Src/UnEdTran.cpp
Src/UnMatinee.cpp  Src/UnMeshEd.cpp  Src/UnMeshLP.cpp  Src/UnParams.cpp  Src/UnScrCom.cpp  Src/UnShadow.cpp
Src/UnSyntax.cpp  Src/UnTopics.cpp  Src/UnVisi.cpp  Src/NullEdHook.cpp
```

Excluded (Windows-only or unwanted), with the line that proves it:

| File | Evidence |
|------|----------|
| `Src/EdHook.cpp` | `:13 #include <windows.h>`, `:16 #include "Window.h"`, `:68 #include "UnEngineWin.h"`, `:420 __declspec(dllexport) void __stdcall NE_EdInit( HWND, HWND )` |
| `Src/TTFontImport.cpp` | `:13 #include <windows.h>`, `:179-180 CreateFont(A)(…)` |
| `Src/UMasterCommandlet.cpp` | `:11 #include "../../Setup/Inc/Setup.h"` (Setup = Win32 installer package, listed as "not built" in `docs/SOURCE-LAYOUT.md`); T excludes it for the same reason |
| `Src/UPS2ConvertCommandlet.cpp` | PS2 sound conversion — owner excludes PSX2 deltas (portable, could be added back) |
| `Src/UPSX2MusicCommandlet.cpp` | 0-byte file, not in `Editor.dsp` |

`target_link_libraries(Editor Engine Core)` and `EDITOR_EXPORTS UPACKAGE_NAME=Editor` as in HP2's file —
correct. The `TARGET_IS_WINDOWS AND BUILD_WINDRV` branch of T (Win32 GUI editor via `Window`) is not
applicable: HP2 keeps `Window`/`UnrealEd` unbuilt.

##### 4.5 Compile fixes to port into HP2's Editor (with HP2 line numbers)

| Fix | HP2 site(s) | Action |
|-----|-------------|--------|
| `PUSH_HIT` `sizeof(decltype(expr))` | `Engine/Inc/UnCamera.h:323` | change the macro (Engine report should own it; it unblocks `UnEdCam.cpp` ×8, `UnMatinee.cpp:174`) |
| `Min<INT>` | `UnScrCom.cpp:1425` | `Min<INT>(1024,Input+InputPos-Start+1)` |
| `UnParams.cpp` duplicate helpers | `UnParams.cpp:22-160` | wrap `GetFVECTOR`×2, `GetSUBSTRING`, `GetFROTATOR`×2 in `#ifndef UNREAL_STATIC … #endif` (T:22-171 with its comment); keep `GetBEGIN/GetEND/SetFVECTOR/GetFSCALE` |
| `TFieldIterator It` reuse | `UnScrCom.cpp:1697` (decl in `for`), `1732`, `1770` (reused) | hoist `TFieldIterator<UProperty> It(Function);` above 1697; `for( ; It && …` at all three |
| `UClass* Test` used after loop | `UnScrCom.cpp:1879-1881` | `UClass* Test; for( Test=Source.PropertyClass; …` |
| label / nest loops | `UnScrCom.cpp` (`FLabelRecord* LabelRecord`, `INT i`, `INT iNest` — grep `for( FLabelRecord\* LabelRecord`, `for( INT i=NestLevel-1`, `for( INT iNest=NestLevel-1`) | hoist as T did |
| `FPolyVertex::FPolyVertex` | `UnEdCam.cpp:622` | drop the qualifier |
| `wcscmp` on ANSI TCHAR | `UnEdCam.cpp:2313,2345,2377` (+ commented 2410) | `appStrcmp(tName,TEXT(""))`; remove `#include <wchar.h>` (`:14`) |
| member-pointer args | `UnVisi.cpp:599,634,1236` | `&FEditorVisibility::AddPortal` / `BlockPortal` / `TagZonePortalFragment`; also `FPortal* ClipPortal;` hoist if the `:962` loop variable is reused |
| `for( It=TObjectIterator<UClass>()` | `UMakeCommandlet.cpp:204` | `for( TObjectIterator<UClass> It; It; ++It )` |
| `#endif NAMES_ONLY` | `Editor/Inc/EditorClasses.h:80` (and `Engine/Inc/EngineClasses.h:3951`) | `#endif` |
| `EditPackages( E_NoInit )` | `UnEditor.cpp:46` | remove the initialiser (T:44); do **not** copy T's 4-package fallback |
| `UnEdExp.cpp` `i` out of scope | `UnEdExp.cpp:211` | `ItE->Names.Num()` |
| `UConformCommandlet.cpp` | `:191,:213` (`i` from the `for( INT i` at `:182`) | `INT i;` before |
| MSVC6 for-scope reuses (heuristic count) | `UnMeshLP.cpp` 57 · `UnEdSrv.cpp` 13 · `UnBsp.cpp` 11 · `UnEdCam.cpp` 9 · `UnEdAct.cpp` 8 · `UnMeshEd.cpp` 8 · `UnEdCsg.cpp` 5 · `UnEdFact.cpp` 3 · `UnEdTran.cpp` 2 · `UnEdExp.cpp` 1 · `UMakeCommandlet.cpp` 1 · `UnMatinee.cpp` 1 (`iActor`) — line lists in `scratchpad/forscope.py` output | hoist as the compiler reports them (about 120 sites; T fixed ~70 of the same in its 433-less files) |
| `#pragma once` | `Inc/UnMatinee.h:13` | fine on GCC |

Not needed: the `Window` link, `Win32EdHook.cpp`, `LOW_MEMORY` variant.

---

#### 5. Dependencies on the other analyses (things this subject cannot compile without)

| Needed by | Symbol / header | Where it must come from | Owner report |
|-----------|-----------------|-------------------------|--------------|
| Render, Fire, SoftDrv, Editor | `ASM=0`, `ASMLINUX=0`, `ASM3DNOW=0`, `GCC_ALIGN`, `__cdecl` empty, no `__PSX2_EE__` | template `Core/Inc/UnGnuG.h` (T:82-83, 104-107, 206-212) | `template-core.md` |
| Render `UnRender.cpp` | `appProcessSeconds()` → `FTime` (`.GetFloat()`) | `Core/Inc/UnFile.h:263` declared; only `UnVcWin32.cpp:501` defines it today → add to the Unix core | `template-core.md` / `hp2-core-engine-specifics.md` |
| Render | `FStats GStat` + `Clock()` RAII (`Core/Inc/UnStat.h:38,157`), `STATS 1` (`UnBuild.h:56`) | HP2 Core (present) | — |
| Render | `GAutoFovFrameBase` (if AutoFOV is ported) | Engine `UnCamera.h/.cpp` from T | `template-engine.md` |
| Render `DrawWorld` | `PF_Lumos*` flags, `APlayerPawn::bLumosOn/fLumosRadius`, `ASkyZoneInfo` | HP2 Engine (present) | — |
| Editor | `PUSH_HIT` with `decltype` | Engine `UnCamera.h:323` | `template-engine.md` |
| Editor (static link) | `UNREAL_STATIC` defined for `BUILD_STATIC` | top-level CMake (HP2 `:63,87`) | `template-core.md` |
| SoftDrv | `PLATFORM_X86` (`H/CMakeLists.txt:156`) / `PLATFORM_WIN32` for the `_rotl` and `IN/OUT` shims | top-level CMake | — |
| Editor content | `Editor.u`, `Entry.unr` in the boot working set / `precache.json` | frontend target | `ufrontend-target-contract.md` |

---

#### 6. Open questions / things to verify at build time

1. **AutoFOV in `DrawWorld` (R15):** does any HP2 script scale the first-person view by `FOVAngle` the way
   UT99's `Inventory.uc:239` does? `grep -rn FOVAngle Source/HGame/Classes Source/UnrealShare/Classes`.
   If not, port only the null-guard and skip the base-FOV swap.
2. **`GCC_ALIGN(4)` on `SparkType`/`DropType`:** confirm at first boot with the layout guard /
   `VERIFY_CLASS_OFFSET` messages rather than by reasoning about tail padding; wasm's `-fpack-struct=4` and
   MinGW's MSVC-style bitfields change the arithmetic (T's BUILD.md, "Two ABI differences").
3. **`URenderDevice::DrawGouraudPolygon` (non-pure, `UnRenDev.h:78`):** check its Engine implementation
   forwards to `DrawTriangles` (or is unused by Render — Render calls only `DrawComplexSurface`,
   `DrawTriangles`, `DrawTile`, `DrawStats`); if it forwards, the merged SoftDrv needs no
   `DrawGouraudPolygon` at all (HP2's header omits it, matching the forks' 433 gates).
4. **`cAlpha` on SoftDrv:** Render computes a graded `iAlpha` only in hardware mode (`UnRender.cpp:3453-3547`); on the
   span-based branch Lumos becomes `PF_Translucent` and `iAlpha` is 0 or 255. So SoftDrv may ignore `cAlpha` — but confirm
   nothing else (HUD, `UCanvas`) calls `DrawComplexSurface` with an alpha below 255 on the software path.
5. **Missing packages in the content graph** (`HPEdit`, `SpellShapes`, `3DWizardCards` imported by
   `HGame.u`/`HPParticle.u`, `Skurge`, `Dungeon`, … by maps are `Textures/*.utx`): outside this report's
   subject; flagged for `hp2-game-boot-and-content.md`.
6. `Render/Src/UnTest.cpp` could alternatively be kept by `#define __stdcall` in `UnGnuG.h`; the template
   chose to drop the file and keep `__stdcall` meaningful for MinGW (`UnGnuG.h:99-104`). Drop it.
7. The `UnParticleRn.cpp` particle renderer was never built with GCC; expect the same class of for-scope
   / template-deduction errors as the rest of Render (the heuristic found none, but it only sees `for`).

## Status: complete

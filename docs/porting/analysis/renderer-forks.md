# renderer-forks — XOpenGLDrv, UTGLR (OpenGLDrv), gl4es, NSDLDrv vs the HP2 build-433 render-device face

Analyst key: `renderer-forks`. Re-run 2026-09-26 (supersedes the unfinished 2026-09-25 draft; every claim below was re-verified against the trees on disk).

Sections (filled incrementally; a section that still reads "pending" was not finished):

- §0 TL;DR
- §1 Workspace facts (where the forks really are, branch state, CMake wiring)
- §2 The 433 render-device face (engine side): URenderDevice, FSceneNode, FSurfaceInfo/Facet, FTextureInfo, ETextureFormat, EPolyFlags, UViewport/UClient, FTime, DECLARE_CLASS
- §3 Who calls what in HP2's Render/Engine (DrawTriangles, DrawGouraudPolygon, DrawComplexSurface cAlpha, particles, skeletal meshes, decals, MaxVertices, GetOsSurface)
- §4 XOpenGLDrv fork: version gates inventory
- §5 XOpenGLDrv fork: engine API surface touched + exact 433 change list (file by file) + gating scheme
- §6 UTGLR fork: gates inventory, engine API surface, exact 433 change list + gating scheme
- §7 gl4es: is any change needed
- §8 NSDLDrv: GL/GLES/software selection, web context creation, UViewport 433 delta
- §9 In-tree devices touched by the same face (NullDrv, SoftDrv) — brief
- §10 Checklists (ordered work plan)
- §11 Open questions / how to verify

## 0. TL;DR

pending

## 1. Workspace facts

Trees read for this report:

| Tree | Path actually on disk | State |
|------|------|------|
| HP2 engine repo | `/home/ion/hp2-ufrontend/HarryPotter2` | `Source/Engine/Inc/*.h` (433, LF), `Source/Render/{Inc,Src}`, `Source/D3DDrv/Src/Direct3D7.cpp` (the shipped retail HW driver, reference only), `Source/SoftDrv`, the template-copied glue `Source/{XOpenGLDrv,OpenGLDrv,NSDLDrv,NullDrv}` |
| XOpenGLDrv fork | **`/home/ion/hp2-ufrontend/ufrontend/XOpenGLDrv`** | branch `hp2-v433-compat` @ `87f6e89` (2026-08-09) == `ut99-v400-compat` == `origin/ut99-v400-compat`; 4 ufront commits on top of OldUnreal `03f7ca2` |
| UTGLR fork | **`/home/ion/hp2-ufrontend/ufrontend/utglr-copy`** | branch `hp2` @ `e60807b` == `main` == `origin/main` (Dohnal drop `ac9e37b` + 3 ufront commits) |
| gl4es fork | **`/home/ion/hp2-ufrontend/ufrontend/gl4es`** | branch `hp2` @ `98441ed5` == `fix/cubemap-bind-active-unit` == `origin/fix/cubemap-bind-active-unit` |
| Template (read only) | `/home/ion/engines/engine-ut99` | `Source/XOpenGLDrv/{ENGINE_CHANGES,WEB_INTEGRATION}.md`, `Source/OpenGLDrv/{PORTING,WEB-INTEGRATION}.md`, `Source/NSDLDrv/Src/*` |
| Epic v400 (read only) | `/home/ion/epic-sources/ut99-v400` | `Engine/Inc/*.h`, `Render/Src/*`, `Engine/Src/UnCamera.cpp` |

Diff convention: `diff --strip-trailing-cr -w -u <epic v400> <HP2>`; line numbers are the HP2 (LF) file's unless marked otherwise.

| # | Fact | Evidence | ACTION |
|---|------|----------|--------|
| 1.1 | **The three fork clones are not where the build looks for them.** They sit *inside* the ufrontend clone as untracked dirs; `/home/ion/hp2-ufrontend/` holds only `HarryPotter2` and `ufrontend`. | `ls /home/ion/hp2-ufrontend` → `HarryPotter2 ufrontend`; `git -C /home/ion/hp2-ufrontend/ufrontend status --short` → `?? XOpenGLDrv/ ?? gl4es/ ?? utglr-copy/`. `Source/CMakePresets.json:20-22,37-39,76-78` set `FETCHCONTENT_SOURCE_DIR_XOPENGLDRV=${sourceDir}/../../XOpenGLDrv` etc. (= `/home/ion/hp2-ufrontend/XOpenGLDrv`). `HarryPotter2/CLAUDE.md` "Map of the world" also says `/home/ion/hp2-ufrontend/XOpenGLDrv`. | Move them: `mv /home/ion/hp2-ufrontend/ufrontend/{XOpenGLDrv,utglr-copy,gl4es} /home/ion/hp2-ufrontend/` (a plain `mv` of the working trees; their `.git` dirs travel with them). Do this before the first configure, otherwise FetchContent with an empty/missing `SOURCE_DIR` fails at configure time. Leaving them inside `ufrontend/` also risks a `git add -A` in the frontend clone committing three foreign repos (they are not in `ufrontend/.gitignore`). |
| 1.2 | `hp2-v433-compat` has **no HP2 commits yet** — it is byte-identical to `ut99-v400-compat`. Same for utglr `hp2` (== `main`) and gl4es `hp2` (== `fix/cubemap-bind-active-unit`). | `git -C …/XOpenGLDrv branch -vv` (both at `87f6e89`); `git -C …/utglr-copy branch -vv` (both `e60807b`); `git -C …/gl4es branch -vv` (both `98441ed5`). | All 433 work in §5/§6 is still to be written. Branches are local-only (`hp2-v433-compat`, utglr `hp2`, gl4es `hp2` have no upstream tracking) — push them before any preset without `FETCHCONTENT_SOURCE_DIR_*` is used, since `Source/CMakeLists.txt:254,266` fetch `hp2-v433-compat` / `hp2` from `github.com/10-p/*`. |
| 1.3 | `GL4ES_GIT_TAG` defaults to `fix/cubemap-bind-active-unit`, not `hp2`. | `Source/CMakeLists.txt:313` | Harmless while presets pass `GL4ES_SOURCE_DIR`. Because gl4es needs no HP2 change (§7) there is no reason to have an `hp2` branch at all: either delete the local `hp2` branch and keep the template's pin, or push `hp2` and change line 313 to `hp2`. Pick one so `meta.json`'s `toolchain.gl4es` (written by `cmake/ArtifactDeps.cmake`) names a ref that exists on the remote. |
| 1.4 | `Source/XOpenGLDrv/{CMakeLists.txt,ENGINE_CHANGES.md,WEB_INTEGRATION.md,XOpenGLDrv.int}`, `Source/NSDLDrv/*`, `Source/NullDrv/*` are **byte-identical** to the template's. | `diff -rq /home/ion/engines/engine-ut99/Source/<d> Source/<d>` → no output | They still describe/compile against v400. The CMake glue itself is fine for 433 (see 1.6); the docs must be updated when the 433 work lands (§5.5). NullDrv needs the face change (§9). |
| 1.5 | `Source/OpenGLDrv/CMakeLists.txt` differs from the template in exactly one line: it adds `UTGLR_ENGINE_HP2` to the compile definitions (line 43). | `diff` → `42a43 >   UTGLR_ENGINE_HP2` | This is the macro the utglr `hp2` branch must key its 433 code on (§6.4). Nothing in the fork reacts to it yet (`grep -rn UTGLR_ENGINE_HP2 utglr-copy` → nothing). |
| 1.6 | Both GL glue CMakeLists add `${CMAKE_CURRENT_SOURCE_DIR}/../Render/Src` as a private include dir, and link `Render`, which exports `Render/Inc` PUBLIC (`Source/Render/CMakeLists.txt:20-24`). HP2 has **no `RenderPrivate.h`** anywhere; its Render private header is `Source/Render/Inc/Render.h` (and `UnSpan.h` lives in both `Render/Inc` and `Render/Src`, identical to v400's `Render/Src/UnSpan.h`). | `find Source -name RenderPrivate.h` → nothing; `ls Source/Render/Inc` → `Amd3d.h Render.h UnRenderNative.h UnSpan.h` | The forks must `#include "Render.h"` instead of `"RenderPrivate.h"` under the 433 gate (§5.4, §6.4). No CMake change needed: `Render/Inc` already reaches them through the `Render` link. |
| 1.7 | HP2's retail hardware driver is **D3DDrv (Direct3D 7)**: `Default.ini:22 GameRenderDevice=D3DDrv.D3DRenderDevice`; `Assets/System` ships only `D3DDrv.dll`, `SoftDrv.dll`, `WinDrv.dll` (no OpenGLDrv/GlideDrv/MetalDrv dll, although `Default.ini:223-250` keeps their sections). `D3DDrv.dsp:109` compiles `Direct3D7.cpp`; `und3d.cpp` is not in the project. | grep | **`Source/D3DDrv/Src/Direct3D7.cpp` is the behavioural reference** for every 433-specific semantic (DrawTriangles, cAlpha, Lumos blending, vertex alpha). All GL-side decisions below cite it. |

## 2. The 433 render-device face (engine side)

### 2.1 `URenderDevice` (`Source/Engine/Inc/UnRenDev.h`) — the complete v400 → 433 diff

The whole diff is 3 hunks: an include guard (lines 11-12, 106), the 4-arg `DECLARE_ABSTRACT_CLASS(URenderDevice,USubsystem,CLASS_Config,Engine)` (line 38) and this block:

```cpp
// v400 Engine/Inc/UnRenDev.h:71-72
virtual void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )=0;
virtual void DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span )=0;

// HP2 Source/Engine/Inc/UnRenDev.h:75-78, 93
virtual void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD PolyFlags, BYTE cAlpha )=0;
virtual INT MaxVertices()=0;
virtual void DrawTriangles( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* Span )=0;
virtual void DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span );   // no longer pure
...
virtual void* GetOsSurface( void* Release = NULL, bool bFrontBuffer=false) { return NULL; }   // new, after PrecacheTexture
```

Everything else is identical to v400: the member variables (`DecompFormat … DetailTextures, Pad1[8], Pad0[8]`, lines 41-60), `Init/SetRes/Exit/Flush/Exec/Lock/Unlock/DrawTile/Draw3DLine/Draw2DClippedLine/Draw2DLine/Draw2DPoint/ClearZ/PushHit/PopHit/GetStats/ReadPixels/EndFlash/DrawStats/SetSceneNode/PrecacheTexture` and the 8 `vtblPad*`. **There is no `DrawGouraudTriangles`, no `DrawGouraudPolyList`, no `SupportsTextureFormat`, no `UpdateTextureRect` in 433** (those are 227/469 interfaces).

| Entry point | 433 status | What a driver must do |
|---|---|---|
| `DrawComplexSurface(Frame,Surface,Facet,PolyFlags,cAlpha)` | pure, new signature | Implement the 5-arg form. **Use the `PolyFlags` argument, not `Surface.PolyFlags`** — the Lumos path passes a modified copy (`LumosPolyFlags`, with `PF_Occlude` added/removed), see §3.3. Honour `cAlpha` (§3.3). |
| `MaxVertices()` | pure, new | Return the largest `NumPts` the device accepts in one `DrawTriangles`. Callers size their buffers from it: meshes allocate `VertBuffer(MaxVertices())` + `IndexBuffer(MaxVertices()*6)` (`Render/Src/UnMeshRn.cpp:113-122`), particles allocate `MaxVertices()` verts + `MaxVertices()*3` indices (`UnParticleRn.cpp:467-475`). The retail D3D7 driver returns `ACTORPOLY_VERTEX_MAX = 256` (`D3DDrv/Src/Direct3D7.cpp:30,1151-1154`). Must be ≤ 65535 (indices are `_WORD`; the mesh batcher uses `_WORD(-1)` as its "unassigned" sentinel, `UnMeshRn.cpp:108,148`). |
| `DrawTriangles(Frame,Info,Pts,NumPts,Indices,NumIndices,PolyFlags,Span)` | pure, new | **The only path meshes, particles and decals use** (§3.1). Two modes, exactly as D3D7 (`Direct3D7.cpp:1218-1230`): `Indices!=NULL` → indexed triangle list, `NumIndices/3` triangles, each index in `[0,NumPts)`; `Indices==NULL` → `NumIndices==NumPts` means **one convex polygon drawn as a fan**, otherwise a non-indexed triangle list of `NumPts` verts. |
| `DrawGouraudPolygon(...)` | virtual, **not pure**; base impl forwards | `Engine/Src/UnCamera.cpp:199-203`: `DrawTriangles( Frame, Info, Pts, NumPts, NULL, NumPts, PolyFlags, SpanBuffer );` — so a driver that does not override it gets fans routed into its `DrawTriangles`. Nothing in HP2 `Render`/`Engine` calls it any more (only `D3DDrv/Src/und3d.cpp:836`, a non-built file, and `NullDrv` define it). |
| `GetOsSurface(Release,bFrontBuffer)` | virtual, default `NULL` | Only caller is `WinDrv/Src/WinViewport.cpp:939,1005` (the HDC dance for GDI text / Smacker). NSDLDrv never calls it. **Do not override** in the GL forks. |

### 2.2 Structures a driver reads — v400 vs 433

| Structure | Header | 433 change | Driver impact |
|---|---|---|---|
| `FSceneNode` | `Engine/Inc/UnRender.h:48-102` | comments/float literals only (`1.0`→`1.f`) | none |
| `FSurfaceInfo`, `FSurfaceFacet`, `FSavedPoly` | `UnRender.h` | none | none |
| `FOutVector`, `FTransform`, `FTransSample`, `FTransTexture` | `UnRender.h` | none in layout; `FTransSample/FTransTexture` operators use `FPlane` arithmetic instead of per-component (`UnRender.h:286-345`) | none. **Semantics change:** `Light.W` now carries per-vertex opacity for meshes and particles (§3.4). |
| `FScreenBounds` | `UnRender.h:398-403` | `FLOAT MinZ` → `FLOAT MinZ, MaxZ` | drivers do not use it |
| `ELineFlags` | `UnRender.h:412-418` | adds `LINE_PreTransformed = 4` | base `URenderDevice::Draw3DLine` skips the `Frame->Coords` transform when set (`Engine/Src/UnCamera.cpp:142-147`). **No caller in the HP2 tree passes it** (`grep -rn LINE_PreTransformed Source` → only the enum and `UnCamera.cpp:143`). Drivers that override `Draw3DLine` (XOpenGL, UTGLR) should still honour it (skip their own `TransformPointBy`) — cheap and future-proof. |
| `URenderBase` | `UnRender.h:422-465` | `BoundVisible(…, const FCoords&, …)`, `DrawCircle(…, bScaleRadiusByZoom)`, `DrawBox(…, FBox, FCoords)`, `DrawCylinder`, inline `Draw3DLine` | drivers do not derive from it |
| `FTextureInfo` | `Engine/Inc/UnTex.h:376-408` | **none** (`Texture, CacheID, PaletteCacheID, Pan, MaxColor, Format, UScale, VScale, USize, VSize, UClamp, VClamp, NumMips, LOD, Palette, bHighColorQuality, bHighTextureQuality, bRealtime, bParametric, bRealtimeChanged, Mips[MAX_MIPS=12]`, `Load/Unload/CacheMaxColor`) | none |
| `FMipmapBase` / `FMipmap` | `UnTex.h:160-…` | none (`DataPtr, USize, VSize, UBits, VBits`) | none |
| `ETextureFormat` | `UnTex.h:222-232` | **none**: `TEXF_P8=0, TEXF_RGBA7=1, TEXF_RGB16=2, TEXF_DXT1=3, TEXF_RGB8=4, TEXF_RGBA8=5` | the forks' v400 `TEXF_*` aliases apply unchanged |
| `FColor` | `UnTex.h:39-116` | wrapped in `union { DWORD Int4; struct {BYTE R,G,B,A;}; }`, `GET_COLOR_DWORD(c)` = `(c).Int4`, adds `IntValue()` | source compatible; `GET_COLOR_DWORD(x)` now needs an lvalue-ish `FColor` expression (still true for every use in the forks — verify at compile) |
| `UBitmap` | `UnTex.h:237-263` | `DOUBLE LastUpdateTime` → `INT __LastUpdateTime[2]` + `GetLastUpdateTime()/SetLastUpdateTime(FTime)`; `Lock(FTextureInfo&, FTime Time, INT LOD, URenderDevice*)` | any driver call to `Texture->Lock(Info, <double>, …)` still compiles (FTime has `FTime(double)`), but **reading** `Viewport->CurrentTime` into a `DOUBLE`/`FLOAT` does not (§2.4) |
| `UTexture` | `UnTex.h:267-376` | `Update(FTime)`, `Get(FTime)`, new `Prime()`, `LoadFromSnap`, `LoadFromBMP`; fields (`BumpMap, DetailTexture, MacroTexture, Diffuse, Specular, Alpha, Scale, Friction, MipMult, …, PolyFlags, …, Palette`) unchanged | none beyond FTime |
| `UPalette` | `UnTex.h:122-…` | 4-arg `DECLARE_CLASS` only | none |
| `EPolyFlags` | `Engine/Inc/UnObj.h:221-281` | **no bit value changed**; 4 aliases appended (lines 276-280, quoted below) | the forks' PF_ handling compiles unchanged; the *meaning* of `PF_HighShadowDetail|PF_Modulated` is new (§3.3) |
| `FSpanBuffer` | `Render/Inc/UnSpan.h` (HP2) vs `Render/Src/UnSpan.h` (v400) | identical file (`diff -q` clean) | none |

```cpp
// HP2 Source/Engine/Inc/UnObj.h:276-280 (appended to enum EPolyFlags)
//HP2 Specific
PF_LumosAffected	= PF_HighShadowDetail | PF_Modulated,	// A lumosAffected surface will disappear by default   (0x00800040)
PF_LumosDisappear	= PF_LumosAffected,						// Same as being affected
PF_LumosAppear		= PF_Gouraud,							// The flag that marks appearing rather than disappearing   (0x00200000)
PF_LumosAlwaysOn	= PF_DirtyShadows,						// This flag will make the lumos surface always on   (0x00040000)
```

Free bit for driver-internal use: **`0x20000000`** is not assigned by any HP2 `EPolyFlags` value (`grep 0x20000000 Engine/Inc/UnObj.h` → nothing) and no Render code ORs raw constants into poly flags. §5/§6 use it as a synthetic `PF_AlphaBlend` on 433.

### 2.3 `UViewport` / `UClient` / `UPlayer` (`Engine/Inc/UnCamera.h`, `UnPlayer.h`) — the 33-line diff

| Change | Lines (HP2) | Driver/NSDLDrv impact |
|---|---|---|
| `DOUBLE LastUpdateTime` → `FTime LastUpdateTime` | `UnCamera.h:222` | only viewports write it |
| `DOUBLE CurrentTime` → `FTime CurrentTime` | `UnCamera.h:238` | any `FLOAT t = Viewport->CurrentTime` or `CurrentTime < x` stops compiling (§2.4). XOpenGL only passes it straight to `UTexture::Lock` (OK). |
| new members `TArray<FColor> snapBuff; unsigned int snapU, snapV;` | `UnCamera.h:252-253` | layout only |
| new `UBOOL ScreenShot(const TCHAR*)` (non-virtual) | `UnCamera.h:268` | none |
| new virtuals `TryRenderDevice(…) {}`, `DrawString(…) {}`, `CreateNativeFont(…) {return NULL;}`, `LoadSmackerMovie/BlitNextSmackerFrame/ShutdownSmacker` (no-op defaults) | `UnCamera.h:286-293` | **NSDLViewport compiles unchanged** (all have defaults). `TryRenderDevice` is a *virtual on UViewport* in 433 (was `UWindowsViewport`-local in v400) — see §8.3. Smacker/native-font defaults mean movies/`DrawString` are silent no-ops on SDL. |
| `REN_MatineeIP = 19`, `REN_MAX = 21` | `UnCamera.h:156-157` | none (XOpenGL defines `REN_WalkableSurfs 7` only if absent) |
| `SHOW_Bounds`, `SHOW_Bones` | `UnCamera.h:185-186` | none |
| `UClient`: `BITFIELD NoFractalAnim`, `INT ParticleDensity` | `UnCamera.h:383,394` | config layout only; NSDLClient must not hard-code offsets (it does not) |
| `UPlayer`: `BITFIELD bSoftwareRendering:1` | `UnPlayer.h:27` | Render sets/reads it? — `grep bSoftwareRendering Source/Render` to confirm; drivers never touch it |
| `UCanvas::WrappedPrint(…, INT numChars=0, UBOOL bCursor=false)` | `UnCamera.h:101` | private; XOpenGL's `DrawStats` calls `Canvas->WrappedPrintf` (public, unchanged) |

Fields every GL driver reads — `SizeX, SizeY, ColorBytes, Actor, Actor->FovAngle, Actor->RendMap, Actor->OrthoZoom, Actor->Region, Actor->CameraRegion, HitX/HitY/HitXL/HitYL, Canvas, IsOrtho(), IsFullscreen(), GetWindow(), ResizeViewport(), GetOuterUClient()->{Brightness,NoLighting}` — are all unchanged.

### 2.4 Core-level type breaks that hit the forks (owned by the Core report; listed here for their renderer consequence)

| Break | Evidence | Renderer consequence |
|---|---|---|
| **`FTime`** replaces `DOUBLE` for engine time | `Core/Inc/Core.h:26-53`: a 64-bit fixed-point class with ctors from `float`/`double`, `operator-(FTime)→float`, `==, !=, >`, **no `<`, no `>=`, no implicit conversion to float/double**, and a non-const `GetFloat()`. `appSeconds()` returns `FTime` (`UnGnuG.h:195,263`, `UnVcWin32.h:263-264`). | Any `DOUBLE/FLOAT x = appSeconds();` or `appSeconds() - someFloat` or `<` comparison fails to compile. XOpenGL: only under `UTGLRFRAMELIMIT`/`UNREAL_OLDUNREAL` (not compiled). UTGLR: yes (§6.3). NSDLDrv: yes (§8.3). |
| **4-arg `DECLARE_CLASS(TClass,TSuper,Flags,TPackage)`** | `Core/Inc/UnObjBas.h:326-341` | Both forks' device class declaration must pass the package token. XOpenGL: add 433 to the 4-arg branch. UTGLR: its v400 shim forces 3 args (§6.3). |
| `TArray` has no `Reserve/AddNoCheck/EmptyNoRealloc` | `grep -c` on `Core/Inc/UnTemplate.h` (HP2) → 0 for all three, same as v400 | XOpenGL's `ENGINE_VERSION==400` `FShaderWriterX` shims (`Inc/XOpenGL.h:425-432,500-506`) must also cover 433. |
| No `BOOL/TRUE/FALSE`, no `PF_None`, no `NAME_DevGraphics` | `grep` in HP2 `Core/Inc`, `Engine/Inc` → nothing | XOpenGL's v400 shim block (`Inc/XOpenGLDrv.h:51-65`) must cover 433. |
| Render private header is `Render.h` (with `#include "UnStat.h"` — the render stats moved to `Core/Inc/UnStat.h`) | `Source/Render/Inc/Render.h:18-21` | include `"Render.h"`, not `"RenderPrivate.h"`. `UnRender.h` is include-guarded in 433 (`_UNRENDER_H_`), so the fork's trailing `#include "UnRender.h"` is harmless. `Engine.h` no longer includes `UnMesh.h` (neither fork uses mesh classes). |
| `DLL_EXPORT` is `extern "C"` under gcc | `Core/Inc/UnGnuG.h:97` | Not used by the GL forks' class declarations. Affects NullDrv/SoftDrv (`class DLL_EXPORT U…RenderDevice`) — Core report owns the macro fix. |


## 3. Who calls what in HP2's Render/Engine — the semantics a GL driver must reproduce

### 3.1 Call-site inventory (`grep -rn` over all of `Source/`, excluding the driver implementations)

| Device call | Callers in HP2 | Arguments as passed |
|---|---|---|
| `DrawComplexSurface` | `Render/Src/UnRender.cpp:3541` (Lumos surfaces), `:3550` (every other BSP surface) | `(Frame, Surface, Facet, LumosPolyFlags, (char)iAlpha)` / `(Frame, Surface, Facet, Surface.PolyFlags, 255)` |
| `DrawTriangles` | `Render/Src/UnMeshRn.cpp:129` (all meshes: vertex, LOD **and skeletal**, via `GMeshRn.Flush()`), `Render/Src/UnParticleRn.cpp:503,552` (all particle systems), `Render/Src/UnRender.cpp:3598` (BSP decals incl. actor blob shadows), `Engine/Src/UnCamera.cpp:202` (base `DrawGouraudPolygon`) | see 3.2 |
| `DrawGouraudPolygon` | **none** in `Render`/`Engine`/`Fire` | — |
| `MaxVertices` | `UnMeshRn.cpp:117`, `UnParticleRn.cpp:467` | — |
| `GetOsSurface` | `WinDrv/Src/WinViewport.cpp:939,1005` only | — |
| `DrawTile`, `Draw2DLine`, `Draw2DPoint`, `Draw3DLine`, `ClearZ`, `EndFlash`, `PushHit/PopHit`, `ReadPixels`, `Lock/Unlock`, `SetSceneNode`, `PrecacheTexture` | unchanged v400 callers (`UnRender.cpp`, `UnSprite.cpp`, `Engine/Src/UnCanvas.cpp`, `UnCamera.cpp`, the editor) | unchanged |

Skeletal meshes: `URender::DrawMesh` (`UnMeshRn.cpp:459`) dispatches LOD/skeletal meshes to `URender::DrawLodMesh` (`UnMeshRnLOD.h:30`, `#include`d at `UnMeshRn.cpp:756`), which detects `bSkeletal` (`UnMeshRnLOD.h:98`), skins on the CPU, and emits triangles through `RenderInit`/`RenderSubsurface`/`RenderFlush` (`UnMeshRnLOD.h:655,695,700`) → `GMeshRn` → `DrawTriangles`. **There is no GPU-skinning or skeletal-specific device entry point.** Bone/influence debug display goes through `Draw3DLine`/`Draw2DPoint` (editor only).

### 3.2 `DrawTriangles` — the three shapes of input

| Caller | `Indices` | `NumIndices` | Topology | `NumPts` bound | Notes |
|---|---|---|---|---|---|
| Mesh batcher `MeshRn::Flush` (`UnMeshRn.cpp:125-134`, `#define INDEX 1` at `:79`) | non-NULL | multiple of 3 | **indexed triangle list**; fans are pre-split into tris (`:194-205`), vertices de-duplicated per mesh via `ProcessVert` (`:144-164`) | `VertCount ≤ MaxVertices()` (flush at `:191-192`); `IndexCount` up to `6*MaxVertices()` | one call per (Frame, Span, Texture, PolyFlags) run; `Pts[i]` point into the mesh's `FTransTexture` pool; indices refer to positions in `Pts` |
| Particle systems `ParticleSystem::Render` (`UnParticleRn.cpp:440-563`) | non-NULL | multiple of 3 | **indexed triangle list** (each particle quad/clipped poly fanned into tris at `:535-540`) | `VertCount ≤ MaxVertices()` (flush at `:497-510`, slop `NumPolys*NumVerts*3`); `IndexCount ≤ 3*MaxVertices()` | **`Pts[VertCount..]` may contain entries never referenced by any index** (`VertCount += Max(Count, NumVerts)`, `:543`) — every pointer is valid, but drivers must not assume all `NumPts` verts are used. Particle `Normal` and `Fog` are **never written** (only `Point/ScreenX/ScreenY/RZ/U/V/Light/Flags`; `SetTrans` `:89-118`, `SetTrans2D` `:131-155`): drivers must not read `Fog` unless `PF_RenderFog` is set, and must not feed `Normal` into anything that can produce NaNs. |
| Decals `UnRender.cpp:3591-3601` | `NULL` | `== NumPts` | **one convex polygon → fan** (comment: `// NumIndices=NumPoints => single polygon`) | `FBspNode::MAX_FINAL_VERTICES` (`ClipDecal` uses static arrays of that size, `UnRender.cpp:3778-3779`) | `Fog` is zeroed (`ClipDecal`, `UnRender.cpp:3857`); `Light = FVector(Unlit)` (W not written) |
| Base `URenderDevice::DrawGouraudPolygon` (`UnCamera.cpp:199-203`) | `NULL` | `== NumPts` | fan | caller-dependent | only reached if a driver does not override `DrawGouraudPolygon` and something calls it (nothing in HP2 does) |
| *(dead)* `UnMeshRn.cpp:136-141` (`#if !INDEX`) | `NULL` | `0` | would be a plain list | — | compiled out; the D3D7 rule `NumIndices==NumPts ? FAN : LIST` would misread it. Keep the list fallback anyway. |

Reference implementation (retail D3D7), `Source/D3DDrv/Src/Direct3D7.cpp:1156-1231`: transforms all `NumPts` vertices, then

```cpp
if( Indices )  DrawIndexedPrimitiveVB( D3DPT_TRIANGLELIST, …, NumPts, Indices, NumIndices, 0 );
else           DrawPrimitiveVB( NumIndices == NumPts ? D3DPT_TRIANGLEFAN : D3DPT_TRIANGLELIST, …, NumPts, 0 );
```

Per-vertex colour it feeds: `dcColor = RGBA(Light.X*MaxColor.X, …, Light.W*255)`; for `PF_Modulated` → opaque white and no specular; fog specular only when `(PolyFlags & (PF_RenderFog|PF_Translucent|PF_Modulated)) == PF_RenderFog` (`:1164,1185-1215`). **`Light.W` is the vertex alpha.** SoftDrv's `DrawTriangles` (`SoftDrv/Src/DrawPoly.cpp:4372-4600`) handles the same three shapes (indexed at `+128`, list when `NumIndices!=NumPts` at `+143`, fan otherwise at `+162`) and returns `MaxVertices()=256` (`:4366-4370`).

### 3.3 `cAlpha` and the Lumos surfaces (the only 433 BSP semantic change)

Render side (`Render/Src/UnRender.cpp:3452-3551`, compiled because nothing defines `NO_LUMOS_CODE`):

```cpp
if( (GSurfs[Node->iSurf].PolyFlags & PF_LumosAffected) && !GIsEditor )   // ANY of PF_HighShadowDetail|PF_Modulated
{ … DWORD LumosPolyFlags = GSurfs[Node->iSurf].PolyFlags;
      int   iAlpha         = ( LumosPolyFlags & PF_LumosAppear ) ? 255 : 0;
  if( Viewport->RenDev->SpanBased ) { /* software: add PF_Translucent if AlwaysOn, skip Appear surfaces while !bLumosOn,
                                        strip PF_LumosAffected|PF_LumosAppear|PF_LumosAlwaysOn */ }
  else /* hardware */ {
      if( LumosPolyFlags & PF_LumosAlwaysOn ) {           // distance fade vs pPlayer->fLumosRadius*0.8
          if( LumosPolyFlags & PF_LumosAppear ) { iAlpha = 255*min(1,dist/R); LumosPolyFlags &= ~PF_Occlude; if(iAlpha<100) iAlpha=100; }
          else { iAlpha = 255 - 255*min(1,dist/R); LumosPolyFlags &= ~PF_Occlude; if(iAlpha>200) iAlpha=200; else if(iAlpha==0) LumosPolyFlags |= PF_Occlude; }
      }
      else if( !(LumosPolyFlags & PF_LumosAppear) ) LumosPolyFlags |= PF_Occlude;
  }
  Viewport->RenDev->DrawComplexSurface( Frame, Surface, Facet, LumosPolyFlags, (char)iAlpha );
}
else Viewport->RenDev->DrawComplexSurface( Frame, Surface, Facet, Surface.PolyFlags, 255 );
```

So on a hardware device the flags **still contain `PF_HighShadowDetail` and/or `PF_Modulated`**, `PF_Occlude` may have been removed, and `cAlpha` ∈ {0, 100..255 (appear), 0..200 (disappear), 255}. Near the player, an *appearing* surface gets a small `cAlpha`, a *disappearing* one a large `cAlpha` → **`cAlpha` is transparency, 255 = invisible, 0 = opaque**. Normal surfaces pass 255, which is harmless only because the driver ignores `cAlpha` unless the Lumos blend is active.

Retail D3D7 (`Direct3D7.cpp:813`, `:1829-1853`), **confirmed in the shipped binary** (`i686-w64-mingw32-objdump -d Assets/System/D3DDrv.dll`, `0x10009318: test $0x800040,%eax` followed by `SetRenderState(27 ALPHABLENDENABLE,1)`, `(19 SRCBLEND, 6 INVSRCALPHA)`, `(20 DESTBLEND, 5 SRCALPHA)`):

```cpp
// multitexture (base+lightmap) path only:
D3DCOLOR clr = (FColor(Stages[0].MaxColor * Stages[1].MaxColor).TrueColor()) | (((DWORD)cAlpha) << 24);
// SetBlending, after the "no blend bits → blending off" test:
else if( PolyFlags & PF_LumosAffected ) { ALPHABLENDENABLE=TRUE; SRCBLEND=INVSRCALPHA; DESTBLEND=SRCALPHA; }
```

Stage-0 alpha is `texture.a * diffuse.a` (`SSS_Classic`, `:2266-2281`), so the Lumos result is `out = src*(1-a) + dst*a` with `a = texA * cAlpha/255`.

**Two retail quirks the GL ports must decide on (evidence-backed recommendation in bold):**

1. *The `&` test catches plain `PF_Modulated`.* Every pure-modulated poly — modulated decals, i.e. **all actor blob shadows** (`Engine/Classes/ActorShadow.uc:76 Style=STY_Modulated` → `PolyFlags = PF_Modulated|PF_RenderFog`, `UnRender.cpp:3593-3594`; used by `HChar`, `HPawn`, `Harry`, `ChocolateFrog`, `HarryShadow`, `BroomShadow`), modulated particles (`UnParticleRn.cpp:369`), modulated sprites/tiles and modulated mesh materials — goes through the Lumos blend on D3D7 with vertex alpha 255 (`:1190-1193`). I parsed `Assets/System/Engine.u` (package v79): `ShadowT` (export 5339) is P8 with `Palette21`, a 0..255 grey ramp with **every A = 255**, `MaxColor=7e7e7eff`. With texA=1 the D3D7 result is `dst` — the shadow is invisible on retail D3D7. The texture itself (max ≈ 0x7E ≈ identity under UE1's 2× modulate, dark centre) is unmistakably authored for `DST_COLOR,SRC_COLOR` modulation, which is also what SoftDrv does (`DrawSurf.cpp:20385-20397`, `DrawPoly.cpp`) and what `UnParticleRn.cpp:417-421` ("Fade toward grey") assumes. **ACTION: in the GL ports treat a poly as Lumos only when `PolyFlags & PF_HighShadowDetail` AND it carries a blend bit (`PF_Modulated|PF_Translucent`); plain `PF_Modulated` keeps the classic modulate blend.** Verify once on real hardware: run retail `Game.exe` under Wine (wined3d, D3D7 path) in any level and look for Harry's shadow; record the result in `docs/porting/`.
2. *`cAlpha` is ignored on D3D7's non-multitexture path* (`UseMultitexture=False`, or surfaces with a macro texture, or no lightmap): vertex alpha is forced to 0xff by `UpdateModulation` (`:3326-3330`), so a Lumos surface is invisible there regardless of distance. **ACTION: apply `cAlpha` on every GL path** (the GL forks draw base×lightmap in one pass anyway).

GL mapping (exact for texA=1, the only case Lumos content uses): `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` with fragment alpha `= texA * (255 - cAlpha)/255`, depth writes per the passed `PF_Occlude`, and **`PF_Modulated`/`PF_HighShadowDetail` stripped before the driver derives its modulate/lighting behaviour** (on D3D7 the modulate bit of a Lumos surface only selects the blend; the base×lightmap texturing is the normal one).

### 3.4 Vertex alpha: `Light.W` and the `PF_Highlighted|PF_Translucent` alpha-blend mode (433-new)

* Meshes: `UnMeshRnLOD.h:369-371` — `if( Owner->Opacity < 1.f ) ExtraFlags |= PF_Highlighted | PF_Translucent;` and `:620-623` — `if( SoftwareRendering ) Vert.Light *= Owner->Opacity; Vert.Light.W = Owner->Opacity;` (`AActor::Opacity`, `Engine/Inc/EngineClasses.h:921`). Hardware devices get **un-premultiplied colour plus alpha in `Light.W`**.
* Particles: `UnParticleRn.cpp:410-423` — `PColor = (LightColor*pColor, W=1)`, then `if (Alpha<1) PColor *= Alpha` (all four components; `FPlane::operator*=`, `Core/Inc/UnMath.h:1073-1077`) and for modulated `+= (1-Alpha)*0.5` (towards grey).
* D3D7: `Light.W` → vertex alpha for every `DrawTriangles` vertex (`:1203-1213`); `SetBlending` maps `PF_Highlighted` + `PF_Translucent` to `SRCALPHA, INVSRCALPHA` ("Post-multiplied source", `:1861-1867`) and `PF_Highlighted` alone to `ONE, INVSRCALPHA` (premultiplied; `DrawTile` sets `PF_Highlighted` for alpha palettes, `:1245-1246`).
* SoftDrv agrees: `DrawPoly.cpp:4380-4385` treats `(PF_Highlighted|PF_Translucent)` as one mode and drops it for masked textures.

**ACTION (both forks):** on 433 map `(PolyFlags & (PF_Highlighted|PF_Translucent)) == (PF_Highlighted|PF_Translucent)` to straight alpha blending (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`) with fragment alpha `*= Light.W`, *before* the fork's own "`PF_Translucent` wins" ordering sees the flags (both forks test `PF_Translucent` before `PF_Highlighted` today, which would draw fading actors as additive ghosts). When `PF_Masked` is also set, follow SoftDrv: drop the alpha-blend pair and draw masked.

### 3.5 Other 433 behaviours that reach the device

| Behaviour | Source | Driver consequence / ACTION |
|---|---|---|
| Decal style → flags: `STY_Modulated → PF_Modulated|PF_RenderFog`, `STY_Translucent → PF_Translucent`, else `0` (v400 always passed `PF_Modulated`, epic `UnRender.cpp:3107`) | `UnRender.cpp:3591-3596` | Opaque decals (`PolyFlags=0`) get `PF_Occlude` from the drivers' own precedence rules — fine. `PF_RenderFog` on a modulated poly: D3D7 strips it (`AdjustPolyFlags`, `:1819-1820`); XOpenGL keeps it (`SetTexture.cpp:900-901` strips only for translucent) and then runs its modulated-fog shader branch with the decal's zeroed `Fog` — harmless (fog alpha 0 → identity), but **strip `PF_RenderFog` when `PF_Modulated` is set** on 433 to match D3D7 exactly. |
| Two-sided depth bias: `Pts[v]->RZ /= (1.f + Pts[v]->RZ*SortBias)` with `SortBias = -0.001f` when `Owner->bTwoSidedBias && (MatFlags & PF_TwoSided)` — **HP2-only**, and `bTwoSidedBias` defaults to **true** for every actor (`Engine/Classes/Actor.uc:312,1741`) | `UnMeshRnLOD.h:674,686` | D3D7 derives depth from `RZ` (`dvRHW = RZ*RProj.Z`), so two-sided mesh parts (hair cards, capes, wings) are pulled slightly towards the camera. **Both GL forks derive depth from `Point.Z` and lose the bias** → possible z-fighting of two-sided mesh sections with the body. ACTION: on 433, in `DrawTriangles`, when `PolyFlags & PF_TwoSided`, apply a small depth offset (`glPolygonOffset(-1,-1)` around the batch, or rescale `Point` so that `Point.Z = Frame->Proj.Z / RZ` for vertices with `!P->Flags`). Verify on Harry's hair/robe close-ups. |
| Near-clip / screen clamp: meshes clamp `ScreenX/ScreenY` into the frame (`UnMeshRn.cpp:410-414`); decals clamp outcoded verts (`UnRender.cpp:3850-3852`) | — | irrelevant to the GL forks (they use `Point`), relevant to SoftDrv only |
| `LINE_PreTransformed` | §2.2 | no caller; honour it in `Draw3DLine` overrides |
| `UTexture::Alpha` | XOpenGL uses `Info.Texture->Alpha` as an opacity multiplier when `> 0` (`DrawGouraud.cpp:110-117`, `DrawTile.cpp:59-62`, `DrawComplex.cpp:39`) — a 227 convention; in UE1 v400/433 it is "reflectivity" (`UnTex.h:280`), default 0 (`Engine/Src/UnTex.cpp:306`) | Pre-existing v400 behaviour. If any HP2 texture has `Alpha>0`, XOpenGL would fade it. Verify with a one-off log in `SetTexture` (print name when `Info.Texture->Alpha>0`) over a playthrough of the first levels; if hits appear, force `TextureAlpha=1` for 433. |


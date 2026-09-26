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

1. **The 433 render-device face is small but breaks every driver that is not HP2's own.** Versus v400, `URenderDevice` (`Engine/Inc/UnRenDev.h`) changes exactly four things: 5-arg `DrawComplexSurface(…, DWORD PolyFlags, BYTE cAlpha)`, new pure `MaxVertices()` and `DrawTriangles(Frame, Info, Pts, NumPts, _WORD* Indices, NumIndices, PolyFlags, Span)`, `DrawGouraudPolygon` demoted to a non-pure forwarder, and a default-NULL `GetOsSurface` (§2.1). `FSceneNode`, `FSurfaceInfo/Facet`, `FTextureInfo`, `ETextureFormat` (P8, RGBA7, RGB16, DXT1, RGB8, RGBA8) and every `EPolyFlags` bit value are unchanged; 433 only appends the aliases `PF_LumosAffected/Disappear/Appear/AlwaysOn` (§2.2). Engine-type breaks that also hit the drivers: `FTime` instead of `DOUBLE` time, 4-arg `DECLARE_CLASS`, and `Render.h` instead of `RenderPrivate.h` (§2.4).
2. **Semantics matter more than signatures (§3).** All meshes (skeletal included, skinned on the CPU), particles and decals go through `DrawTriangles` only. Nothing in HP2 calls `DrawGouraudPolygon` any more. `DrawTriangles` has three input shapes (indexed list / `Indices==NULL && NumIndices==NumPts` fan / plain list). `cAlpha` is **transparency** (255 = gone) and is used only for Lumos BSP surfaces. `Light.W` carries per-vertex opacity, and `PF_Highlighted|PF_Translucent` means straight alpha blending. The retail `Direct3D7.cpp` is the behavioural spec (§5.2).
3. **XOpenGLDrv (`hp2-v433-compat`, today identical to `ut99-v400-compat`)** has 31 `ENGINE_VERSION==400`/`!=400` preprocessor lines (`grep -rn` over `Src` and `Inc`), grouped as sites X1-X28 in §4.1. About 25 must also fire on 433: add a derived `XOGL_UE1_V4XX`. Two need their own 433 branch: the `Render.h` include and the 4-arg `DECLARE_CLASS` (§4). New face code goes under `XOGL_HP2`: C1 Lumos `DrawComplexSurface`, C2 `MaxVertices=256` + `DrawTriangles`, C3 `HP2AdjustPolyFlags` (straight alpha through a synthetic `PF_AlphaBlend=0x20000000`, the one free bit), C4 `LINE_PreTransformed`, C5 alpha-palette tiles, and optional C7 two-sided depth bias (§5.3).
4. **UTGLR (`hp2`, today identical to `main`)**: HP2 is closer to *upstream* UTGLR (v436: 4-arg `DECLARE_CLASS`, `FTime`) than to the v400 port. As-is it fails to compile, because `UTGLR_ENGINE_UT99DC` (defined by the HP2 CMake) forces the v400 types at three sites. Split that macro: platform stays `UT99DC`, and a derived `UTGLR_ENGINE_V400` covers the types. Then implement H1-H7 under `UTGLR_ENGINE_HP2`, reusing upstream's Rune/227 `PF_AlphaBlend` plumbing through a `UTGLR_HAS_ALPHABLEND` feature macro (§6).
5. **gl4es needs no change** (§7). **NSDLDrv's GL/GLES/software choice keys on the device-name substrings `OPENGL`/`XOPENGL` and needs no logic change.** It still has four 433 compile breaks (4-arg `DECLARE_CLASS` ×2, two `FTime` sites), and the web inis must name the GL device in *both* `GameRenderDevice` and `WindowedRenderDevice` (§8). NullDrv needs the new face (§9). Every device must return `MaxVertices()=256` (§9 note).
6. **Workspace blocker:** the three fork clones sit inside `/home/ion/hp2-ufrontend/ufrontend/` as untracked directories. The CMake presets expect them at `/home/ion/hp2-ufrontend/{XOpenGLDrv,utglr-copy,gl4es}`, so move them before the first configure (§1).

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


## 4. XOpenGLDrv fork: version-gate inventory

Tree: `/home/ion/hp2-ufrontend/ufrontend/XOpenGLDrv` (see 1.1 — to be moved to `/home/ion/hp2-ufrontend/XOpenGLDrv`), branch `hp2-v433-compat` @ `87f6e89`. HP2 `Core/Inc/UnObjVer.h:14` → `#define ENGINE_VERSION 433`; no `UNREAL_OLDUNREAL`, `UNREAL_TOURNAMENT_OLDUNREAL`, `XOPENGL_MODIFIED_LOCK`, `UTGLRFRAMELIMIT` is defined by the HP2 build (`Source/XOpenGLDrv/CMakeLists.txt` passes only `XOPENGLDRV_EXPORTS UPACKAGE_NAME=XOpenGLDrv SDLBUILD=1 SDL2BUILD=1 SDL3BUILD=0`). TCHAR is narrow on every HP2 target (`Core/Inc/Core.h:94-113`, `_UNICODE` is never defined by `Source/CMakeLists.txt`), exactly as in the v400 template.

**Key point:** the fork's version matrix has four classes of gate, and 433 falls through them differently from 400:

| Gate form | 400 | 433 | Consequence |
|---|---|---|---|
| `#if ENGINE_VERSION==400` | taken | **not taken** | every ufront v400 adaptation is lost on 433 → must become "400 or 433" |
| `#if ENGINE_VERSION==227` / `UNREAL_OLDUNREAL` / `UNREAL_TOURNAMENT_OLDUNREAL` … `#else` | else | else | identical behaviour — nothing to do |
| `ENGINE_VERSION>=430 && ENGINE_VERSION<1100` | not taken (hence the explicit `\|\| ==400`) | **taken** | 433 already gets the UT-family light layout and `MAX_LIGHTS 256` |
| `ENGINE_VERSION==430`, `==436`, `>=436 && <1100` | not taken | **not taken** | 433 falls to the final `#else` — wrong for `Render.h` include and 4-arg `DECLARE_CLASS` |

### 4.1 Every `ENGINE_VERSION==400` site (the ones 433 must join)

| # | File:line | What it adapts | Needed on 433? | Why (HP2 evidence) |
|---|---|---|---|---|
| X1 | `Inc/XOpenGLDrv.h:37-38` | `#include "RenderPrivate.h"` | **No — needs its own branch** | HP2 has no `RenderPrivate.h`; private header is `Render/Inc/Render.h` (§1.6). Add `#elif ENGINE_VERSION==433` → `#include "Render.h"`. |
| X2 | `Inc/XOpenGLDrv.h:43-45` | `#if ENGINE_VERSION!=400 #include "UnRender.h"` | harmless as is | HP2 `UnRender.h` is include-guarded (`_UNRENDER_H_`), so the extra include is a no-op; leave the `!=400` test alone. |
| X3 | `Inc/XOpenGLDrv.h:51-65` | `typedef UBOOL BOOL; TRUE/FALSE; PF_None 0; NAME_DevGraphics→NAME_Init` | **Yes** | HP2 Core/Engine define none of `BOOL/TRUE/FALSE/PF_None/NAME_DevGraphics` (§2.4); `NAME_Init` exists (`Core/Inc/UnNames.h:195 REGISTER_NAME(762, Init)`). |
| X4 | `Inc/XOpenGL.h:58` (`==436\|\|==430\|\|==400`) | `clockFast/unclockFast` | **Yes** | without it neither macro is defined on 433 → compile error in every `STAT(clockFast(...))`. |
| X5 | `Inc/XOpenGL.h:183-238` | `PF_AlphaBlend=0`, `PF_Straight_AlphaBlend=0`, `PF_Premultiplied_AlphaBlend=0`, `TEXF_*` 469-name aliases, `FIsCompressedFormat`, `FTextureFormatString`, `enum ERenderZTest` | **Yes, but not verbatim** | `ETextureFormat` is unchanged (§2.2) so the TEXF aliases/stubs/`ERenderZTest` apply as is. The three alpha-blend flags must **not** be 0 on 433: they become the synthetic flag `0x20000000` that carries the 433 alpha modes (§5.3 C3). |
| X6 | `Inc/XOpenGL.h:425-432, 500-506` | `FShaderWriterX`: `Empty(1000)+Add()` instead of `Reserve/AddNoCheck`; `Empty()+Add()` instead of `EmptyNoRealloc/AddNoCheck` | **Yes** | HP2 `Core/Inc/UnTemplate.h` `TArray` (a `TRefArray<T>` derivative, lines 353-660) has `Add/Empty(Slack)/AddItem/AddZeroed/RemoveItem/Last` but no `Reserve`, `AddNoCheck`, `EmptyNoRealloc`. |
| X7 | `Inc/XOpenGL.h:667-672` | `INT MaxTextureSize` member | **Yes** | HP2 `URenderDevice` (`UnRenDev.h:41-60`) has no `MaxTextureSize`. |
| X8 | `Inc/XOpenGL.h:521-531` | `DECLARE_CLASS` selection (not a `==400` gate, but the v400 path is the final 3-arg `#else`) | **No — needs the 4-arg form** | HP2 `DECLARE_CLASS(TClass,TSuper,Flags,TPackage)` (`Core/Inc/UnObjBas.h:326`). Add `\|\| ENGINE_VERSION==433` to the `ENGINE_VERSION==430` branch. |
| X9 | `Src/XOpenGL.cpp:409-418` | `OpenGLVersion = GL_ES_WEBGL` under `__EMSCRIPTEN__` | **Yes** | WebGL2 is platform, not engine. |
| X10 | `Src/XOpenGL.cpp:780-788` | `SelectGLVersion` short-circuit (3.3, no temporary SDL windows) | **Yes** | NSDLDrv owns the context on HP2 too (§8). |
| X11 | `Src/XOpenGL.cpp:884-900` | `CreateOpenGLContext` adopts `SDL_GL_GetCurrentContext()` / `emscripten_webgl_get_current_context()` | **Yes** | same NSDLViewport. |
| X12 | `Src/XOpenGL.cpp:2014-2020` | Post-process present uses `Viewport->SizeX/SizeY` (no `PhysicalSizeX/Y`) | **Yes** | HP2 `UViewport` has no `PhysicalSizeX/Y` (`grep PhysicalSize Engine/Inc` → nothing). |
| X13 | `Src/XOpenGL.cpp:2025-2031` | no `SDL_GL_SwapWindow` in `Unlock` (NSDLViewport swaps) | **Yes** | same NSDLViewport. |
| X14 | `Src/XOpenGL.cpp:2141-2147` | `GammaCorrection<=0 → 0.5` | **Yes** | HP2 `UClient::Brightness` is config-loaded exactly like v400; the web client zero-initialises the device on the StaticFindObject path (template NSDLClient). |
| X15 | `Src/XOpenGL.cpp:2150-2157` | `GammaMult<=0 → 1.75` | **Yes** | same config-default problem (driver `StaticConstructor` defaults are not applied to CPF_Config floats loaded as 0). Verify on 433: log `GammaMultiplier` after `Init`. |
| X16 | `Src/XOpenGL.cpp:2167-2173` | `Gamma<=0 → 1` guard | **Yes** | same. |
| X17 | `Src/XOpenGL.cpp:2341-2348` | `Exit`/`UnsetRes`: do not destroy the adopted context | **Yes** | same NSDLViewport. |
| X18 | `Src/XOpenGL.cpp:1625,1642,1650` (`(>=430&&<1100)\|\|==400`) | HW light list / `FPlane.X/Y/Z` / `Region.ZoneNumber` | already covered | 433 is inside `>=430 && <1100`. HP2 `AActor` has `WorldLightRadius()` (`Engine/Inc/AActor.h:41`), `FGetHSV` (`UnTex.h:117`). No change. |
| X19 | `Src/DrawGouraud.cpp:75-80` | `NoNearZ = FALSE` (no `HACKFLAGS_NoNearZ`) | **Yes** | HP2 has `GUglyHackFlags` (`Core.h:445`) but no `HACKFLAGS_*` (`grep HACKFLAGS_ Core/Inc Engine/Inc` → nothing). |
| X20 | `Src/DrawComplex.cpp:121-125` (`==1100\|\|==400`) | bump-map `Lock(Info, Viewport->CurrentTime, 0, this)` | **Yes (either branch compiles)** | HP2 `Lock(FTextureInfo&, FTime, INT, URenderDevice*)` (`UnTex.h`) and `Viewport->CurrentTime` is `FTime` → the v400 branch compiles and is the semantically right one (the `FTime()` branch passes time 0 to animated bump maps). |
| X21 | `Src/SetTexture.cpp:257-281, 347-355, 423-434, 645-655` | WebGL2 software decode of non-4-aligned DXT1 (`XOpenGLDecodeDXT1Image`, vendored `decompress.h`) | **Yes** | platform issue; HP2 `ETextureFormat` has `TEXF_DXT1=3` and HP2 content does ship DXT1 (S3TC textures exist in the retail packages — verify: `ucc`-free check is `grep -c` impossible on binary; run the port once with `DebugLevel` and count `TEXF_3` uploads). |
| X22 | `Src/SetTexture.cpp:668-674` (the `#else` of `==227\|\|UT_OLDUNREAL`) | DXT1 `CompImageSize` without `FTextureBytes` | already covered (it is an `#else`) | 433 has no `FTextureBytes` either. |
| X23 | `Src/SetTexture.cpp:686-693` | `appMsgf` → `GWarn->Logf` | **Optional** | HP2 *has* `appMsgf` (`Core/Inc/UnFile.h:59`, returns `const void`), so the else branch would compile; keep the 400 branch for identical behaviour (it is editor-only). |
| X24 | `Src/CheckExtensions.cpp:294-301` | accept `WEBGL_compressed_texture_s3tc` | **Yes** | platform. |
| X25 | `Src/ShaderProgram.cpp:487-510` | `DumpShader` without `TArray<FString>::AddItem` | **Yes** | HP2 `TArray::AddItem` (`UnTemplate.h:621-627`) is `INT Index=Add(); (*this)(Index)=Item;` — the same assign-onto-unconstructed-memory bug as v400. |
| X26 | `Src/ShaderProgram.cpp:565-570` | `%s` + raw ANSI compiler log | **Yes** | narrow TCHAR. |
| X27 | `Src/ShaderProgram.cpp:909-917` | `ADD_OPTION` via `TEXT(#x)` instead of `L ## #x` | **Yes** | narrow TCHAR. |
| X28 | `Src/XOpenGL.cpp:2540-2544` (`ENGINE_VERSION<400`) | `Canvas->Color` vs `DrawColor` | already covered | HP2 `UCanvas::Color` exists. |

### 4.2 Other gates (no 433 action; listed for completeness)

| Gate | Sites | Behaviour on 433 |
|---|---|---|
| `ENGINE_VERSION==227` | `DrawGouraud.cpp:111,133,189,249,378`; `SetTexture.cpp:73,83,213,378,409,606,616,963`; `DrawComplex.cpp:77,114,130,194,204`; `ShaderProgram.cpp:83,105,688,748`; `EditorHit.cpp:133`; `XOpenGL.cpp:2196,2215`; `XOpenGLDrv.h:26` | else-branch, same as v400. Notable else-behaviours that *do* matter for HP2: `SetTexture.cpp:963` → **culling is never toggled** (so two-sided/one-sided is decided by the engine's own backface rejection; fine), `DrawGouraud.cpp:111-115` / `DrawTile.cpp:57-62` → `UTexture::Alpha>0` fades the poly (§3.5 row "UTexture::Alpha"), `SetTexture.cpp:73-79` → realtime textures are treated as **always stale** (re-upload whenever `bRealtimeChanged`; HP2 uses realtime textures for `FireTexture`/`ScriptedTexture` the same way — no change). |
| `UNREAL_OLDUNREAL` / `UNREAL_TOURNAMENT_OLDUNREAL` | `XOpenGL.h:61,70,163,346,515,521,525,687,1974,1987,1992,2009,2061`; `XOpenGL.cpp:131,205,212,236,270,273,298,307,326,437,449,476,480,541,620,1119,1572,1583,1597,1717,1726,2243,2276,2377,2390,2434,2458,2479,2503,2582`; `SetTexture.cpp:75,85,115,450,656`; `DrawTile.cpp:44,91,111,252,270,324,342`; `DrawGouraud.cpp:233,302`; `ShaderProgram.cpp:868` | else-branch, identical to v400. `DrawGouraudPolyList`/`DrawGouraudTriangles`/`SupportsTextureFormat`/`UpdateTextureRect` are not compiled — correct, 433 has none of them. |
| `XOPENGL_MODIFIED_LOCK` | `XOpenGL.h:80`, `DrawGouraud.cpp:57,154` | 0 → classic `Texture->Lock/Unlock` path; 433 `UTexture::Lock(FTextureInfo&, FTime, INT, URenderDevice*)` matches. |
| `UTGLRFRAMELIMIT` | `XOpenGL.cpp:176,244,2048` | not defined → the `FTime` arithmetic inside (which would not compile against 433 `FTime`: no `<`) stays dead. **Do not enable it on 433.** |
| `ENGINE_VERSION==1100`, `==300` | `SetTexture.cpp:253,314`; `XOpenGL.cpp:2080-2086` | not taken; `OPENGL_SRGB_GAMMA_OFFSET 0.15f` like v400. |
| `SDLBUILD`, `__EMSCRIPTEN__`, `__EMSCRIPTEN_PTHREADS__`, `_WIN32`, `__LINUX__`, `MACOSX`, `__LINUX_ARM__` | throughout `XOpenGL.cpp`, `CheckExtensions.cpp`, `XOpenGL.h:45,600,654,2037`, `DrawTile.cpp:265,337`, `XOpenGLDrv.h:68-80` | platform gates, engine-independent; the HP2 CMake glue forces `SDLBUILD=1 SDL2BUILD=1`. No change. |
| `ENGINE_VERSION>=430 && <1100` | `XOpenGL.h:107` (`MAX_LIGHTS 256`), `XOpenGL.cpp:1625,1642,1650` | taken on 433 (good). |
| `ENGINE_VERSION>=436 && <1100`, `==430` | `XOpenGLDrv.h:28,30`; `XOpenGL.h:523,527` | **not** taken → fixed by X1/X8. |


## 5. XOpenGLDrv fork: engine API surface, the retail driver delta, exact 433 change list, gating scheme

### 5.1 Engine API surface the fork touches (extracted with `grep -oh` over `Src/*.cpp`; status vs HP2 433)

| Engine symbol | Members used by XOpenGL | 433 status |
|---|---|---|
| `URenderDevice` overrides (`Inc/XOpenGL.h:1955-1984`) | `Init, SetRes, Exit, Flush, Exec, Lock, Unlock, DrawComplexSurface(3-arg), DrawGouraudPolygon, DrawTile, Draw3DLine, Draw2DLine, Draw2DPoint, ClearZ, PushHit, PopHit, GetStats, ReadPixels(FColor*), EndFlash, DrawStats, SetSceneNode, PrecacheTexture` + own `PushClipPlane/PopClipPlane/SetZTestMode` | all signatures identical to HP2 `UnRenDev.h:67-94` **except `DrawComplexSurface`** (5-arg) — and the two new pure virtuals `MaxVertices`, `DrawTriangles` are missing → `UXOpenGLRenderDevice` is abstract on 433 until §5.3 C1/C2 land. `DrawGouraudPolygon` override stays valid (same signature, now non-pure). `Draw2DClippedLine`/`GetOsSurface` not overridden (fine). |
| `URenderDevice` fields | `Viewport, SpanBased, FullscreenOnly, SupportsFogMaps, SupportsDistanceFog, VolumetricLighting, ShinySurfaces, Coronas, HighDetailActors, SupportsTC, PrecacheOnFlip, SupportsLazyTextures, PrefersDeferredLoad, DetailTextures, DescFlags, Description` | identical in HP2 (`UnRenDev.h:41-60`); `MaxTextureSize` is the fork's own under X7 |
| `FSceneNode` | `Viewport, Level, X, Y, XB, YB, FX, FY, FX2, FY2, Zoom, Coords, Uncoords, RProj, NearClip, Mirror, Recursion` | identical (§2.2). `NearClip` exists in HP2 (`UnRender.h`) — verify with `grep -n NearClip Source/Engine/Inc/UnRender.h`. |
| `FSurfaceInfo` | `PolyFlags, Level, Texture, LightMap, MacroTexture, DetailTexture, FogMap, FlatColor` (+ 227-only `BumpMap/EnvironmentMap/HeightMap`, not compiled) | identical |
| `FSurfaceFacet` / `FSavedPoly` / `FTransform` | `MapCoords.{Origin,XAxis,YAxis,ZAxis}`, `Polys`, `Poly->{Next,NumPts,Pts[]}`, `Pts[i]->Point` | identical |
| `FTransTexture` | `Point, Normal, U, V, Light (incl. .W), Fog (incl. .W), Flags` | identical layout; `Light.W` = vertex alpha on 433 (§3.4); `Normal`/`Fog` not written by particles (§3.2) |
| `FTextureInfo` | `Texture, CacheID, Pan, Format, UScale, VScale, USize, VSize, NumMips, Mips[i]->{DataPtr,USize,VSize}, Palette, bRealtimeChanged, Load(), Unload()` (+227-only `RenderTag, Modifier, UClampMode, VClampMode, Name, Type, ArrayCount`) | identical (§2.2); `MaxColor` is not read by XOpenGL (it uploads unscaled), `bHighColorQuality/bParametric` not read |
| `ETextureFormat` | `TEXF_P8, TEXF_RGBA7 (via TEXF_BGRA8_LM), TEXF_RGB16 (via TEXF_R5G6B5), TEXF_DXT1 (via TEXF_BC1), TEXF_RGB8, TEXF_RGBA8 (via TEXF_BGRA8)` | identical values 0..5 (`UnTex.h:222-232`) → aliases of X5 are correct on 433 |
| `UTexture` | `Alpha, BumpMap, DetailTexture, MacroTexture, Diffuse, Specular, Scale, PolyFlags, Lock(Info,Time,LOD,RenDev), Unlock(Info), GetFullName/GetPathName/GetOuter` | identical names; `Lock` takes `FTime` (callers pass `Viewport->CurrentTime`, an `FTime` on 433 → fine) |
| `UViewport` | `SizeX, SizeY, ColorBytes, CurrentTime, HitX/HitY/HitXL/HitYL, Canvas, Actor->{FovAngle,OrthoZoom,RendMap,Region,CameraRegion}, IsOrtho(), GetWindow(), ResizeViewport(BLIT_*), GetOuterUClient()->{Brightness,…}` (+469-only `PhysicalSizeX/Y`, not compiled) | identical except `CurrentTime` is `FTime` (§2.3); `BLIT_OpenGL=0x80`, `BLIT_HardwarePaint=0x100` unchanged (`UnCamera.h:34-45`) |
| `AActor` (HW lights) | `LightType, LightRadius, LightBrightness, LightHue, LightSaturation, LightCone, LightEffect, LightPeriod, LightPhase, VolumeBrightness, VolumeFog, VolumeRadius, bStatic, bMovable, Location, Region.ZoneNumber, WorldLightRadius()` | present in HP2 (`AActor.h:41`, `EngineClasses.h`); only used when `UseHWLighting=True` (web inis keep it off) |
| `UCanvas` | `Color, WrappedPrintf, SmallFont` (`DrawStats`) | present |
| Globals / helpers | `GIsEditor, GWarn, GConfig, GUglyHackFlags (read only in dead 469 code), appSprintf, appToAnsi, appFromAnsi, appRealloc, appMemcpy, appErrorf, LocalizeError, debugf, FGetHSV, FTriple (469-only path), FString::Printf, Parse/ParseCommand` | all present in HP2 Core/Engine (checked by extracting every `app*`/`G*`/`Parse*`/`Localize*` identifier and grepping `Core/Inc`, `Engine/Inc`; the only misses were GL typedefs and `appGetSystemErrorCode`, which sits in WGL-only code) |
| `LE_MAX`, `REN_MAX`, `REN_*` (shader `#define`s, `ShaderProgram.cpp:83-109`) | `LE_MAX+1`, `REN_MAX+1`, `REN_Zones/PolyCuts/Polys/PlainTex/WalkableSurfs` | HP2 `LE_MAX=20` (`EngineClasses.h:452`), `REN_MAX=21` (`UnCamera.h:157`): the fork emits `LE_Sunlight 21u`, `REN_Normals 22u` — harmless sentinels, no change |
| Registration | `IMPLEMENT_PACKAGE(XOpenGLDrv)`, `IMPLEMENT_CLASS(UXOpenGLRenderDevice)`, `autoInitializeRegistrantsXOpenGLDrv` (declared only), `UPACKAGE_NAME` | HP2 macros in `UnObjBas.h:350-434`; static-build package symbol plumbing (`GLoaded<Pkg>`/`UPACKAGE_NAME`) is the Core report's subject — the driver needs no change as long as HP2 Core carries the template's `UNREAL_STATIC` scheme |
| GL / SDL | own `glad.h/glext.h` (desktop GL loader, `Src/glad.cpp`), own `glm/`, `SDL2/SDL.h`, `emscripten/html5.h` (`XOpenGL.cpp:20-120`) | engine-independent |

### 5.2 What the retail HP2 D3D7 driver changed vs the v400 D3D7 driver (the behavioural spec for the GL ports)

Epic v400 ships the same driver source (`/home/ion/epic-sources/ut99-v400/D3DDrv/Src/Direct3D7.cpp`), so `diff --strip-trailing-cr -w` against `Source/D3DDrv/Src/Direct3D7.cpp` gives the exact 433-era device delta (180 hunks, 1539 diff lines). Driver-behaviour-relevant hunks only:

| # | HP2 line(s) | Change | GL-port relevance / ACTION |
|---|---|---|---|
| D1 | 782-805, 812-815 | `DrawComplexSurface(…, PolyFlags, cAlpha)`; every `Surface.PolyFlags` in the body replaced by the **argument** `PolyFlags`; base×lightmap vertex colour alpha = `cAlpha` | §3.3; ACTION C1 |
| D2 | 1151-1233 | `MaxVertices()=256`; `DrawGouraudPolygon` **replaced** by `DrawTriangles` (indexed list / fan / list); one pass with vertex fog in specular; vertex alpha `Light.W*255` | §3.2; ACTION C2 |
| D3 | 1192-1193, 1255 | **modulated polys and modulated tiles use vertex colour pure white** (v400: `MaxColor`) | XOpenGL already draws modulated with raw texel (`DrawGouraud_GLSL.cpp` "`else if DF_Modulated: TotalColor = Color`", `DrawTile.cpp:50-56` sets white) → no change. UTGLR: see §6. |
| D4 | 330-345 | 16-bit P8 upload: modulated palettes are not brightness-normalised by `MaxColor` | 16-bit-only; GL forks upload RGBA8 → no change |
| D5 | 350-355 | 16-bit brightness factor `1.4` → `1.0` ("to please the artists") | 16-bit-only → no change |
| D6 | 1815-1826 | `AdjustPolyFlags`: `PF_Highlighted` polys no longer get `PF_Occlude` (v400: they did) | XOpenGL `GetPolyFlagsAndDrawFlags` (`SetTexture.cpp:899-903`) already excludes `PF_Highlighted` → matches 433. UTGLR: §6. |
| D7 | 1844-1854 | Lumos blend `INVSRCALPHA/SRCALPHA` when `PolyFlags & PF_LumosAffected` (checked *after* "no blend bits → off", *before* Invisible/Highlighted/Translucent/Modulated) | §3.3; ACTION C3 |
| D8 | 1861-1870 | `PF_Highlighted` moved **before** `PF_Translucent`; `PF_Highlighted|PF_Translucent` → `SRCALPHA, INVSRCALPHA` (straight alpha), `PF_Highlighted` alone → `ONE, INVSRCALPHA` | §3.4; ACTION C3 |
| D9 | 1911-1912 | multitexture stage 1 (lightmap) `MODULATE` → `MODULATE2X` | GL forks already scale lightmaps ×2 (XOpenGL `LightMapIntensity*255/127`, `DrawComplex_GLSL.cpp` lightmap fetch) → matches 433 |
| D10 | 981-1066 | detail textures: `DetailMax=3`, per-layer scale `4.223` (v400: 4, 4.0), grey `0x80` (v400 `0x7F`); drawn even when `DetailTextures=False` in the non-MT path | XOpenGL already uses `4.223` and `DetailMax` (`DrawComplex_GLSL.cpp` detail loop, ini default 2, clamp 0..3) → set web ini `DetailMax=3` if parity is wanted; no code change |
| D11 | 1245-1246 (unchanged from v400) | `DrawTile`: `Palette[128].A!=255 && !PF_Translucent → PF_Highlighted` (alpha-palette tiles, premultiplied) | **XOpenGL never had this** (v400 gap too). HP2 HUD/menus use alpha-palette P8 textures far more than UT (menu fades, spell icons) — ACTION C5 |
| D12 | 1320-1336 | `Draw2DLine` depth-cue colour fade with `LINE_DepthCued` | editor-only → ignore |
| D13 | 1401-1577 | real `PushHit/PopHit` pixel hit-testing | editor-only; XOpenGL has its own `EditorHit.cpp` → ignore |
| D14 | 1780-1807 | `GetOsSurface` returns the back/front-buffer HDC | WinDrv-only → do not implement (§2.1) |
| D15 | 1993, 2006-2008 | texture hash `CacheID>>12`; unknown formats fall back to 8888 instead of `appErrorf` ("accidentally some weird format textures in UT") | XOpenGL already logs + placeholders unknown formats (see C6); UTGLR: §6. Verify by watching for `Unknown texture format` in the log during a playthrough. |
| D16 | 2991 | `ResizeViewport(… | BLIT_HardwarePaint)` | XOpenGL/UTGLR already pass `BLIT_HardwarePaint|BLIT_OpenGL` (`XOpenGL.cpp:1151,1236`) → no change |

### 5.3 Exact change list for branch `hp2-v433-compat` (file by file)

Gate macro (see 5.4): `XOGL_UE1_V4XX` = "vanilla UE1 v400-family engine without OldUnreal extensions" = `(ENGINE_VERSION==400 || ENGINE_VERSION==433)`, and `XOGL_HP2` = `(ENGINE_VERSION==433)` for the render-face-specific code.

**`Inc/XOpenGLDrv.h`**

| Line | Change |
|---|---|
| top, after `#include "Engine.h"` (line 25) | `#define XOGL_UE1_V4XX (ENGINE_VERSION==400 \|\| ENGINE_VERSION==433)` and `#define XOGL_HP2 (ENGINE_VERSION==433)` |
| 37 | add `#elif ENGINE_VERSION==433` / `#include "Render.h"` before the `==400` branch (X1) |
| 51 | `#if ENGINE_VERSION==400` → `#if XOGL_UE1_V4XX` (X3) |

**`Inc/XOpenGL.h`**

| Line | Change |
|---|---|
| 58 | add `\|\| ENGINE_VERSION==433` (X4) |
| 183 | split the block: TEXF aliases, `FIsCompressedFormat`, `FTextureFormatString`, `ERenderZTest` under `XOGL_UE1_V4XX`; the three alpha-blend flags stay `0` under `ENGINE_VERSION==400` only; add under `XOGL_HP2`: `#define PF_XOGL_AlphaBlend 0x20000000` (the one free `EPolyFlags` bit, §2.2) and `#define PF_AlphaBlend PF_XOGL_AlphaBlend`, `#define PF_Straight_AlphaBlend PF_XOGL_AlphaBlend`, `#define PF_Premultiplied_AlphaBlend 0` (X5). With `PF_AlphaBlend` real, the existing code paths light up with no further edits: `SetBlend` → `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` (`SetTexture.cpp:1007-1010`), `GetPolyFlagsAndDrawFlags` → `DF_AlphaBlended` (`:922-923`), Gouraud shader → `Color.a *= In.LightColor.a` (`DrawGouraud_GLSL.cpp:413-414`). DrawSimple's `BlendMode = PF_AlphaBlend` (`DrawSimple.cpp:66,127,168`) becomes a real alpha blend of `Color.W=1` → visually identical to v400's opaque path. |
| 425, 500 | `ENGINE_VERSION==400` → `XOGL_UE1_V4XX` (X6) |
| 523 | `#elif ENGINE_VERSION==430` → `#elif ENGINE_VERSION==430 \|\| ENGINE_VERSION==433` (X8, 4-arg `DECLARE_CLASS`) |
| 667 | `ENGINE_VERSION==400` → `XOGL_UE1_V4XX` (X7) |
| 1958 | `#if XOGL_HP2`: `void DrawComplexSurface(FSceneNode*, FSurfaceInfo&, FSurfaceFacet&, DWORD PolyFlags, BYTE cAlpha); INT MaxVertices(); void DrawTriangles(FSceneNode*, FTextureInfo&, FTransTexture**, INT NumPts, _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer*);` `#else` the 3-arg `DrawComplexSurface` `#endif` (C1, C2). Add a private helper `DWORD HP2AdjustPolyFlags(DWORD PolyFlags, UBOOL IsTile, FTextureInfo* Info)` (C3). |

**`Src/DrawComplex.cpp`** — C1 (`DrawComplexSurface`, lines 48-202)

```cpp
#if XOGL_HP2
void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD InPolyFlags, BYTE cAlpha)
#else
void UXOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
#endif
{
    ...
#if XOGL_HP2
    DWORD SurfPolyFlags = InPolyFlags & ~PF_XOGL_AlphaBlend;         // never trust content with the synthetic bit
    FLOAT LumosOpacity  = 1.f;
    if (!GIsEditor && (SurfPolyFlags & PF_HighShadowDetail) && (SurfPolyFlags & (PF_Modulated|PF_Translucent)))
    {   // §3.3: Lumos surface. cAlpha is transparency (255 = gone); the Occlude bit was already decided by Render.
        LumosOpacity  = (255 - cAlpha) / 255.f;
        SurfPolyFlags = (SurfPolyFlags & ~(PF_Modulated|PF_Translucent|PF_HighShadowDetail|PF_Gouraud|PF_DirtyShadows)) | PF_XOGL_AlphaBlend;
    }
    const DWORD NextPolyFlags = GetPolyFlagsAndDrawFlags(SurfPolyFlags, DrawFlags, FALSE);
#else
    const DWORD NextPolyFlags = GetPolyFlagsAndDrawFlags(Surface.PolyFlags, DrawFlags, FALSE);
#endif
```

…and after the diffuse `SetTextureHelper` (line 97): `DrawCallParams->DiffuseInfo.z *= LumosOpacity;` (the complex fragment shader already does `Color.a *= GetDiffuseInfo(vDrawID).z`, `DrawComplex_GLSL.cpp:404`; lightmap alpha is forced to 1 at `:449`, so the factor survives to `FragColor`). Every other `Surface.PolyFlags` use in the function must read `SurfPolyFlags` (today there is only the one at line 64). Note `PF_Gouraud` (= `PF_LumosAppear`) and `PF_DirtyShadows` (= `PF_LumosAlwaysOn`) are meaningless to the GL path and are stripped only inside the Lumos branch.

**`Src/DrawGouraud.cpp`** — C2 (`MaxVertices`, `DrawTriangles`)

```cpp
#if XOGL_HP2
INT UXOpenGLRenderDevice::MaxVertices() { return 256; }   // == retail D3D7/SoftDrv (ACTORPOLY_VERTEX_MAX); the Gouraud VBO holds
                                                          // DRAWGOURAUDPOLY_SIZE*12 = 12288 verts, so up to 2048 would fit 6*N indices.
void UXOpenGLRenderDevice::DrawTriangles(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts,
                                         _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* Span)
{
    guard(UXOpenGLRenderDevice::DrawTriangles);
    if (NoDrawGouraud || NumPts < 3) return;
    if (!Indices && NumIndices == NumPts) { DrawGouraudPolygon(Frame, Info, Pts, NumPts, PolyFlags, Span); return; } // fan (decals)
    PolyFlags = HP2AdjustPolyFlags(PolyFlags, FALSE, &Info);                   // C3
    const INT OutCount = Indices ? (NumIndices/3)*3 : (NumPts/3)*3;            // list
    // same buffering as DrawGouraudPolygon (lines 170-228): CanBuffer/Flush, PrepareGouraudCall, StartDrawCall,
    // then for (k<OutCount) BufferVert(Out++, Pts[Indices ? Indices[k] : k], DrawID); EndDrawCall(OutCount) …
    unguard;
}
#endif
```

and in `DrawGouraudPolygon` (line 160) add `PolyFlags = HP2AdjustPolyFlags(PolyFlags, FALSE, &Info);` under `XOGL_HP2` (it is now reached from `DrawTriangles` for decals). `BufferVert` (line 35) keeps copying `Normal`/`Fog`; to avoid feeding uninitialised particle normals into GLSL `normalize()`, under `XOGL_HP2` write `Normals = vec4(0)` when `!UseHWLighting && !BumpMaps` (the only consumers), and rely on `GetPolyFlagsAndDrawFlags` stripping `PF_RenderFog` (particles never set it, `UnParticleRn.cpp:361-371`).

**`Src/SetTexture.cpp`** — C3 (`HP2AdjustPolyFlags`, new, next to `GetPolyFlagsAndDrawFlags` line 897)

```cpp
#if XOGL_HP2
// 433 blend semantics that UT469/v400 never had (retail D3D7 SetBlending, Direct3D7.cpp:1829-1870; SoftDrv DrawPoly.cpp:4380-4385).
DWORD UXOpenGLRenderDevice::HP2AdjustPolyFlags(DWORD PolyFlags, UBOOL IsTile, FTextureInfo* Info)
{
    PolyFlags &= ~PF_XOGL_AlphaBlend;
    if ((PolyFlags & (PF_Highlighted|PF_Translucent)) == (PF_Highlighted|PF_Translucent))      // fading actor / vertex-alpha poly
        PolyFlags = (PolyFlags & PF_Masked) ? (PolyFlags & ~(PF_Highlighted|PF_Translucent))  // SoftDrv: masked wins
                                            : ((PolyFlags & ~(PF_Highlighted|PF_Translucent)) | PF_XOGL_AlphaBlend);
    if (IsTile && Info && Info->Palette && Info->Palette[128].A != 255 && !(PolyFlags & PF_Translucent))
        PolyFlags |= PF_Highlighted;                                                            // D11: alpha-palette tile, premultiplied
    if ((PolyFlags & (PF_Modulated|PF_RenderFog)) == (PF_Modulated|PF_RenderFog))
        PolyFlags &= ~PF_RenderFog;                                                             // §3.5 decals, = D3D7 AdjustPolyFlags
    return PolyFlags;
}
#endif
```

Mesh `PF_HighShadowDetail` is not a Lumos marker on `DrawTriangles` (Render only applies Lumos to BSP), so `HP2AdjustPolyFlags` intentionally leaves it alone; the `&`-quirk that makes retail D3D7 route every `PF_Modulated` triangle through the Lumos blend is **not** reproduced (evidence in §3.3 quirk 1).

**`Src/DrawTile.cpp`** — C5: at line 36 (before `GetPolyFlagsAndDrawFlags`) under `XOGL_HP2`: `PolyFlags = HP2AdjustPolyFlags(PolyFlags, TRUE, &Info);`. Premultiplied `PF_Highlighted` → `GL_ONE, GL_ONE_MINUS_SRC_ALPHA` already exists (`SetTexture.cpp:1011-1014`); the P8 upload already carries palette alpha (`SetTexture.cpp:572-579`, `GET_COLOR_DWORD(Palette[i])`). Because the texel colour is **not** premultiplied by the upload, verify one HUD icon against the retail screenshot; if edges glow, premultiply RGB by A in the P8 upload when `Palette[128].A!=255` (D3D7 does not premultiply either, so parity says "leave it").

**`Src/DrawSimple.cpp`** — C4: `Draw3DLine` (line 104-105): under `XOGL_HP2` wrap the two `TransformPointBy` calls in `if (!(LineFlags & LINE_PreTransformed))` (mirrors `Engine/Src/UnCamera.cpp:142-147`; no HP2 caller today, cheap insurance).

**`Src/SetTexture.cpp`** — C6 (D15): **probably no change.** The format switch already degrades an unknown format to a placeholder (`SetTexture.cpp:525-528`: `default: GWarn->Logf("Unknown texture format …"); UnsupportedTexture = true;`); the `appErrorf("Unpacking unknown format …")` at `:680` is only reachable for a format that passed the first switch. The one hard stop left is `:324-325` `appErrorf("Encountered bogus P8 texture")` for a P8 without palette — keep it (it also fatals on D3D7). Verify during the first playthrough that no `Unknown texture format` line appears in the log.

**`Src/XOpenGL.cpp`, `Src/DrawComplex.cpp:121`, `Src/DrawGouraud.cpp:75`, `Src/SetTexture.cpp:257,347,423,645,686`, `Src/CheckExtensions.cpp:294`, `Src/ShaderProgram.cpp:487,565,909`** — mechanical: every `#if ENGINE_VERSION==400` → `#if XOGL_UE1_V4XX` (X9-X17, X19-X21, X23-X27), and `DrawComplex.cpp:121` `ENGINE_VERSION==1100 || ENGINE_VERSION==400` → `ENGINE_VERSION==1100 || XOGL_UE1_V4XX`. The `(ENGINE_VERSION>=430 && ENGINE_VERSION<1100) || ENGINE_VERSION==400` sites (X18) need nothing.

**Two-sided depth bias (§3.5)** — C7, optional, behind a new config bool `HP2TwoSidedBias` (default on): in `DrawTriangles`, when `PolyFlags & PF_TwoSided`, flush and wrap the batch in `glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-1.f, -1.f);` (WebGL2 supports it). Decide after looking at Harry's robe/hair in the first level.

**Docs** — `Source/XOpenGLDrv/ENGINE_CHANGES.md` + `WEB_INTEGRATION.md` in the HP2 repo (byte-copies of the template's) must be rewritten for 433 once the branch lands: §4 of ENGINE_CHANGES lists the v400 adaptations — append the 433 face (C1-C7) and replace "gated `ENGINE_VERSION==400`" with "gated `XOGL_UE1_V4XX` / `XOGL_HP2`". The CMake comment block in `Source/XOpenGLDrv/CMakeLists.txt:3-8,22-24` still says "RenderPrivate.h/UnRender.h come from ../Render/Src" → correct it to "Render.h from Render/Inc (via the Render link)".

### 5.4 Gating scheme (why two macros, and why not `ENGINE_VERSION>=400 && <436`)

* `ENGINE_VERSION==433` alone would be the literal reading of the brief, but ~25 sites are "vanilla v4xx without OldUnreal" rather than "HP2": keeping them as `(==400 || ==433)` spelled out 25 times invites drift. One derived macro per meaning keeps the fork diff reviewable and lets the same branch still build the UT99 template (400) unchanged:
  * `XOGL_UE1_V4XX` — Core/TCHAR/TArray/SDL-context-adoption/gamma/WebGL fixes shared by 400 and 433.
  * `XOGL_HP2` — the 433 render-device face and the HP2 blend semantics (C1-C7).
* Range gates (`>=400 && <436`) are rejected: 430/432 builds (Unreal 224/225, DeusEx 1100 has its own gates) have different faces, and the fork already uses `==430` for Unreal 226 with a 4-arg `DECLARE_CLASS`.
* Verification that the v400 build is untouched: build the template's native preset against the new branch (outside `/home/ion/engines`, e.g. a scratch copy) and diff `XOpenGLDrv.so` symbols (`nm -C | grep UXOpenGLRenderDevice`) — the v400 symbol set must not gain `DrawTriangles`/`MaxVertices`.


## 6. UTGLR fork (`OpenGLDrv`): gates inventory, engine API surface, exact 433 change list, gating scheme

Tree: `/home/ion/hp2-ufrontend/ufrontend/utglr-copy` (to be moved to `/home/ion/hp2-ufrontend/utglr-copy`, §1.1), branch `hp2` @ `e60807b` (= `main`): `ac9e37b` Dohnal drop → `18de00e` "UT99 v400 + web adaptations (squashed)" → `8bca5fc` (gl4es proc resolver on a worker) → `e60807b` (MinGW). Flat layout: `OpenGL.cpp` (8170 lines), `OpenGL.h` (1590), `OpenGLDrv.{h,cpp}`, `OpenGL1Funcs.h`, `OpenGLExtFuncs.h`, `c_gclip.*`, `c_rbtree.h`. The HP2 glue `Source/OpenGLDrv/CMakeLists.txt` compiles `OpenGL.cpp OpenGLDrv.cpp c_gclip.cpp` with `UPACKAGE_NAME=OpenGLDrv UTGLR_UT_BUILD UTGLR_ENGINE_UT99DC UTGLR_ENGINE_HP2 __UNIX__` (+ `UTGLR_USE_GL4ES` when `USE_GL4ES`), i.e. **both** the v400 macro and the new HP2 macro are defined today.

### 6.1 The existing multi-build switch — and why HP2 is closer to *upstream* than to the v400 port

UTGLR upstream already supports several UE1 builds through a mandatory build-config macro (`OpenGL.h:10-29`, `#error Valid build config not selected`): `UTGLR_UT_BUILD` (UT **v436**), `UTGLR_DX_BUILD` (Deus Ex 1112f), `UTGLR_RUNE_BUILD` (Rune 107), `UTGLR_UNREAL_227_BUILD` (Unreal 227). There is **no v400 build and no 433 build** upstream; the ufront port added `UTGLR_ENGINE_UT99DC` on top of `UTGLR_UT_BUILD` to *undo* two v436 assumptions (4-arg `DECLARE_CLASS`, `FTime`) and to carry the SDL2/NSDLDrv platform work.

HP2 433 has **both** v436 features the v400 port had to undo (§2.4: 4-arg `DECLARE_CLASS`, `FTime` with `operator-(FTime)→float`). So for the *engine-type* sites HP2 wants the **upstream `UTGLR_UT_BUILD` branch**, while for the *platform* sites it wants everything `UTGLR_ENGINE_UT99DC` gives. `UTGLR_ENGINE_UT99DC` currently conflates the two meanings, and because the HP2 CMake defines it, **the fork as-is fails to compile on 433** at exactly the engine-type sites (U1-U3 below).

Reusable upstream machinery that 433 needs (no invention required):

| Upstream feature | Where | Reuse for HP2 |
|---|---|---|
| `PF_AlphaBlend` blend mode (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`, fog suppressed, tile checks) | Rune/227 branches: `SetBlend` `OpenGL.h:1280-1305`, `SetBlendNoCheck` `OpenGL.cpp:6304-6372`, gouraud fog masks `:4200-4204, 4355-4359`, tile `:4537-4545, 4619-4658, 4688-4715` | the vehicle for the 433 `PF_Highlighted|PF_Translucent` straight-alpha mode (§3.4) and for Lumos (§3.3), with a synthetic `PF_AlphaBlend = 0x20000000` (the free `EPolyFlags` bit, §2.2) |
| per-poly alpha into the colour array (`m_gpAlpha`, `FPlaneTo_RGB_Aub`) | Rune: `OpenGL.h:760`, `OpenGL.cpp:1085, 1574, 4394-4400` | pattern for per-vertex alpha; HP2 needs `FPlaneTo_RGBA(&P->Light)` (Light.W, §3.4) instead of `Texture->Alpha` |
| v436 `FTime m_prevFrameTimestamp` / `FTime curFrameTimestamp` | `OpenGL.h:640-646`, `OpenGL.cpp:3703-3707` | compiles against HP2 `FTime` unchanged (`appSeconds()` returns `FTime`, `FTime-FTime→float`, `appSleep(FLOAT)` exists, `Core/Inc/UnFile.h:264`) |
| v436 4-arg `DECLARE_CLASS(UOpenGLRenderDevice, URenderDevice, CLASS_Config, OpenGLDrv)` | `OpenGL.h:362-368` `#else` | exactly HP2's macro (`Core/Inc/UnObjBas.h:326`) |

### 6.2 Every gate in the fork and what it means on 433

| # | Site | Gate | What it does | 433 verdict / ACTION |
|---|---|---|---|---|
| U1 | `OpenGL.h:362-368` | `UTGLR_DX_BUILD \|\| UTGLR_ENGINE_UT99DC` | 3-arg `DECLARE_CLASS` | **Wrong on 433** → must take the upstream 4-arg `#else`. |
| U2 | `OpenGL.h:640-646` | `DX \|\| RUNE \|\| UT99DC` | `FLOAT m_prevFrameTimestamp` | **Wrong on 433** → upstream `FTime`. |
| U3 | `OpenGL.cpp:3703-3707` | `DX \|\| RUNE \|\| UT99DC` | `FLOAT curFrameTimestamp = appSeconds();` | **Compile error on 433** (`FTime` has no conversion to `float`) → upstream `FTime`. Only reached when `FrameRateLimit>=20`, but it must compile. |
| U4 | `OpenGLDrv.h:24-26` | `!WIN32 \|\| UT99DC` | `#include "SDL2/SDL.h"` | keep (platform) |
| U5 | `OpenGLDrv.h:39-47` | `WIN32 && !UT99DC` | `UTGLR_USE_WGL` vs `UTGLR_USE_SDL` | keep (platform) — SDL on all HP2 targets |
| U6 | `OpenGLDrv.h:47-65` | `UT99DC` | SDL 1.2 → SDL2 shims (`SDL_GL_SwapBuffers`, `SDL_SetGammaRamp`, `SDL_ListModes`, `SDL_FULLSCREEN`) | keep (platform). Note: gamma ramp is a no-op → `Brightness` has no effect with UTGLR on HP2 (same as UT99 web). |
| U7 | `OpenGL.h:36` | `!__UNIX__` | `<tchar.h>` | keep |
| U8 | `OpenGL.h:57` | `_WIN32 && _MSC_VER` | x86 asm / SSE intrinsics | keep (not MSVC → C fallbacks) |
| U9 | `OpenGL.cpp:2239-2247` | `UT99DC` | `UseS3TC` read straight from ini, default on | keep — HP2 retail content: verify how many `TEXF_DXT1` textures load (§11). |
| U10 | `OpenGL.cpp:2921-2928` | `__EMSCRIPTEN__` | `SupportsFogMaps = 1` on the zero-init web instance | keep — HP2 zone fog uses the same `VolumetricLighting && SupportsFogMaps` gate (verify: `grep -n SupportsFogMaps Source/Render/Src/*.cpp`). |
| U11 | `OpenGL.cpp:2940-2962` | `UT99DC` / `UTGLR_USE_GL4ES` / `__EMSCRIPTEN_PTHREADS__` | skip `SDL_GL_LoadLibrary`; gl4es `setenv(LIBGL_ES=2, NOBANNER, USEVBO=0)`, `set_getprocaddress(emscripten_webgl_get_proc_address \| SDL_GL_GetProcAddress)`, `initialize_gl4es()` | keep (platform; NSDLViewport owns the context on HP2 too, §8) |
| U12 | `OpenGL.cpp:1746-1756, 1802-1808` | `UTGLR_USE_GL4ES` / `UTGLR_USE_SDL` | GL proc resolution through `gl4es_GetProcAddress` | keep |
| U13 | `OpenGL.cpp:66-74` | `UTGLR_USE_GL4ES`, `__EMSCRIPTEN__ && __EMSCRIPTEN_PTHREADS__` | includes `gl4esinit.h`, `emscripten/html5_webgl.h` | keep |
| U14 | `OpenGL.cpp:3632-3640` | `UT99DC` | 60-frame `Unlock` heartbeat log | keep (debug aid; harmless) |
| U15 | `OpenGL.cpp:4415-4421, 4445-4450, 4750-4754` | `__EMSCRIPTEN__` | tiles with `Z==1` drawn with depth test off | keep. HP2's `UCanvas` draws HUD/menu tiles at `Z=1` like v400 (verify `grep -n 'DrawTile' Source/Engine/Src/UnCanvas.cpp`). |
| U16 | many | `UTGLR_RUNE_BUILD`, `UTGLR_UNREAL_227_BUILD`, `UTGLR_DX_BUILD` | other games | not defined → UT branch. **Note the idiom `#elif UTGLR_UNREAL_227_BUILD`** (`:4202, 4357, 4539, 4623, 4652, 4690, 4711, 6306, 6333`): it is a value test, so any HP2 addition must be written `#elif defined(UTGLR_UNREAL_227_BUILD) \|\| defined(UTGLR_ENGINE_HP2)` (or better, a new feature macro — §6.4). |
| U17 | `OpenGL.cpp:5708`, `6068-6080` | `UTGLR_UNREAL_227_BUILD` | `Info.UClampMode`, `TEXF_DXT3/5` | stay off (HP2 `FTextureInfo` has no clamp modes; `ETextureFormat` has no DXT3/5, §2.2) |
| U18 | `OpenGL.cpp:4148, 4322` | (none) `GUglyHackFlags & 0x1` | near-Z range hack for the first-person overlay | works on 433: `Render/Src/UnRender.cpp:3902-3904` sets/clears bit 1 around `eventRenderOverlays` exactly as v400 (`UnRender.cpp:3407-3409`). The new `DrawTriangles` must preserve it (the wand/overlay meshes go through `DrawTriangles` on 433). |

### 6.3 Engine API surface UTGLR touches (extracted with `grep -oh` over `OpenGL.cpp`)

| Symbol | Members used | 433 status |
|---|---|---|
| `URenderDevice` overrides (`OpenGL.h:1060-1141`) | `StaticConstructor, Exit, ShutdownAfterError, SetRes, Init, Exec, Lock, SetSceneNode, Unlock, Flush, DrawComplexSurface(3-arg), DrawGouraudPolygon, DrawTile, Draw3DLine, Draw2DLine, Draw2DPoint, ClearZ, PushHit, PopHit, GetStats, ReadPixels, EndFlash, PrecacheTexture` (+ own `DrawGouraudPolygonOld`) | identical signatures to HP2 `UnRenDev.h` **except `DrawComplexSurface`**; **`MaxVertices` and `DrawTriangles` missing → class abstract on 433**. No `DrawStats` override (base no-op — fine). `GetOsSurface`: do not override. |
| `URenderDevice` fields | `SpanBased, FullscreenOnly, SupportsFogMaps, SupportsDistanceFog, SupportsTC, Coronas, ShinySurfaces, HighDetailActors, VolumetricLighting, DetailTextures, PrefersDeferredLoad, SupportsLazyTextures, Viewport` | identical (`UnRenDev.h:41-60`) |
| `UViewport` | `SizeX, SizeY, ColorBytes, HitX/HitY/HitXL/HitYL, IsOrtho(), GetWindow(), ResizeViewport(), GetOuterUClient()->Brightness, Actor->{FovAngle,OrthoZoom}` | identical (§2.3); UTGLR never reads `CurrentTime` |
| `FSceneNode` | `Viewport, X, Y, XB, YB, FX, FY, FX2, FY2, Zoom, Coords, RProj.Z` | identical |
| `FSurfaceInfo` | `PolyFlags` (**one read, `OpenGL.cpp:3906`**), `Texture, LightMap, MacroTexture, DetailTexture, FogMap, FlatColor` | identical; the one `Surface.PolyFlags` read must become the 5-arg `PolyFlags` argument |
| `FSurfaceFacet` / `FSavedPoly` / `FTransform` | `MapCoords.{Origin,XAxis,YAxis}`, `Polys`, `Poly->{Next,NumPts,Pts[]->Point}` | identical |
| `FTransTexture` | `Point, Normal (only with CF_NORMAL_ARRAY, i.e. never on the UT path), U, V, Light (X/Y/Z only; W never), Fog (X/Y/Z/W)` | identical layout. **Fog is read whenever `(PolyFlags & (PF_RenderFog\|PF_Translucent\|PF_Modulated)) == PF_RenderFog`** — particles never set `PF_RenderFog` (§3.2), decals zero `Fog` → safe. 433 needs `Light.W` read for the alpha mode (§6.4 H4). |
| `FTextureInfo` | `Texture, CacheID, Format, Palette (incl. [0].A, [128].A), NumMips, Mips[]->{DataPtr,USize,VSize,UBits,VBits}, USize/VSize, UClamp/VClamp, UScale/VScale, bRealtimeChanged, Load(), Unload()` (`UClampMode/VClampMode` 227-only) | identical |
| `ETextureFormat` | `TEXF_DXT1` explicitly; `Palette!=NULL` → P8; **everything else is treated as `TEXF_RGBA7` (BGRA7777 ×2)** (`OpenGL.cpp:6052-6118`) | HP2 values unchanged (§2.2). `TEXF_RGB8/RGBA8` would be mis-read (×2 overflow) — **the retail D3D7 driver does exactly the same** (`Direct3D7.cpp:2006-2008`, "weird format textures" fallback to the RGBA7 filler). The only runtime `TEXF_RGBA8` producers in 433 are `UTexture::LoadFromSnap/LoadFromBMP` (`Engine/Src/UnTex.cpp:1042,1075`) reached from natives 321/322 `CreateTextureFromBMP/CreateTextureFromScreenShot` (`Engine/Src/UnScript.cpp:2688-2725`); **no UnrealScript in `Source/*/Classes` calls either** (`grep -rln CreateTextureFrom Source --include=*.uc` → only the declaration in `Engine/Classes/Actor.uc:1652-1653`). → no change; if a later phase finds a caller in a shipped `.u` not in `Source/`, add a `TEXF_RGBA8` case (`GL_BGRA` 8888, no ×2) under `UTGLR_ENGINE_HP2`. |
| `UTexture` | `Alpha` (Rune only), `Info.Texture` as cache owner | fine |
| Globals | `GIsEditor, GConfig, GUglyHackFlags, GUnicodeOS, GSecondsPerCycle, GMem, appSeconds, appSleep, appRound, appFromAnsi, appSprintf, debugf, LocalizeError` | all present in HP2 Core (`Core.h:437,445,460`, `UnFile.h:264`, `UnGnuG.h:263`) |
| Property registration | `UBoolProperty/UIntProperty/UFloatProperty(CPP_PROPERTY(x), TEXT("Options"), CPF_Config)` via `SC_Add*ConfigParam` (`OpenGL.cpp:802-850`) | HP2 `UnType.h:42-205` ctors `(ECppProperty, INT, const TCHAR*, DWORD)` = v400 |
| `LINE_*` | `LINE_None` only; `Draw3DLine` always `TransformPointBy(Frame->Coords)` (`OpenGL.cpp:4770-4771`) | add the `LINE_PreTransformed` skip (§6.4 H7) |

### 6.4 What the retail D3D7 delta (§5.2) means for UTGLR specifically

| D# (§5.2) | UTGLR today | 433 action |
|---|---|---|
| D1 `DrawComplexSurface(…,PolyFlags,cAlpha)` | 3-arg, reads `Surface.PolyFlags` (`:3906`) | H1 |
| D2 `MaxVertices` / `DrawTriangles` | absent | H2 |
| D3 modulated polys/tiles use white | already: `glColor3f(1,1,1)` for `PF_Modulated` (`:4163-4166, 4368-4370, 4702-4703`); tiles skip the colour array (`:4603`) | none |
| D6 `PF_Highlighted` gets no `PF_Occlude` | `SetBlend` (`OpenGL.h:1290-1292`) already excludes `PF_Highlighted`, **but `DrawTile` forces `PF_Highlighted \| PF_Occlude` for alpha-palette tiles** (`OpenGL.cpp:4544, 4695`) | H5: under `UTGLR_ENGINE_HP2` add only `PF_Highlighted` (D3D7 433 `Direct3D7.cpp:1245-1246` + `AdjustPolyFlags :1815-1826` → no depth write). Matters for alpha-palette sprites in the 3D world (drawn through `DrawTile` by `UnSprite.cpp`): with `PF_Occlude` their transparent texels write depth and cut holes in whatever is drawn later. |
| D7 Lumos blend | absent | H1 |
| D8 `Highlighted\|Translucent` → straight alpha | `SetBlendNoCheck` tests `PF_Translucent` first (`:6320-6328`) → fading actors would render **additive** | H3/H4 |
| D9 lightmap ×2 | `SetTexEnvNoCheck` `GL_RGB_SCALE_EXT 2.0` on units ≠0 unless `OneXBlending` (`:6461-6477`); UT default `OneXBlending=0` (`:784-788`) | none (keep `OneXBlending=False` in the web ini) |
| D10 detail textures `DetailMax=3` | UTGLR draws one detail layer | none (cosmetic) |
| D11 alpha-palette tiles | already present (`:4538-4545, 4689-4696`) | only the `PF_Occlude` part (H5) |
| D15 unknown formats → RGBA7 | same fallback (`:6100-6118`) | none |
| §3.5 `PF_RenderFog` on modulated decals | fog pass only when `(PolyFlags & (RenderFog\|Translucent\|Modulated)) == RenderFog` (`:4200-4205, 4355-4361`) → modulated never fogged | none |
| §3.5 two-sided depth bias | depth from `Point.Z` | H6 (optional) |

### 6.5 Exact change list for branch `hp2` (file by file)

Macro scheme (see 6.6): keep `UTGLR_ENGINE_UT99DC` = "ufront platform port (SDL2/NSDLDrv/gl4es/web)"; add, at the top of `OpenGL.h` (after the build-config check, line 29):

```cpp
// ufront: engine-face selection. UT99DC = the ufront platform layer (SDL2/NSDLDrv/gl4es/web), shared by v400 and HP2.
// V400 = the ut99dc engine types (3-arg DECLARE_CLASS, no FTime). HP2 = UE1 build 433 (v436-style types + the HP2 render face).
#if defined(UTGLR_ENGINE_UT99DC) && !defined(UTGLR_ENGINE_HP2)
#define UTGLR_ENGINE_V400 1
#endif
#if defined(UTGLR_RUNE_BUILD) || defined(UTGLR_UNREAL_227_BUILD) || defined(UTGLR_ENGINE_HP2)
#define UTGLR_HAS_ALPHABLEND 1
#endif
#if defined(UTGLR_ENGINE_HP2)
#define PF_AlphaBlend 0x20000000   // synthetic: free EPolyFlags bit in 433 (Engine/Inc/UnObj.h:221-281); never set by content
#endif
```

**`OpenGL.h`**

| Line | Change |
|---|---|
| 362 | `#if defined UTGLR_DX_BUILD \|\| defined UTGLR_ENGINE_UT99DC` → `#if defined UTGLR_DX_BUILD \|\| defined UTGLR_ENGINE_V400` (U1 → 4-arg on HP2) |
| 640 | `… \|\| defined UTGLR_ENGINE_UT99DC` → `… \|\| defined UTGLR_ENGINE_V400` (U2) |
| 760 | make `BYTE m_gpAlpha;` unconditional-safe (it already is unconditional) — add `FLOAT m_hp2SurfOpacity;` and `bool m_hp2VertexAlpha;` under `UTGLR_ENGINE_HP2` |
| 1120 | `#ifdef UTGLR_ENGINE_HP2`: `void DrawComplexSurface(FSceneNode*, FSurfaceInfo&, FSurfaceFacet&, DWORD PolyFlags, BYTE cAlpha); INT MaxVertices(); void DrawTriangles(FSceneNode*, FTextureInfo&, FTransTexture**, INT, _WORD*, INT, DWORD, FSpanBuffer*); DWORD HP2AdjustPolyFlags(DWORD);` `#else` the 3-arg declaration `#endif` |
| 1280-1305 (`SetBlend`) | Rune's `PF_AlphaBlend` pre-block stays Rune-only (it *adds* `PF_Occlude`; D3D7 433 does not). Add `#elif defined(UTGLR_ENGINE_HP2)`: `if (PolyFlags & PF_AlphaBlend) PolyFlags &= ~PF_Masked; else` before the common test, and change the common test's mask to `(PF_Translucent\|PF_Modulated\|PF_Highlighted\|PF_AlphaBlend)` under HP2 so alpha-blended polys do **not** write depth; `blendFlags` mask: use the 227 line (`… \| PF_AlphaBlend`) when `UTGLR_HAS_ALPHABLEND`. |

**`OpenGL.cpp`**

H0 (U3) — `:3703` `… \|\| defined UTGLR_ENGINE_UT99DC` → `… \|\| defined UTGLR_ENGINE_V400`.

H1 — `DrawComplexSurface` (`:3807`, and `RenderPassesNoCheckSetup :7205-7211`, `RenderPassesNoCheckSetup_FP :7300-7305`):

```cpp
#ifdef UTGLR_ENGINE_HP2
void UOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD InPolyFlags, BYTE cAlpha) {
#else
void UOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet) {
#endif
...
#ifdef UTGLR_ENGINE_HP2
	DWORD PolyFlags = InPolyFlags & ~PF_AlphaBlend;
	m_hp2SurfOpacity = 1.f;
	if (!GIsEditor && (PolyFlags & PF_HighShadowDetail) && (PolyFlags & (PF_Modulated | PF_Translucent))) {   // §3.3 Lumos
		m_hp2SurfOpacity = (255 - cAlpha) / 255.f;                    // cAlpha = transparency, 255 = gone
		PolyFlags = (PolyFlags & ~(PF_Modulated | PF_Translucent | PF_HighShadowDetail | PF_Gouraud | PF_DirtyShadows)) | PF_AlphaBlend;
		drawDetailTexture = drawDetailTexture && (cAlpha == 0);     // a separate modulated detail pass would darken what shows through
	}
#else
	DWORD PolyFlags = Surface.PolyFlags;                            // line 3906
#endif
```

and in both `RenderPassesNoCheckSetup*`: after `glColor3fv/glColor4fv(m_complexSurfaceColor3f_1f)`, under HP2 `if (MultiPass.TMU[0].PolyFlags & PF_AlphaBlend) glColor4f(m_complexSurfaceColor3f_1f[0], [1], [2], m_hp2SurfOpacity);`. Alpha then reaches the blender as `tex0.a × opacity × (lightmap.a if the lightmap is on TMU1)` because the modulate-2× combine on units ≠0 also modulates alpha (`SetTexEnvNoCheck :6464-6474`, `GL_COMBINE_ALPHA_EXT = GL_MODULATE`). **Lightmap alpha is not guaranteed to be 255** (UTGLR uploads RGBA7 lightmaps ×2 including A, `:6217-6238`). Two acceptable fixes — pick one after a test (§11 Q6): (a) under HP2 set `GL_COMBINE_ALPHA_EXT = GL_REPLACE, GL_SOURCE0_ALPHA_EXT = GL_PREVIOUS_EXT` for units ≠0 in the modulate branch (alpha on the units after the base is unused by every other UT blend mode, so this is safe globally), or (b) use constant-alpha blending for Lumos (`glBlendColor(0,0,0,opacity)` + `glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA)`; needs `GL1_PROC(void,glBlendColor,(GLfloat,GLfloat,GLfloat,GLfloat))` added to `OpenGL1Funcs.h` under `UTGLR_ENGINE_HP2`; ES2/WebGL1 and gl4es (`src/gl/blend.c`) have it). **Recommendation: (a)** — no new GL entry point, exact for texA=1 content. Depth writes follow the passed `PF_Occlude` (Render already decided it); `SetBlend` must not re-add it for `PF_AlphaBlend` (see OpenGL.h 1280-1305 row).

H2 — `MaxVertices` / `DrawTriangles` (new, next to `DrawGouraudPolygon :4233`):

```cpp
#ifdef UTGLR_ENGINE_HP2
INT UOpenGLRenderDevice::MaxVertices() { return 256; }   // = retail D3D7 / SoftDrv. VERTEX_ARRAY_SIZE (4000) is per flush, not per call.

void UOpenGLRenderDevice::DrawTriangles(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts,
                                        _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* Span) {
	guard(UOpenGLRenderDevice::DrawTriangles);
	if (NumPts < 3) return;
	PolyFlags = HP2AdjustPolyFlags(PolyFlags);
	if (!Indices && NumIndices == NumPts) {                     // one convex polygon (decals, base DrawGouraudPolygon)
		DrawGouraudPolygon(Frame, Info, Pts, NumPts, PolyFlags, Span);
		return;
	}
	const INT NumTris = (Indices ? NumIndices : NumPts) / 3;
	FTransTexture* Tri[3];
	for (INT t = 0; t < NumTris; t++) {
		for (INT k = 0; k < 3; k++) Tri[k] = Pts[Indices ? Indices[t*3+k] : t*3+k];
		DrawGouraudPolygon(Frame, Info, Tri, 3, PolyFlags, Span);   // 3 <= m_bufferActorTrisCutoff → buffered path, batched by (PolyFlags, CacheID)
	}
	unguard;
}
#endif
```

This reuses the buffered triangle path unchanged (hit-select, near-Z range hack via `GUglyHackFlags`, masked-texture cache ids, fog). Per-triangle state checks are what UT v436 meshes already cost (UT's mesh renderer calls `DrawGouraudPolygon` per triangle). If profiling on the web shows it hot, a later optimisation can buffer the whole index list in one `m_pBuffer3VertsProc` loop.

H3 — `HP2AdjustPolyFlags` (new; same semantics as XOpenGL C3, §5.3):

```cpp
DWORD UOpenGLRenderDevice::HP2AdjustPolyFlags(DWORD PolyFlags) {
	PolyFlags &= ~PF_AlphaBlend;
	if ((PolyFlags & (PF_Highlighted | PF_Translucent)) == (PF_Highlighted | PF_Translucent))
		PolyFlags = (PolyFlags & PF_Masked) ? (PolyFlags & ~(PF_Highlighted | PF_Translucent))
		                                    : ((PolyFlags & ~(PF_Highlighted | PF_Translucent)) | PF_AlphaBlend);
	return PolyFlags;
}
```

H4 — per-vertex alpha. In the buffered `DrawGouraudPolygon` state block (`:4380-4400`), under HP2: `m_hp2VertexAlpha = (PolyFlags & PF_AlphaBlend) != 0; if (m_hp2VertexAlpha) m_pBuffer3VertsProc = Buffer3Verts;` and in `Buffer3Verts` (`:1083-1088`) and `BufferAdditionalClippedVerts` (`:1572-1577`) add `#elif defined(UTGLR_ENGINE_HP2)`: `color = pRD->m_hp2VertexAlpha ? FPlaneTo_RGBA(&P->Light) : FPlaneTo_RGB_A255(&P->Light);` (and the same in `DrawGouraudPolygonOld :4178-4183` for >10-vertex polys). The fog masks at `:4200-4204` and `:4355-4359` take the 227 form (`… \| PF_AlphaBlend`) under `UTGLR_HAS_ALPHABLEND`, so alpha-blended polys never get the additive fog pass. `SetBlendNoCheck` (`:6304-6340`): the 227 `GL_BLEND_FLAG_BITS` and the `else if (blendFlags & PF_AlphaBlend) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` branch must be compiled for HP2 (rewrite the three `#elif UTGLR_UNREAL_227_BUILD` there as `#elif defined(UTGLR_HAS_ALPHABLEND)` — Rune keeps its own first branch). Leave the 227 alpha-test block at `:6343-6357` 227-only (HP2 has no masked-alpha-blend content).

H5 — `DrawTile` (`:4537-4545`, `:4688-4696`): under HP2 `PolyFlags |= PF_Highlighted;` (no `PF_Occlude`).

H6 — optional two-sided bias (§3.5): in `DrawTriangles`, if `PolyFlags & PF_TwoSided`, `EndGouraudPolygonBuffering(); glEnable(GL_POLYGON_OFFSET_FILL);` around the batch and disable after (`glPolygonOffset(-1,-1)` is already the default state, `:2389`, and the ZTrick path re-sets it per frame, `:3208-3224`). Same decision rule as XOpenGL C7.

H7 — `Draw3DLine` (`:4770-4771`): under HP2 `if (!(LineFlags & LINE_PreTransformed)) { P1 = …; P2 = …; }`.

**`OpenGLDrv.h`** — no change (includes `Engine.h` + `UnRender.h` only; UTGLR never needed `RenderPrivate.h`, so the §1.6 include problem does not exist here). The comment block at `OpenGLDrv.h:55-60` should mention that the shims are platform (shared with HP2).

**HP2 repo glue** — `Source/OpenGLDrv/CMakeLists.txt:34-44` already defines `UTGLR_ENGINE_HP2` next to `UTGLR_ENGINE_UT99DC` (§1.5). Keep both (UT99DC = platform layer). Its private include `${CMAKE_CURRENT_SOURCE_DIR}/../Render/Src` (line 31) is unused by UTGLR — harmless. `Source/OpenGLDrv/{PORTING,WEB-INTEGRATION}.md` are byte copies of the template's; append an "HP2 / 433" section listing U1-U3 and H1-H7 when the branch lands.

### 6.6 Gating scheme for UTGLR

* `UTGLR_ENGINE_UT99DC` — **platform** (SDL2 shims, no WGL, NSDLDrv-owned context, gl4es, web tweaks, S3TC ini read, heartbeat). Defined by both the v400 template and HP2.
* `UTGLR_ENGINE_V400` — derived: ut99dc engine *types* (3-arg `DECLARE_CLASS`, no `FTime`). Replaces `UTGLR_ENGINE_UT99DC` at exactly three sites (U1-U3). Not defined for HP2 → HP2 takes the upstream v436 branches.
* `UTGLR_ENGINE_HP2` — the 433 render face and blend semantics (H1-H7).
* `UTGLR_HAS_ALPHABLEND` — derived feature macro (Rune ∪ 227 ∪ HP2) replacing the value-tested `#elif UTGLR_UNREAL_227_BUILD` at the blend/fog-mask sites, so HP2 reuses upstream's alpha-blend plumbing instead of duplicating it.
* This keeps the v400 template's build bit-identical (it defines `UT99DC` only → `V400` on, `HP2`/`HAS_ALPHABLEND` off). Verify the same way as XOpenGL (§5.4): build the template's native preset against the new branch in a scratch copy and compare `nm -C OpenGLDrv.so | grep UOpenGLRenderDevice`.

## 7. gl4es: is any change needed for HP2? — No

Tree: `/home/ion/hp2-ufrontend/ufrontend/gl4es` (to be moved, §1.1), `hp2` @ `98441ed5` == `fix/cubemap-bind-active-unit` (origin `github.com/10-p/gl4es`). The fork = upstream `ptitSeb/gl4es` up to `e6bb082b`/`17f0894e` + 5 ufront commits: `627a77e4` cube map bound to the wrong unit, `8ab08207`/`414db130`/`94fa3eff` per-fragment reflection-map texgen (UT2004's GL4ESDrv needs), `bb07c829` depth24→16 renderbuffer downgrade, `98441ed5` MinGW build.

Why nothing changes:

| Question | Answer (evidence) |
|---|---|
| Does gl4es see engine types? | No. gl4es is a GL→GLES2 translator; it only sees the GL calls UTGLR makes. The 433 face is absorbed entirely inside UTGLR (§6). XOpenGLDrv does not use gl4es at all (WebGL2 native). |
| Do the HP2 additions to UTGLR use GL features the UT99 path did not? | H1-H7 use: `GL_SRC_ALPHA/GL_ONE_MINUS_SRC_ALPHA` blending (already used by UTGLR for `PF_Masked` + smooth-masked, `OpenGL.cpp:6338-6340`), a 4-component colour array with real alpha (`glColorPointer(4, GL_UNSIGNED_BYTE…)` already, `:2437, 6639`), `glColor4f` (already, `:4007-4019`), `GL_COMBINE_ALPHA_EXT = GL_REPLACE / GL_SOURCE0_ALPHA_EXT = GL_PREVIOUS_EXT` (gl4es emulates `GL_COMBINE` including the alpha combiner in its fixed-function shader generator, `src/gl/texenv.c:117,675,756`; UTGLR already sets `GL_COMBINE_ALPHA_EXT = GL_REPLACE` for `PF_Memorized`/`PF_Highlighted` units, `:6486, 6504`), optional `GL_POLYGON_OFFSET_FILL` (already used for detail textures, `:7679, 7858`), optional `glBlendColor` (only in the non-recommended Lumos variant (b); gl4es implements it, `src/gl/blend.c:50-82`). |
| Different texture formats? | No: HP2 has the same `ETextureFormat` as v400 (§2.2); P8→RGBA8, RGBA7→BGRA8888 and DXT1 paths are the ones UT99 already exercises through gl4es (`LIBGL` S3TC passthrough when the browser has `WEBGL_compressed_texture_s3tc`). |
| Different context setup? | No: same NSDLViewport, same ES2 context request (§8), same `LIBGL_ES=2 LIBGL_USEVBO=0` env (`OpenGL.cpp:2946-2961`). |

**ACTION:** none in the fork. Resolve the branch-name mismatch of §1 fact 1.3 (either delete the local `hp2` branch and keep `GL4ES_GIT_TAG=fix/cubemap-bind-active-unit`, or push `hp2` and point `Source/CMakeLists.txt:313` at it). Only revisit gl4es if the first UTGLR web run shows a translation bug specific to HP2 content (symptom → check with `LIBGL_LOGSHADERERROR=1` and the gl4es debug log).

## 8. NSDLDrv: GL / GLES / software selection, web context creation, and the 433 `UViewport` delta

HP2's `Source/NSDLDrv/{Inc/NSDLDrv.h, Src/NSDLClient.cpp, Src/NSDLViewport.cpp, Src/NSDLDrv.cpp, CMakeLists.txt}` are byte-identical to the template's (§1 fact 1.4). Line numbers below are valid for both trees.

### 8.1 How the viewport decides what surface to create (`UNSDLViewport::OpenWindow`, `NSDLViewport.cpp:309-660`)

| Step | Code | Effect |
|---|---|---|
| 1. Resolve the device name *before* any device exists | `:360-385`: `Parse(appCmdLine(),"GAMERENDERDEVICE=")`, else `GConfig->GetString("Engine.Engine","GameRenderDevice")`; `appStrupr`; if it lacks `"OPENGL"`, the same for `WindowedRenderDevice` | the only input is the **class-path string**; the device class is not loaded yet |
| 2. GL or not | `DoOpenGL = 1` iff the upper-cased name contains `"OPENGL"` (`:366-378`); skipped for `Temporary`, `GIsEditor`, `-nohard` | `XOpenGLDrv.XOpenGLRenderDevice` ✔, `OpenGLDrv.OpenGLRenderDevice` ✔, `SoftDrv.SoftwareRenderDevice` ✘, **`D3DDrv.D3DRenderDevice` ✘** (HP2 `Default.ini:22`) |
| 3. Native profile | `GLProfile` starts `COMPATIBILITY` (`:317`); the `"GLES"` test (`:379-380`) also sets COMPATIBILITY (a no-op left from ut99dc's NOpenGLESDrv) | native always gets a desktop compat context — UTGLR/gl4es and XOpenGL (which then asks for 3.3 through its own `SelectGLVersion` short-circuit, XOpenGL X10) both run on it |
| 4. Web GLES major | `WebGLESMajor = 2`, `= 3` iff the name contains `"XOPENGL"` (`:322, 383-384`) | XOpenGL → WebGL 2 (GLES3); UTGLR via gl4es → WebGL 1 (GLES2) |
| 5. Window flags | `SDL_WINDOW_HIDDEN \| SDL_WINDOW_RESIZABLE`; `\| SDL_WINDOW_OPENGL` only when `DoOpenGL` and **not** a pthreads web build (`:420-429`) | on the threaded web build SDL owns window + input only |
| 6. Context | native / non-threaded web: `SDL_GL_CreateContext` + `SDL_GL_MakeCurrent` (`:543-555`). **Threaded web (`__EMSCRIPTEN_PTHREADS__`)**: `emscripten_webgl_create_context("#canvas", Attrs)` on the worker with `majorVersion = WebGLESMajor-1`, `alpha=0, depth=1, stencil=1, antialias=0, explicitSwapControl=1, renderViaOffscreenBackBuffer=1`, made current, handle stashed in `GLCtx` (`:500-542`) | the device **adopts** the current context: XOpenGL via `SDL_GL_GetCurrentContext()`/`emscripten_webgl_get_current_context()` (X11), UTGLR by resolving procs through gl4es → `emscripten_webgl_get_proc_address` (U11). Neither creates or destroys a context (X17). |
| 7. Software surface | no GL: threaded web → own ARGB8888 framebuffer, blitted to the transferred OffscreenCanvas through a 2D context (`:557-575`); native → `SDL_CreateRenderer` + streaming texture (`:577-600`) | SoftDrv path; also what a null device gets |
| 8. Present | `Repaint`/`Unlock`: `emscripten_webgl_commit_frame()` (threaded web) or `SDL_GL_SwapWindow` (`:880-900`) | drivers must not swap (XOpenGL X13; UTGLR's `SDL_GL_SwapBuffers` is a no-op shim, U6) |
| 9. Device creation | `Client->TryRenderDevice(this, "ini:Engine.Engine.GameRenderDevice", StartupFullscreen)`, then `WindowedRenderDevice`, (`SoftDrv` for temporary viewports) (`:651-657`) → `UNSDLClient::TryRenderDevice` (`NSDLClient.cpp:425-468`): `StaticLoadClass(URenderDevice, …, LOAD_NoWarn)`, `ConstructObject`, `RenDev->Init(Viewport, SizeX, SizeY, ColorBytes, Fullscreen)`, `Level->DetailChange(HighDetailActors)` | a missing class (e.g. `D3DDrv`) fails silently and falls through to `WindowedRenderDevice` |

**Consequences for HP2 (ACTION):**

1. The shipped `Config/System/Default.ini:22,31` (`GameRenderDevice=D3DDrv.D3DRenderDevice`, `WindowedRenderDevice=SoftDrv.SoftwareRenderDevice`, `ViewportManager=WinDrv.WindowsClient` `:35`) would give **SoftDrv with no GL window** under NSDLDrv. The web/native boot inis (`Config/System/web/` currently holds only the template's `README-ut99.md` — no inis yet) must set `ViewportManager=NSDLDrv.NSDLClient` and **both** `GameRenderDevice` and `WindowedRenderDevice` to the chosen GL device, because step 1 reads `WindowedRenderDevice` only when `GameRenderDevice` lacks "OPENGL", and step 9 falls through to `WindowedRenderDevice` when the game device fails to `Init` — a GL window with SoftDrv in it breaks on the threaded web build (a canvas can have only one context type, `:896-900`).
2. Keep the device names: `XOpenGLDrv.XOpenGLRenderDevice` (contains `XOPENGL` → WebGL2) and `OpenGLDrv.OpenGLRenderDevice` (contains `OPENGL`, not `XOPENGL`, not `GLES` → WebGL1 / native compat). Do not rename either package.
3. HP2's retail `[OpenGLDrv.OpenGLRenderDevice]` section (`Default.ini:244-250`) carries only the 6 generic flags; the web ini must add the UTGLR settings the template's web ini carries (`UseFragmentProgram=False`, `UsePalette=False`, `OneXBlending=False`, `UseS3TC=True`, `SinglePassFog`/`SinglePassDetail` per gl4es capability) — copy from the template's web `Default.ini` and keep HP2's `DetailTextures=True`.
4. No change to the name-sniffing logic is needed for HP2.

### 8.2 NSDLDrv compile breaks against the 433 headers (not renderer logic, but the renderer cannot start without them)

| # | Site | v400 code | 433 problem | ACTION |
|---|---|---|---|---|
| N1 | `Inc/NSDLDrv.h:26` | `DECLARE_CLASS( UNSDLViewport, UViewport, CLASS_Transient )` | 433 `DECLARE_CLASS` takes 4 args (`Core/Inc/UnObjBas.h:326`) | `DECLARE_CLASS( UNSDLViewport, UViewport, CLASS_Transient, NSDLDrv )` (gate on `ENGINE_VERSION==433` if the file must stay shared with the template; in the HP2 repo it can simply be edited) |
| N2 | `Inc/NSDLDrv.h:109` | `DECLARE_CLASS( UNSDLClient, UClient, CLASS_Transient\|CLASS_Config )` | same | add `, NSDLDrv` |
| N3 | `Src/NSDLClient.cpp:248` | `Viewport->LastUpdateTime<BestViewport->LastUpdateTime` | `LastUpdateTime` is `FTime` (`UnCamera.h:222`), which has no `operator<` (§2.4) | `BestViewport->LastUpdateTime>Viewport->LastUpdateTime` — the exact form HP2's own `WinDrv/Src/WinClient.cpp:365` uses |
| N4 | `Src/NSDLViewport.cpp:1154-1155`, `Inc/NSDLDrv.h:87`, `:1313` | `const FLOAT CurTime = appSeconds(); const FLOAT DeltaTime = CurTime - InputUpdateTime;` with `FLOAT InputUpdateTime` | `appSeconds()` returns `FTime` (`UnGnuG.h:263`), no conversion to `FLOAT` | `FTime InputUpdateTime;` in the header; `const FTime CurTime = appSeconds(); const FLOAT DeltaTime = CurTime - InputUpdateTime;` (`FTime-FTime→float`); `:1313` unchanged |
| N5 | `Inc/NSDLDrv.h:9`, `Src/NSDLDrv.cpp:7` | `#define NSDLDRV_API DLL_EXPORT`, `DLL_EXPORT UBOOL GTickDue;` | 433 gcc `DLL_EXPORT` is `extern "C"` (`UnGnuG.h:97`) | owned by the Core report's `DLL_EXPORT` decision; if `DLL_EXPORT` stays `extern "C"`, `class NSDLDRV_API UNSDLViewport` is ill-formed → define `NSDLDRV_API` as the visibility attribute only |
| N6 | `Src/NSDLClient.cpp:449-456` | `Engine->Audio->SetViewport(…)` | HP2 `UAudioSubsystem::SetViewport(UViewport*)` exists (`Engine/Inc/UnAudio.h:59`) | none |
| N7 | `Src/NSDLClient.cpp:445-448` comment | relies on the template's `UObject::StaticConstructObject` fix that re-loads `CPF_Config` device flags on the web | HP2 `Core/Src/UnObj.cpp` must carry the same fix, otherwise XOpenGL/UTGLR start with `Coronas/VolumetricLighting/ShinySurfaces/DetailTextures/HighDetailActors = 0` on the web | cross-check with the Core report; verify on the first web boot by logging those five flags after `Init` |

### 8.3 What the 433 `UViewport` delta (the 33-line `UnCamera.h` diff) changes for NSDLDrv

Full diff table in §2.3. NSDLDrv-relevant conclusions:

| 433 change | Effect on `UNSDLViewport` | ACTION |
|---|---|---|
| `FTime CurrentTime`, `FTime LastUpdateTime` | N3 (and any future arithmetic on them) | N3 |
| new virtual `TryRenderDevice(const TCHAR*, INT, INT, INT, UBOOL) {}` on `UViewport` (`UnCamera.h:286`) | `UNSDLViewport` does not override it; NSDLDrv keeps its own `UNSDLClient::TryRenderDevice(UViewport*, const char*, UBOOL)` (`NSDLDrv.h:146`) — different class, different signature, no hiding or overload clash. Engine-side callers of the viewport virtual are only `WinDrv` (`WinViewport.cpp:335-341, 2394-2525`) and `UnrealEd/Src/Main.cpp:842-844` (`grep -rn TryRenderDevice Source`) — nothing on the NSDLDrv path calls it. | none. Optional nicety: override it in `UNSDLViewport` to forward to `Client->TryRenderDevice(this, appToAnsi(ClassName), Fullscreen)` so any future engine call (e.g. a renderer-switch exec) works. |
| new virtuals `DrawString`, `CreateNativeFont` (no-op / `NULL`) | Windows GDI native fonts. Callers (`grep -rn 'CreateNativeFont\|->DrawString(' Source --include=*.cpp`, excluding WinDrv/D3DDrv): **only `Editor/Src/UnEdCam.cpp:2314-2445`** (editor control-point labels); `CreateNativeFont` has no caller outside WinDrv. The game never reaches either through NSDLDrv. | none |
| new virtuals `LoadSmackerMovie/BlitNextSmackerFrame/ShutdownSmacker` (defaults `false/true/true`) | Smacker cut-scene movies are a silent no-op on SDL | out of renderer scope (movie playback report); note that `BlitNextSmackerFrame` on WinDrv blits through `GetOsSurface` — a GL port would need its own path (upload the frame as a texture and `DrawTile` it) |
| `snapBuff`, `snapU/V`, `ScreenShot()` + the `SNAP` exec (`Engine/Src/UnCamera.cpp:674-830`) | save-game thumbnails come from `RenDev->ReadPixels` | both forks implement `ReadPixels` with a vertical flip + R/B exchange (`XOpenGL.cpp:2244-2273`, `OpenGL.cpp:5034-5049`), the v400 convention `ScreenShot` expects. XOpenGL binds `RenderFBO` first (correct on WebGL2). **Verify the thumbnail colours/orientation once in the save menu** (§11 Q9); on the threaded web build `ReadPixels` after `commit_frame` reads the offscreen back buffer (`renderViaOffscreenBackBuffer=1`), which is preserved → should be fine. |
| `REN_MatineeIP`, `SHOW_Bounds/SHOW_Bones`, `UClient::{NoFractalAnim, ParticleDensity}` | none | none |

## 9. In-tree devices on the same face (NullDrv, SoftDrv) — brief

| Device | State in HP2 | ACTION |
|---|---|---|
| `NullDrv` (`Source/NullDrv/Src/{NullDrvPrivate.h,NullDrv.cpp}`, template copy) | v400 face: 3-arg `DECLARE_CLASS(UNullRenderDevice,URenderDevice,CLASS_Config)` (`NullDrvPrivate.h:23`), 3-arg `DrawComplexSurface` (`:35`, `NullDrv.cpp:54`), no `MaxVertices`/`DrawTriangles` → **abstract on 433, does not compile/instantiate** | `DECLARE_CLASS(…, CLASS_Config, NullDrv)`; `void DrawComplexSurface(FSceneNode*, FSurfaceInfo&, FSurfaceFacet&, DWORD, BYTE) {}`; `INT MaxVertices() { return 256; }` (callers allocate `MaxVertices()` verts and `6*MaxVertices()` indices even for a null device — 0 would make `UnMeshRn.cpp:113-122` allocate nothing and then write into it); `void DrawTriangles(…) {}`. Keep the `DrawGouraudPolygon` no-op (harmless). Name must keep lacking "OPENGL" (its header comment `:15-19` explains why). `class DLL_EXPORT` → see §8.2 N5. |
| `SoftDrv` (`Source/SoftDrv/Src/*`, HP2's own 433 source) | already implements the 433 face: `DECLARE_CLASS(USoftwareRenderDevice,URenderDevice,CLASS_Config,SoftDrv)` (`SoftDrvPrivate.h:117`), 5-arg `DrawComplexSurface`, `MaxVertices`, `DrawTriangles` (`:156-158`; body `DrawPoly.cpp:4366-4600`) | no face work. It is the **reference for the software semantics** of Lumos (`SpanBased` branch of `UnRender.cpp:3452-3551`) and of `PF_Highlighted\|PF_Translucent` (`DrawPoly.cpp:4380-4385`). Its portability (MSVC `__asm` under `#if ASM`, `SoftDrvPrivate.h:205-215`; framebuffer via `Viewport->ScreenPointer`, NSDLViewport's software surface §8.1 step 7) belongs to the SoftDrv report. |
| `D3DDrv` (`Source/D3DDrv/Src/Direct3D7.cpp`) | Windows-only, not built | reference only (§5.2) |

**`MaxVertices()` must be the same value on every device (use 256 everywhere).** The mesh batcher's buffers are sized **once**, from whichever device renders the first mesh (`Render/Src/UnMeshRn.cpp:113-122`: `if( !VertBuffer.GetData() ) { VBSize = RenDev->MaxVertices(); VertBuffer.Set(VBSize); IndexBuffer.Set(VBSize*6); }`), and the flush test uses `VertBuffer.Num()` (`:191`), not the current device's value. A renderer switch at runtime (e.g. SoftDrv fallback → GL, or `TryRenderDevice` after a failed `Init`) keeps the first size. A device reporting a *smaller* value than the first one would receive batches larger than it promised. Particles re-read `MaxVertices()` every call (`UnParticleRn.cpp:466-476`, `GMem` allocation), so they are not affected. → XOpenGL, UTGLR, NullDrv: **256**, identical to SoftDrv (`DrawPoly.cpp:4366-4370`) and retail D3D7.

Resolved while writing §8-§9 (answers to "verify" notes left in §2-§6):

| Earlier note | Result |
|---|---|
| §2.3 `UPlayer::bSoftwareRendering` — does anything read it? | `grep -rn bSoftwareRendering Source --include=*.{cpp,h,uc}` → only the declaration `Engine/Inc/UnPlayer.h:27`. Dead field; drivers ignore it. |
| §5.1 `FSceneNode::NearClip` exists? | yes, `Engine/Inc/UnRender.h:65` (`FPlane NearClip`). |
| §6.2 U10 fog-map gate | `Render/Src/UnRender.cpp:2430`: `IsVolumetric = RenDev->VolumetricLighting && RenDev->SupportsFogMaps && Zone->bFogZone` — same gate as v400, so UTGLR's web `SupportsFogMaps = 1` is still required. |
| §6.2 U15 HUD tiles at `Z==1` | `UCanvas::DrawTile` forwards the caller's `Z` (`Engine/Src/UnCanvas.cpp:66`); the canvas-level helpers pass their `Z` through (`:93,116`) — same as v400. |

## 10. Checklists (ordered work plan)

### 10.0 Workspace (before anything else)

- [ ] Move the three clones out of the frontend clone: `mv /home/ion/hp2-ufrontend/ufrontend/{XOpenGLDrv,utglr-copy,gl4es} /home/ion/hp2-ufrontend/` (§1 fact 1.1). Confirm `Source/CMakePresets.json` `FETCHCONTENT_SOURCE_DIR_*` / `GL4ES_SOURCE_DIR` then resolve.
- [ ] Decide the gl4es ref (§1 fact 1.3 / §7) — no code change in gl4es.
- [ ] Push `hp2-v433-compat` (XOpenGL) and `hp2` (utglr) once they carry commits, before any preset without local source dirs is used (§1 fact 1.2).

### 10.1 Engine-side prerequisites owned by other reports (renderer cannot compile/start without them)

- [ ] Core: `DLL_EXPORT`/`*_API` under gcc (§2.4, §8.2 N5).
- [ ] Core: web `CPF_Config` repair in `UObject::StaticConstructObject` (§8.2 N7).
- [ ] Config: web boot inis in `Config/System/web/` with `ViewportManager=NSDLDrv.NSDLClient`, `GameRenderDevice` = `WindowedRenderDevice` = the chosen GL device, and full `[XOpenGLDrv.XOpenGLRenderDevice]` / `[OpenGLDrv.OpenGLRenderDevice]` sections copied from the template's web ini (§8.1 actions 1-3).

### 10.2 NSDLDrv (HP2 repo, `Source/NSDLDrv`)

- [ ] N1/N2 4-arg `DECLARE_CLASS` (`Inc/NSDLDrv.h:26,109`).
- [ ] N3 `FTime` comparison (`Src/NSDLClient.cpp:248`).
- [ ] N4 `FTime InputUpdateTime` + `CurTime` (`Inc/NSDLDrv.h:87`, `Src/NSDLViewport.cpp:1154-1155`).
- [ ] (optional) override `UViewport::TryRenderDevice` to forward to the client (§8.3).

### 10.3 NullDrv (HP2 repo, `Source/NullDrv`)

- [ ] 4-arg `DECLARE_CLASS`, 5-arg `DrawComplexSurface`, `MaxVertices()=256`, empty `DrawTriangles` (§9).

### 10.4 XOpenGLDrv fork, branch `hp2-v433-compat` (§4-§5)

- [ ] Add `XOGL_UE1_V4XX` / `XOGL_HP2` in `Inc/XOpenGLDrv.h` (§5.3).
- [ ] X1 `#include "Render.h"` on 433; X8 4-arg `DECLARE_CLASS` (`Inc/XOpenGL.h:523`).
- [ ] Mechanical `ENGINE_VERSION==400` → `XOGL_UE1_V4XX` at X3-X7, X9-X17, X19-X21, X23-X27 (≈25 sites, list in §4.1).
- [ ] X5 split: alpha-blend flags become `PF_XOGL_AlphaBlend = 0x20000000` on 433 (§5.3).
- [ ] C1 5-arg `DrawComplexSurface` with Lumos opacity (`Src/DrawComplex.cpp`).
- [ ] C2 `MaxVertices()=256` + `DrawTriangles` (indexed list / fan / list) (`Src/DrawGouraud.cpp`); zero normals for particles.
- [ ] C3 `HP2AdjustPolyFlags` (`Src/SetTexture.cpp`), called from `DrawTriangles`, `DrawGouraudPolygon`, `DrawTile`.
- [ ] C4 `LINE_PreTransformed` in `Draw3DLine` (`Src/DrawSimple.cpp`).
- [ ] C5 alpha-palette tiles → `PF_Highlighted` (`Src/DrawTile.cpp`).
- [ ] C7 (decide after first look) two-sided polygon offset.
- [ ] Update `Source/XOpenGLDrv/{ENGINE_CHANGES,WEB_INTEGRATION}.md` + CMake comment in the HP2 repo (§5.3 "Docs").
- [ ] Regression: template v400 build symbol set unchanged (§5.4).

### 10.5 UTGLR fork, branch `hp2` (§6)

- [ ] `UTGLR_ENGINE_V400`, `UTGLR_HAS_ALPHABLEND`, synthetic `PF_AlphaBlend` in `OpenGL.h` (§6.5).
- [ ] U1-U3: `UTGLR_ENGINE_UT99DC` → `UTGLR_ENGINE_V400` at `OpenGL.h:362,640`, `OpenGL.cpp:3703`.
- [ ] H1 5-arg `DrawComplexSurface` + Lumos opacity via `glColor4f` + lightmap-unit alpha `REPLACE(PREVIOUS)` (variant a).
- [ ] H2 `MaxVertices()=256` + `DrawTriangles` over the buffered `DrawGouraudPolygon`.
- [ ] H3 `HP2AdjustPolyFlags`; H4 per-vertex alpha (`Buffer3Verts`, `BufferAdditionalClippedVerts`, `DrawGouraudPolygonOld`), 227-style blend/fog masks under `UTGLR_HAS_ALPHABLEND`, `SetBlend` no-occlude for alpha-blend.
- [ ] H5 alpha-palette tiles without `PF_Occlude`.
- [ ] H6 (decide after first look) two-sided polygon offset; H7 `LINE_PreTransformed`.
- [ ] Update `Source/OpenGLDrv/{PORTING,WEB-INTEGRATION}.md` in the HP2 repo.
- [ ] Regression: template v400 build symbol set unchanged (§6.6).

### 10.6 First-light verification (both GL devices, native then web)

- [ ] Boot to the main menu: HUD/menu tiles, alpha-palette icons (C5/H5), mouse cursor on top (U15).
- [ ] First level: Harry + NPC meshes (DrawTriangles indexed lists), particles (index list with unused verts), actor blob shadows (modulated fan decals), fog zones (fog maps).
- [ ] A Lumos surface (any level with invisible walkways; cast Lumos): appear/disappear fade with distance, depth behaviour (§3.3).
- [ ] A fading actor (`Opacity<1`: e.g. ghosts / fade-out on death/teleport): straight alpha, not additive (§3.4).
- [ ] First-person/overlay meshes: near-Z range hack (UTGLR U18).
- [ ] Save-game thumbnail colours/orientation (§8.3).
- [ ] Log scan: no `Unknown texture format`, no GL errors, `SupportsTC` / DXT1 upload count.

## 11. Open questions / how to verify

| # | Question | Why it matters | How to verify |
|---|---|---|---|
| Q1 | Does retail D3D7 really draw Harry's blob shadow invisible (the `PF_LumosAffected` `&`-test quirk, §3.3 quirk 1)? | decides whether the GL ports' "Lumos only when `PF_HighShadowDetail` + blend bit" rule is parity or a fix | run retail `Assets/System/Game.exe` under Wine (wined3d, D3DDrv), stand in any lit room, screenshot Harry's feet; record in `docs/porting/`. The GL rule stays either way (SoftDrv draws the shadow). |
| Q2 | Which HP2 surfaces actually carry `PF_HighShadowDetail` without a blend bit? | the GL Lumos test requires `PF_Modulated\|PF_Translucent` too; a Lumos surface authored with only `PF_HighShadowDetail` would be missed | once the port loads maps, log (one line per surface, first frame only) every `DrawComplexSurface` whose flags `& PF_LumosAffected` with the flags value; or scan `Assets/Maps/*.unr` with a package reader for `FBspSurf.PolyFlags & 0x00800000`. |
| Q3 | Two-sided depth bias needed (C7/H6)? | HP2-only `bTwoSidedBias` default true (`Actor.uc:312,1741`) | close-up of Harry's robe/hair in the first level on both GL devices; z-fighting ⇒ enable the polygon offset. |
| Q4 | Any HP2 texture with `UTexture::Alpha > 0`? | XOpenGL fades those (227 convention, §3.5) | temporary log in XOpenGL `SetTexture` when `Info.Texture->Alpha > 0` over the first levels; if hits, force opacity 1 under `XOGL_HP2`. |
| Q5 | How much DXT1 content does HP2 ship? | S3TC path exercise; WebGL2 non-4-aligned DXT1 decode (X21) | count `TEXF_DXT1` (format 3) uploads per level via the driver log (`DebugLevel`), or a package scan of `Assets/Textures/*.utx`. |
| Q6 | UTGLR Lumos alpha: is lightmap alpha 255? | if not, H1 variant (a) (`REPLACE(PREVIOUS)` on units ≠0) is required, not just nice | first Lumos surface on UTGLR: if the fade is patchy along lightmap texels, (a) is needed; implement (a) up front anyway — it is cheap and safe. |
| Q7 | XOpenGL gamma defaults on 433 (X14-X16) | zero-init web device | log `GammaCorrection/GammaMultiplier` after `Init` on the first web boot. |
| Q8 | Premultiplied alpha-palette tiles: edges glow? (C5) | D3D7 does not premultiply; parity says leave it | compare one HUD icon against a retail screenshot. |
| Q9 | Save thumbnails (`SNAP` → `ReadPixels`) colour order and orientation | both forks flip + swap R/B (v400 convention); HP2's `ScreenShot`/snap consumer is v400 code | save once, look at the thumbnail in the load menu and at the BMP in the save folder. |
| Q10 | Is `DrawTriangles` hot enough on the web to need a whole-list fast path in UTGLR (H2 loops per triangle)? | perf only | `stat fps` / browser profiler in a crowded scene (Great Hall); optimise only if the UTGLR build is chosen as a shipping renderer. |
| Q11 | Any shipped UnrealScript (not in `Source/*/Classes`) calling `CreateTextureFromScreenShot/BMP` (TEXF_RGBA8 at runtime)? | UTGLR/D3D7 would mis-read RGBA8 (§6.3) | search the shipped `.u` name tables (`Assets/System/*.u`) with a package reader for those function names; `grep -a -o 'CreateTextureFrom[A-Za-z]*' Assets/System/*.u` finds the names only in `Engine.u` (the declarations) — **not conclusive**, because calls to `native(321/322) final` functions compile to the extended native opcode (`0x61 0x41` / `0x61 0x42`) with no name import. Conclusive check: disassemble `HGame.u`/`HPMenu`-side packages with a UE1 bytecode reader, or break on `AActor::execCreateTextureFromScreenShot` in a native build during a save. |

## Status: complete

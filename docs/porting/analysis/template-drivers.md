Record — kept as history; the current state is in README.md, BUILD.md and the change inventory.

# template-drivers — the helper/net drivers of engine-ut99 vs HP2 build 433

Key: `template-drivers`. Subject: `NSDLDrv`, `NullDrv`, `SoundDrv`, `NOpenALDrv`, `IpDrv`, `WebRTCDrv`
(+`WebRTCLan`) as they exist in `/home/ion/engines/engine-ut99/Source/` (the template, UT99 v400), the
exact engine API each one uses, and what changes when the same code is compiled against HP2's build-433
headers in `/home/ion/hp2-ufrontend/HarryPotter2/Source/{Core,Engine}/Inc`. The copies in
`HarryPotter2/Source/{NSDLDrv,NullDrv,SoundDrv,NOpenALDrv,IpDrv,WebRTCDrv}` are byte-identical to the
template today (`diff -rq`: only `IpDrv/CMakeLists.txt` differs — the Dreamcast pthread block was
already dropped), so every line number below applies to both trees.

Paths are abbreviated: `T/` = `/home/ion/engines/engine-ut99/Source/`, `H/` =
`/home/ion/hp2-ufrontend/HarryPotter2/Source/`, `E/` = `/home/ion/epic-sources/ut99-v400/`.

> **Two passes.** Sections 0–10 are the first analyst's pass; **§11 is an independent second pass** that
> verifies them against source and bytes, corrects nine points (C1–C9: NullDrv `MaxVertices` must be 256 not 0,
> `ParticleDensity=1`, HP2 has no script gesture matching, EA-XA decode settled against the lip-sync oracle,
> `ServerActors`/`AudioDevice` stale values are *fatal*, …) and adds A1–A16. Where the two disagree, §11 wins.

---

## 0. Executive summary (read this if nothing else)

| Driver | Verdict | Effort | The one thing that decides it |
|--------|---------|--------|-------------------------------|
| **NSDLDrv** | ports almost as-is | small | HP2's `UViewport::LastUpdateTime` and `appSeconds()` are `FTime`, not `DOUBLE`; `FTime` has no `<` or implicit float conversion (`H/Core/Inc/Core.h:26-53`). Three call sites to touch. EInputKey is **identical** to v400 (255 entries, diffed). |
| **NullDrv** | mechanical | small | HP2's `URenderDevice` (`H/Engine/Inc/UnRenDev.h:75-93`) changes `DrawComplexSurface`'s signature and adds pure `MaxVertices()` + `DrawTriangles()`; `DrawGouraudPolygon` is no longer pure. 3 new/changed stubs. |
| **SoundDrv (null)** | rewrite the 20-line class | small | HP2's `UAudioSubsystem` (`H/Engine/Inc/UnAudio.h:52-83`) shares only `Init/SetViewport/Exec/NoteDestroy/GetViewport/RenderAudioGeometry/PostRender` names with v400; 11 pure virtuals to stub with HP2 signatures. No `UMusic` exists in HP2. |
| **NOpenALDrv** | **not a port of the template — a port of HP2's own `ALAudio`** to OpenAL 1.1 + libvorbis + a 50-line EA-XA ADPCM decoder | the big one (Phase E) | 2994 of 3289 sounds (121 MB of 127 MB) are `FileType=XA`, `CoreFlags` `SF_Compressed|SF_Streaming`: raw **EA-XA block ADPCM, 15 bytes per 28 mono 16-bit samples** (measured 0.5357 bytes/sample on all 2994; produced by `sx -eaxa_blk -raw`, `H/Engine/Src/UnAudio.cpp:580`). The closed `coda` lib only decodes this one format at runtime (`CEAXABLKDec`, `H/Core/Src/UnVcWin32.cpp:773-823`); it is a well-known codec (ffmpeg `adpcm_ea_r1`/`maxis_xa`, vgmstream `ea_xa_int`). Music is 111 plain Ogg Vorbis files in `Assets/Music`. |
| **IpDrv** | ports with ~10 mechanical edits | small–medium | v400→433 net headers differ only by `DOUBLE→FTime` and the autodownload fields; **no** signature changes. The script side (`IpDrv.u`) is optional: `TcpNetDriver`/`TcpipConnection` need no script classes; `IpDrvClasses.h` must be regenerated for HP2's `Engine.u` if `IpDrv.u` is compiled. |
| **WebRTCDrv** | ports with 3 edits | small | Same `UNetDriver`/`UNetConnection` face; only `DOUBLE`-typed locals and the `"ut99"` peer prefix change. `WebRTCLan` compiles but is inert: HP2 has no `UBrowser`/`UdpBeacon` script and no server browser. |

Cross-cutting: every `DECLARE_CLASS(A,B,Flags)` in the six drivers must become `DECLARE_CLASS(A,B,Flags,<Pkg>)`
(HP2 `H/Core/Inc/UnObjBas.h:326,336`); `IMPLEMENT_CLASS/IMPLEMENT_PACKAGE/IMPLEMENT_FUNCTION` stay as written
once Phase A adopts the template's macro bodies (already decided in `PLAN.md` §2). Natives inside a package
(only `IpDrv` has them) resolve through the template's `STATIC_EXPORT` registry, **not** through HP2's
`GNativeLookupFuncs` table — see §6.4.

---

## 1. Cross-cutting 433 facts (hit by every driver)

### 1.1 Class/package macros

| Item | Template (v400/ut99dc) | HP2 433 | Action |
|------|------------------------|---------|--------|
| `DECLARE_CLASS` | 3 args (`T/Core/Inc/UnObjBas.h`) | 4 args, last = package name: `H/Core/Inc/UnObjBas.h:326` `DECLARE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage )`; `:336` `DECLARE_ABSTRACT_CLASS(…,TPackage)`. The 4th arg is only consumed by the `__STATIC_LINK` branch (`:299-325`, `GetPrivateStaticClass##TClass( TEXT(#TPackage) )`), which the plan keeps **undefined** — so it is a no-op token, but the macro arity is fixed. | Add `,NSDLDrv` / `,NullDrv` / `,SoundDrv` / `,NOpenALDrv` / `,IpDrv` / `,WebRTCDrv` to every `DECLARE_CLASS` in the drivers (12 sites, listed per driver). |
| `IMPLEMENT_CLASS(X)` | `T/Core/Inc/UnObjBas.h:335-352`: constructs `PrivateStaticClass` with `THIS_PACKAGE`, exports `autoclass##X` and `STATIC_EXPORT( X, autoclass##X )` | `H/Core/Inc/UnObjBas.h:350-386`: same constructor shape but with `GPackage`, **no** autoclass export, no `STATIC_EXPORT`; plus a `__STATIC_LINK` variant at `:387` | Phase A (PLAN §3 `UnObjBas.h`) replaces HP2's macro body with the template's. Drivers keep `IMPLEMENT_CLASS(X)` verbatim. Verify after Phase A: `grep -n STATIC_EXPORT H/Core/Inc/UnObjBas.h` must hit inside `IMPLEMENT_CLASS`. |
| `IMPLEMENT_PACKAGE(pkg)` | `T/Core/Inc/UnObjBas.h:355-357` → `THIS_PACKAGE[]` + `IMPLEMENT_PACKAGE_PLATFORM` (`T/Core/Inc/UnGnuG.h:221-223`: `GLoaded##pkg` + `STATIC_EXPORT`) | `H/Core/Inc/UnObjBas.h:431-433` → `GPackage[]` + `H/Core/Inc/UnGnuG.h:175-176` (`BYTE GLoaded##pkgname;` only, no export) | Phase A `UnGnuG.h` = "take the template's file" already covers `IMPLEMENT_PACKAGE_PLATFORM`; `IMPLEMENT_PACKAGE` in `UnObjBas.h` must also take the template's `THIS_PACKAGE` spelling **or** keep `GPackage` consistently in both `IMPLEMENT_CLASS` and `IMPLEMENT_PACKAGE` (they must agree — the class constructor reads the same symbol the package macro defines). Drivers unchanged. |
| Package lookup (`appGetDllHandle`) | `T/Core/Src/UnUnix.cpp:269-291`: builds `"GLoaded<Pkg>"` and resolves it with `appGetStaticExport` (registry in `T/Core/Src/UnStaticExports.cpp`, already copied to `H/Core/Src/UnStaticExports.cpp`) | HP2's own `UnUnix.cpp` is Loki-era `dlopen` | Phase A takes the template's `UnUnix.cpp`. Every driver's `IMPLEMENT_PACKAGE` then registers itself; nothing driver-side. |
| `DLL_EXPORT_CLASS` (used by `TcpNetDriver.cpp:40,161`, `WebRTCDrv.cpp:91,172`) | empty on GCC (`T/Core/Inc/UnGnuG.h:95`) | empty on GCC (`H/Core/Inc/UnGnuG.h:98`) | none |
| `NO_DEFAULT_CONSTRUCTOR(cls)` | protected `cls() {}` | identical (`H/Core/Inc/UnObjBas.h:264-265`) | none |
| `StaticConstructor` vs `InternalClassInitializer` | v400 calls `StaticConstructor` from `IMPLEMENT_CLASS`; the template already migrated NSDLClient/NOpenAL (comments at `T/NSDLDrv/Src/NSDLClient.cpp:16-24`, `T/NOpenALDrv/NOpenALDrv.cpp:112-121`) | 433 identical (`(void(UObject::*)())&TClass::StaticConstructor` in `H/Core/Inc/UnObjBas.h:350-366`) | none; the dead `InternalClassInitializer` hooks (`NSDLDrv.h`, `NSDLViewport.cpp:189-195`, `SoundDrv/Null.cpp:32`) can stay or go. |

### 1.2 Time

HP2 replaced `DOUBLE` timestamps with the fixed-point `FTime` (`H/Core/Inc/Core.h:26-53`):
`GetFloat()`, `operator+(float)`, `float operator-(FTime)`, `operator==/!=/>` — **no `operator<`, no
`operator>=`, no implicit conversion to `float`/`double`**, no `operator-(float)`. `appSeconds()` returns
`FTime` (`H/Core/Inc/UnGnuG.h:262-271`, `appSecondsSlow()` `:195`). Affected driver lines:

| File:line | Code | Problem | Fix |
|-----------|------|---------|-----|
| `NSDLClient.cpp:248` | `Viewport->LastUpdateTime<BestViewport->LastUpdateTime` | `UViewport::LastUpdateTime` is `FTime` (`H/Engine/Inc/UnCamera.h:222`); no `<` | `BestViewport->LastUpdateTime > Viewport->LastUpdateTime` (operator> exists) |
| `NSDLViewport.cpp:1154-1155` | `const FLOAT CurTime = appSeconds(); const FLOAT DeltaTime = CurTime - InputUpdateTime;` | `FTime`→`FLOAT` no conversion | `const FTime Now = appSeconds(); const FLOAT DeltaTime = Now - InputUpdateTime;` and make `InputUpdateTime` an `FTime` (`NSDLDrv.h:87`), assign at `:1313` |
| `NOpenALDrv.cpp:902-904` (`DOUBLE DeltaTime = appSeconds() - MusicTime; MusicTime += DeltaTime;`) | same | irrelevant — this whole music block is replaced (HP2 has no `UMusic`/`Song`/`Transition` on the player; see §5) |
| `WebRTCDrv.cpp:399-402` | `static DOUBLE LastPush = 0.0; DOUBLE Now = appSeconds(); if( Now - LastPush > 1.0 )` | same | `static FTime LastPush; FTime Now = appSeconds(); if( Now - LastPush > 1.f ) { LastPush = Now; …}` |
| `WebRTCLan.cpp:89,101,105-106,188,217,430,488,504,517` | `DOUBLE` timestamps compared with `<`/`>=`/`+` | same | either `FTime` + `operator-` everywhere, or (simplest, and this file is inert on HP2 — §7.3) keep `DOUBLE` and write `appSeconds().GetFloat()` |
| `IpDrv` | none — `TcpLink/UdpLink/TcpNetDriver` never touch time | | |
| Engine-side (not driver): `UNetDriver::Time`, `UNetConnection::LastReceiveTime/LastSendTime/…`, `FOutBunch::Time`, `UActorChannel::RelevantTime` are `FTime` in HP2 (`H/Engine/Inc/UnNetDrv.h:45`, `UnConn.h:80-83,96,114-115,120`, `UnBunch.h:18`, `UnChan.h:112-113`) | drivers only read `State`, `URL`, `MaxPacket`, `PacketOverhead` — none of the timestamps | none |

### 1.3 Input

`EInputKey` in `H/Engine/Inc/EngineClasses.h:131-388` is **identical** to `E/Engine/Inc/EngineClasses.h`
(255 entries, `difflib` diff empty). `EInputAction` (`IST_Press/Release/Axis/Hold`) unchanged. `UInput`
(`H/Engine/Inc/UnIn.h`) adds one virtual, `DirectAxis( EInputKey, FLOAT Length, FLOAT Delta )` (`:65`,
impl `H/Engine/Src/UnIn.cpp:379-389`: builds `"<binding> Speed=<Length>"` and execs it with `IST_Hold`)
and a `SPEEDBASE=` axis modifier (`UnIn.cpp:273-289`). **Nothing in HP2 calls `DirectAxis`** (grep over
`H/` finds only the declaration and definition — not even `WinDrv`), so NSDLDrv need not use it.

`UEngine` hooks NSDLDrv calls (`H/Engine/Inc/UnEngine.h:57-63`): `Key(UViewport*,EInputKey)`,
`InputEvent(UViewport*,EInputKey,EInputAction,FLOAT Delta=0.f)`, `Draw(UViewport*,UBOOL Blit=1,BYTE*,INT*)`,
`MouseDelta(UViewport*,DWORD,FLOAT,FLOAT)`, `MousePosition(UViewport*,DWORD,FLOAT,FLOAT)`,
`Click(…)` — all same as v400.

### 1.4 Gesture (spell) input — what NSDLDrv must guarantee

HP2's spell casting is **not** a separate input path. The gesture matcher `UGesture`
(`H/Engine/Src/UnGesture.cpp`, `execCompareGesture/execCompareGesturePoint` bound as natives 426/427,
`EngineClasses.h:3698-3706,3919-3920`) receives 2D points from **script** (`HGame/Classes/SpellCursor.uc`,
`GestureSprite.uc`, `HChar.uc`). Script derives them from the ordinary mouse axes: `DefUser.ini:105-106`
binds `MouseX=Axis aMouseX Speed=4.0 | Axis aBroomYaw Speed=4.0`, `MouseY=Axis aMouseY Speed=4.0 | …`, and
`Harry.uc:5310-5311` forwards `aMouseX/aMouseY` (`SmoothMouseX` in `BaseCam.uc:533`). The menu cursor
uses the absolute path: `HPConsole.uc:1117-1119` and `UWindow/Classes/WindowConsole.uc:350-352` read
`Viewport.WindowsMouseX/Y` when `Viewport.bWindowsMouseAvailable` (fields on `UPlayer`,
`H/Engine/Inc/UnPlayer.h:24-28`, script `Engine/Classes/Player.uc:21-24`), and `VendorManager.uc:190` /
`WindowConsole.uc:249,271` toggle `bShowWindowsMouse`.

So the template's NSDLViewport already delivers exactly what HP2 needs:
- relative motion → `Client->Engine->MouseDelta(…)` **and** `IK_MouseX/IK_MouseY IST_Axis` (`NSDLViewport.cpp:1259-1261`) — this is the raw delta feed the `aMouseX` bindings integrate (matches the retail WinDrv path `H/WinDrv/Src/WinViewport.cpp:1807,1830-1831`);
- absolute motion → `Client->Engine->MousePosition(this,0,x,y)` + the same axis deltas (`:1247-1249`), which `UGameEngine::MousePosition` (`H/Engine/Src/UnGame.cpp:2054-2071`) stores into `WindowsMouseX/Y`;
- `SetMouseCapture` → `SDL_SetRelativeMouseMode` (`:1109-1133`).

**Two HP2-specific actions:**
1. `ResizeViewport` sets `bWindowsMouseAvailable = !(NewBlitFlags & BLIT_Fullscreen)` (`NSDLViewport.cpp:853`). HP2's retail `WinViewport.cpp:547` instead sets it in `SetMouseCapture`: `bWindowsMouseAvailable = !Capture;`. Because `HPConsole`/`UWindow` only trust `WindowsMouseX` when the flag is up, keep the template's line **and** add `bWindowsMouseAvailable = !Capture;` at the top of `UNSDLViewport::SetMouseCapture` so the flag flips with capture exactly as the game expects (in a menu the mouse is uncaptured and absolute; in-game it is captured and relative-only).
2. HP2's `UGameEngine::Tick` capture policy (`H/Engine/Src/UnGame.cpp:1722-1740`) releases capture while `Pauser!=""`, `bShowMenu` or `bShowWindowsMouse`, and `MouseDelta` (`:2028-2049`) re-captures on `MOUSE_FirstHit` when `Client->CaptureMouse` is off. This is v400's stock logic; the template's ufront 2.38 change to `UnGame.cpp` ("release relative mode while a menu is up") is the same idea — port it by analogy in Phase A (`UnGame.cpp` is HP2-authoritative per PLAN §3). On the web, `CaptureMouse=True` in `[NSDLDrv.NSDLClient]` gives UT2004-style pointer lock (template `Launch.cpp:499+`).

Verify: with `-log`, cast a spell in `Ch1Rictusempra` and confirm `CompareGesture` returns non-zero
(add a temporary `debugf` in `UGesture::execCompareGesture`), and that the `HPConsole` cursor follows the
pointer with `bShowWindowsMouse` on (Vendor screens).

### 1.5 Native function binding in a static binary (matters for IpDrv only)

HP2 433 has two mechanisms; only one is live in our configuration:
- `H/Core/Inc/UnCoreNative.h:12-46` / `UnCoreNative.cpp:272-275` / `Engine/Src/UnEngineNative.cpp:76-238`: per-class name→member-pointer tables (`GEngineAActorNatives[]`…) and the 32-slot `GNativeLookupFuncs[]`. It is consulted **only** under `#if __STATIC_LINK` in `UPackage::GetDllExport` (`H/Core/Src/UnObj.cpp:4027`), and the table is filled only by `H/UCC/Src/UCC.cpp:125-139` (the PS2/Loki static launcher). With `__STATIC_LINK` undefined (PLAN §2) this path is dead code.
- The live path: `UFunction::Bind` (`H/Core/Src/UnClass.cpp:1775-1800`) builds `"int<Class>exec<Func>"` and calls `ClassPackage->GetDllExport(Proc,1)` → `appGetDllExport(DllHandle,…)` → in the template's `UnUnix.cpp:363-374` → `appGetStaticExport(ProcName)` (the `STATIC_EXPORT` registry). The template's `IMPLEMENT_FUNCTION` (`T/Core/Inc/UnScript.h:28-31`) registers each native with `STATIC_EXPORT( int##cls##func, … )`; HP2's (`H/Core/Inc/UnScript.h:28-30`) does not.

**Action (Phase A, `UnScript.h`):** take the template's `IMPLEMENT_FUNCTION` body (adds the `STATIC_EXPORT` line). Then `IpDrv.cpp:19-25`'s `AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)` expansion of `IpDrvClasses.h:234-255` registers all 22 natives with no IpDrv-side change. `WebRTCDrv`, `NSDLDrv`, `NullDrv`, `SoundDrv`, `NOpenALDrv` have **no natives**. Do not add IpDrv to `GNativeLookupFuncs`; leave `UnCoreNative.h` compiling (fix its MSVC token pasting per PLAN §3) but unused.

### 1.6 Static package registration (`autoclass`, `GLoaded`)

Each driver relies on the template's `IMPLEMENT_CLASS` exporting `autoclass##Class` and `IMPLEMENT_PACKAGE`
exporting `GLoaded##Pkg` into the `GExportsTable` (`H/Core/Inc/UnStaticExports.h:24-25`, copied). The
engine finds `NSDLDrv.NSDLClient` etc. by `StaticLoadClass(…,"ini:Engine.Engine.ViewportManager")` →
`appGetDllHandle("NSDLDrv")` → `GLoadedNSDLDrv` → then the class by `autoclassUNSDLClient`. Nothing to do in
the drivers; the top-level `H/CMakeLists.txt` must `--whole-archive` them (it already lists
`NSDLDrv NullDrv SoundDrv IpDrv WebRTCDrv` in `INSTALL_TARGETS`, `:279-362`).

---

## 2. NSDLDrv (SDL2 window/input/GL-context client)

Files: `T/NSDLDrv/Inc/NSDLDrv.h` (156), `Src/NSDLDrv.cpp` (13), `Src/NSDLClient.cpp` (469),
`Src/NSDLViewport.cpp` (1363), `CMakeLists.txt` (21; links `${SDL2_LIBRARY} Engine Core`, defines
`NSDLDRV_EXPORTS UPACKAGE_NAME=NSDLDrv`).

### 2.1 Engine API inventory

| Header/class | Member | Used at | HP2 status (`H/Engine/Inc/…`) |
|--------------|--------|---------|-------------------------------|
| `UViewport` (`UnCamera.h`) | `DECLARE_CLASS(UNSDLViewport,UViewport,CLASS_Transient)` | `NSDLDrv.h:26` | 4-arg macro (`UnCamera.h:211` base is `DECLARE_ABSTRACT_CLASS(UViewport,UPlayer,CLASS_Transient,Engine)`) → **add `,NSDLDrv`** |
| | `Lock(FPlane,FPlane,FPlane,DWORD,BYTE*,INT*)`, `Unlock(UBOOL)`, `Exec`, `Repaint`, `SetModeCursor`, `UpdateWindowFrame`, `OpenWindow(DWORD,UBOOL,INT,INT,INT,INT)`, `CloseWindow`, `UpdateInput(UBOOL)`, `MakeCurrent`, `MakeFullscreen(INT,INT,UBOOL)`, `GetWindow`, `SetMouseCapture(UBOOL,UBOOL,UBOOL)`, `IsFullscreen`, `ResizeViewport(DWORD,INT,INT,INT)` | `NSDLDrv.h:37-51`, impls `NSDLViewport.cpp:712,870,1324,980,262,271,309,672,1094,952,1041,1031,1109,781,794` | all present with the same signatures (`UnCamera.h:272-274` etc.; the epic→HP2 diff of `UnCamera.h` shows no signature change). HP2 adds **non-pure** `TryRenderDevice(const TCHAR*,INT,INT,INT,UBOOL)`, `DrawString`, `CreateNativeFont`, `LoadSmackerMovie/BlitNextSmackerFrame/ShutdownSmacker` (`:286-293`) with default bodies → nothing to implement, but note `UViewport::TryRenderDevice` now **exists as a virtual with a different signature** from `UNSDLClient::TryRenderDevice(UViewport*,const char*,UBOOL)` — different classes, no clash. |
| | fields `SizeX,SizeY,ColorBytes,Caps,ScreenPointer,Stride,Current,RenDev,Input,Actor,Console,HitTesting,LastUpdateTime` | `NSDLViewport.cpp:226-231,239-240,648-649,755-766,983` | present (`UnCamera.h:216-240`); `LastUpdateTime` is `FTime` → §1.2 |
| | `bWindowsMouseAvailable` (on `UPlayer`) | `NSDLViewport.cpp:853` | `UnPlayer.h:24` → keep; also set from `SetMouseCapture` (§1.4) |
| | `IsRealtime()` | `NSDLClient.cpp:246` | `UnCamera.h:311` |
| | `BLIT_Fullscreen`, `CC_RGB565` | `:812`, `:230,594` | `UnCamera.h:34`, `:163` |
| | `Actor->ShowFlags & SHOW_PlayerCtrl / SHOW_ChildWindow`, `Actor->RendMap`, `REN_*` | `:281-291,407,484,1336` | present; HP2 adds `REN_MatineeIP=19`, `SHOW_Bounds/SHOW_Bones` — harmless |
| `UClient` (`UnCamera.h:369-420`) | `DECLARE_CLASS(UNSDLClient,UClient,CLASS_Transient\|CLASS_Config)` | `NSDLDrv.h:109` | **add `,NSDLDrv`** |
| | `Init(UEngine*)`, `ShowViewportWindows`, `EnableViewportWindows`, `Poll`, `CurrentViewport`, `Exec`, `Tick`, `MakeCurrent`, `NewViewport(FName)`, `EndFullscreen` | `NSDLDrv.h:134-143` | HP2 `UClient` has **no `Poll()`, no `CurrentViewport()`, no `EndFullscreen()` virtuals** (`UnCamera.h:409-418`: `Init, Flush, ShowViewportWindows, EnableViewportWindows, Tick, Exec, NewViewport, MakeCurrent`). epic v400 doesn't either — the template declares them as plain (non-overriding) members. Keep as-is; they are called only from NSDLDrv itself. |
| | config fields `CaptureMouse, CurvedSurfaces, ScreenFlashes, NoLighting, Decals, NoDynamicLights, WindowedViewportX/Y, WindowedColorBits, FullscreenViewportX/Y, FullscreenColorBits, Brightness, MipFactor, TextureLODSet[], MinDesiredFrameRate, DrawCycles, Viewports, Engine` | `NSDLClient.cpp:55-77,112-127`, `NSDLViewport.cpp:347-348,715,1020-1022` | all present (`UnCamera.h:373-394`). HP2 adds `NoFractalAnim` (`:383`) and `ParticleDensity` (`:394`) — assign defaults in `StaticConstructor` (`NoFractalAnim=0; ParticleDensity=100` — check `H/Engine/Src/UnCamMgr.cpp` `UClient::StaticConstructor` for the registered name/range before choosing) |
| | `SaveConfig()` | `NSDLViewport.cpp:1022,1062` | UObject |
| `UEngine` (`UnEngine.h`) | `Audio->ConditionalShutdownAfterError()`, `Audio->SetViewport()`, `Audio->GetViewport()` | `NSDLClient.cpp:178,451,455`, `NSDLViewport.cpp:800-801,856-857` | present (`UnAudio.h:59,72`) |
| | `InputEvent`, `Key`, `MousePosition`, `MouseDelta`, `Draw` | `NSDLViewport.cpp:1141,1171,1247,1259,984` | `UnEngine.h:57-62`, identical |
| `URenderDevice` (`UnRenDev.h`) | `Exit()`, `Init(Viewport,X,Y,ColorBytes,Fullscreen)`, `HighDetailActors` | `NSDLClient.cpp:436-454` | `UnRenDev.h:67-68,53` |
| `ULevel` | `Actor->GetLevel()->DetailChange(UBOOL)` | `NSDLClient.cpp:454` | `UnLevel.h:209` |
| `UObject` | `StaticLoadClass(URenderDevice::StaticClass(),NULL,*ClassNameStr,NULL,LOAD_NoWarn,NULL)`, `ConstructObject<URenderDevice>`, `CastChecked/Cast`, `new(this,Name)UNSDLViewport(NULL,this)`, `GetFlags()&RF_Destroyed` | `NSDLClient.cpp:443-446,239,293`, `NSDLViewport.cpp:800` | same |
| Core | `GConfig->GetInt/GetString`, `Parse(…,QWORD&)` (`:390-392`), `ParseParam`, `ParseCommand`, `appCmdLine`, `appStrupr`, `appFromAnsi`, `appQsort` (`NSDLClient.cpp:366`), `appMalloc/appFree`, `appMemset`, `appSprintf`, `appAtoi`, `appStrchr`, `LocalizeGeneral("Product","Core")`, `LocalizeError("Failed3D")`, `debugf(NAME_Init/Log/Warning/Exit)`, `appErrorf`, `appExit`, `clock/unclock`, `Align`, `Clamp`, `Abs`, `FString::Printf/InStr/Len`, `TArray::AddItem/AddUniqueItem/Empty` | throughout | all present in HP2 Core (`UnFile.h:295-298,337,374-375`). `Failed3D` lives in `Assets/System/WinDrv.int:7`, not `Engine.int` → `LocalizeError("Failed3D")` (no package arg → current package `NSDLDrv`) will log "?int?NSDLDrv.Errors.Failed3D?" — cosmetic; change to a literal or `LocalizeError("Failed3D","WinDrv")`. |
| Globals | `GIsEditor`, `GLog`, `GTickDue` (own) | | same |
| `UInput::Init(UViewport*)` | `NSDLViewport.cpp:240` (editor only) | `UnIn.h` | same |

### 2.2 Adaptation checklist (NSDLDrv)

- [ ] `NSDLDrv.h:26` `DECLARE_CLASS( UNSDLViewport, UViewport, CLASS_Transient, NSDLDrv )`; `:109` `DECLARE_CLASS( UNSDLClient, UClient, CLASS_Transient|CLASS_Config, NSDLDrv )`.
- [ ] `NSDLDrv.h:87` `FLOAT InputUpdateTime` → `FTime InputUpdateTime`; `NSDLViewport.cpp:1154-1155,1313` per §1.2.
- [ ] `NSDLClient.cpp:248` swap the `<` for `>` (§1.2).
- [ ] `NSDLClient.cpp:55-87` (`StaticConstructor` defaults) — add `NoFractalAnim = 0; ParticleDensity = <default>;` (HP2-only `UClient` fields; check `UnCamMgr.cpp` for the shipped default; `Config/System/Default.ini` `[WinDrv.WindowsClient]` may carry `ParticleDensity=`).
- [ ] `NSDLClient.cpp:24-33` — config section registration names are per-class (`[NSDLDrv.NSDLClient]`), fine. The HP2 ini has `[WinDrv.WindowsClient]`; the launcher's ini normalisation (template `Launch.cpp:489`) writes `ViewportManager=NSDLDrv.NSDLClient`; add a `[NSDLDrv.NSDLClient]` block to `Config/System/web/Default.ini` (copy the template's, PLAN §6.5).
- [ ] `NSDLViewport.cpp:1109-1133` add `bWindowsMouseAvailable = !Capture;` (§1.4).
- [ ] `NSDLViewport.cpp:362-384` device-name heuristic: GL context iff the resolved `GameRenderDevice`/`WindowedRenderDevice` name contains `OPENGL`; WebGL2 iff it contains `XOPENGL`. HP2 device names: `XOpenGLDrv.XOpenGLRenderDevice`, `OpenGLDrv.OpenGLRenderDevice`, `SoftDrv.SoftwareRenderDevice`, `NullDrv.NullRenderDevice` — the heuristic holds unchanged. HP2's shipped `Default.ini:22,31` names `D3DDrv.D3DRenderDevice` / `SoftDrv.SoftwareRenderDevice`; the launcher's `-SOFTWARERENDERER/-NULLRENDERER/-XOPENGLRENDERER/-UTGLRRENDERER` switch table + `NormalizeSubsystem` (template `Launch.cpp:535-547`) must map D3D→XOpenGL.
- [ ] `NSDLViewport.cpp:460` `LocalizeError("Failed3D")` — see table.
- [ ] `NSDLViewport.cpp:189-195` / `NSDLDrv.h:31` — optional cleanup of the dead `InternalClassInitializer`.
- [ ] No change to the web/worker context code (`:14-38, 421-429, 503-556, 729-757, 885-931, 1276-1284`); it depends only on SDL/Emscripten.
- [ ] `CMakeLists.txt` unchanged (the top-level `H/CMakeLists.txt:299-302` already adds it under `USE_SDL`).

Verify: `Game -SOFTWARERENDERER -log` reaches `Opened viewport` and `NSDLClient: 1280x720, …` (`NSDLClient.cpp:125`), then `-NULLRENDERER`; keyboard `Escape` opens the HP2 menu; `getres`/`setres` exec.

---

## 3. NullDrv (GPU-free render device)

Files: `T/NullDrv/Src/NullDrvPrivate.h` (49), `Src/NullDrv.cpp` (67), `CMakeLists.txt` (27; includes
`../Render/Src` for `RenderPrivate.h`).

### 3.1 API inventory and the 433 delta

| Member (v400, `NullDrvPrivate.h` line) | HP2 `URenderDevice` (`H/Engine/Inc/UnRenDev.h`) | Action |
|--------|------|--------|
| `DECLARE_CLASS(UNullRenderDevice,URenderDevice,CLASS_Config)` `:23` | `DECLARE_ABSTRACT_CLASS(URenderDevice,USubsystem,CLASS_Config,Engine)` `:38` | `DECLARE_CLASS(UNullRenderDevice,URenderDevice,CLASS_Config,NullDrv)` |
| `Init(UViewport*,INT,INT,INT,UBOOL)` `:29` / `NullDrv.cpp:26` | `:67` pure, same | keep; it sets `SpanBased/FullscreenOnly/SupportsFogMaps/SupportsDistanceFog` (`:46-49`) — all exist (`UnRenDev.h:46-49`) |
| `SetRes` `:30` / `NullDrv.cpp:38` → `Viewport->ResizeViewport(BLIT_DibSection,Align(NewX,2),NewY,NewColorBytes)` | `:68` pure, same; `BLIT_DibSection` `UnCamera.h:36` | keep |
| `Exit`, `Flush(UBOOL)`, `Lock(FPlane,FPlane,FPlane,DWORD,BYTE*,INT*)`, `Unlock(UBOOL)` | `:69,70,72,73` same | keep |
| `DrawComplexSurface( FSceneNode*, FSurfaceInfo&, FSurfaceFacet& )` `:35` / `NullDrv.cpp:54` | **`:75` `DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD PolyFlags, BYTE cAlpha )=0`** | change signature (else the class stays abstract and `ConstructObject` fails to compile) |
| — | **`:76` `virtual INT MaxVertices()=0;`** | add `INT MaxVertices() { return 0; }` — check what `Render` does with 0 (`grep -n MaxVertices H/Render/Src/*.cpp`); HP2's SoftDrv value is the safe reference (`grep -n MaxVertices H/SoftDrv/Src/*`) — return the same number to avoid a divide-by-zero in a batching loop |
| — | **`:77` `virtual void DrawTriangles( FSceneNode*, FTextureInfo&, FTransTexture** Pts, INT NumPts, _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* )=0`** | add empty stub |
| `DrawGouraudPolygon(…)` `:36` | `:78` **non-pure** now (base has a body) | keep the stub (harmless) or drop |
| `DrawTile(…, FSpanBuffer*, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags)` `:37` | `:79` same | keep |
| `Draw2DLine`, `Draw2DPoint`, `ClearZ`, `PushHit`, `PopHit`, `GetStats(TCHAR*)`, `ReadPixels(FColor*)` `:38-44` | `:82-88` same | keep |
| `Draw3DLine`, `Draw2DClippedLine` | `:80-81` non-pure in HP2 (base impl) — v400 had them non-pure too | nothing |
| `EndFlash`, `DrawStats`, `SetSceneNode`, `PrecacheTexture`, `GetOsSurface(void*,bool)` | `:89-93` non-pure with bodies | nothing (`GetOsSurface` returns NULL by default — the retail SoftDrv/D3D use it for Smacker/screenshots; a null device is fine) |
| `vtblPad0..7` `:96-103` | HP2-only vtable padding | nothing |
| `StaticConstructor` `NullDrv.cpp:19` | `URenderDevice::StaticConstructor` registers the CPF_Config feature flags (`Coronas`, `ShinySurfaces`, …) | keep the empty override; the base class still runs (`IMPLEMENT_CLASS` calls `StaticConstructor` of the concrete class only — check `H/Engine/Src/UnCamera.cpp` `URenderDevice::StaticConstructor` is invoked through `Super` chain by `UClass::…`; the template already relies on this for NullDrv) |
| Includes: `RenderPrivate.h` (`NullDrvPrivate.h:6`) | HP2 has **no** `Render/Src/RenderPrivate.h`; the equivalent is `H/Render/Inc/Render.h` (PLAN §3). `FSceneNode/FSurfaceInfo/FSurfaceFacet/FTransTexture/FSpanBuffer` are declared in `H/Engine/Inc/UnRender.h` (public, `#include "UnRender.h"` from `Engine.h` consumers) | `#include "Engine.h"` + `#include "UnRender.h"` is enough (that is what `SoundDrv/Null.cpp` does); `CMakeLists.txt:22` `../Render/Src` include can go |
| `FTextureInfo` | `H/Engine/Inc/UnTex.h` (FColor gained the `Int4` union, `UBitmap::Lock` takes `FTime`) | NullDrv never dereferences it — no change |

### 3.2 Checklist (NullDrv)

- [ ] `NullDrvPrivate.h:23` 4-arg `DECLARE_CLASS`.
- [ ] `NullDrvPrivate.h:35` + `NullDrv.cpp:54`: `DrawComplexSurface( FSceneNode*, FSurfaceInfo&, FSurfaceFacet&, DWORD, BYTE )`.
- [ ] add `INT MaxVertices()` and `void DrawTriangles(…)` (decl + empty defs).
- [ ] `#include "RenderPrivate.h"` → `#include "Engine.h"` / `"UnRender.h"`; drop the `../Render/Src` include dir.
- [ ] `H/CMakeLists.txt:353-357` already adds `NullDrv` under `BUILD_NULLDRV`.

Verify: `Game -NULLRENDERER -log` ticks the startup map for 30 s with no `Critical:` (PLAN acceptance B).

---

## 4. SoundDrv — the null audio subsystem for HP2's `UAudioSubsystem`

Template `T/SoundDrv/Null.cpp` (64 lines) implements v400's interface (`:35-48`). HP2's interface
(`H/Engine/Inc/UnAudio.h:52-83`) is a different contract; a new class body is needed. Every engine call site
(the contract the stub must honour) — from `grep -rn "Audio->" H/Engine/Src`:

| Engine call | Site | Stub must |
|-------------|------|-----------|
| `Audio = ConstructObject<UAudioSubsystem>(AudioClass); if(!Audio->Init()) { delete Audio; Audio=NULL; }` | `UnEngine.cpp:74-84` (`ini:Engine.Engine.AudioDevice`, skipped with `-NOSOUND` or `UseSound=False`) | `Init()` → `return 1` |
| `Audio->Exec(Cmd,Ar)` | `UnEngine.cpp:233` | `return 0` |
| `Audio->SetViewport(Viewport)` / `SetViewport(Audio->GetViewport())` / `SetViewport(NULL)` | `UnGame.cpp:240,682,878,1415`, `UnCamera.cpp:362-363` | store/return the pointer |
| `Audio->Update(Frame)` (`FSceneNode*`) | `UnGame.cpp:1549` (inside `Viewport->Lock`, after `Render->CreateMasterFrame`) | no-op |
| `Audio->PostRender(Frame)` | `UnGame.cpp:1581` | no-op |
| `Audio->CleanUp()` | `UnGame.cpp:1166` (after `LoadMap`'s garbage collect) | non-pure in the base (`UnAudio.h:73`), no-op |
| `Audio->NoteDestroy(Actor)` | `UnLevAct.cpp:345` | no-op |
| `Audio->UnregisterSound(this)` (Id defaults −1) | `UnAudio.cpp:197` (`USound::Destroy`) | no-op |
| `Audio->PlaySound( Actor, Id, Sound, Loc, Volume, Radius, Pitch, Flags, 0.f )` returns `INT` | `UnScript.cpp:480` (`execPlaySound`; note `Flags = Sound->CoreFlags | SF_No3D | SF_Looping` as computed at `:471-479`) | return `-1` (ALAudio's "could not play", `ALAudioSubsystem.cpp:717,727`). The value is not stored by script (`PlaySound` is `native(264)` returning nothing), so any INT works; `-1` is the honest one |
| `Audio->PlayMusic( Song, FadeInTime )` returns `INT SongHandle` | `UnScript.cpp:1203` → script `MusicTrigger.uc:24` stores it and later calls `StopMusic(SongHandle,…)` | return `0` (ALAudio returns 0 on failure, `:1162`; `StopMusic` bounds-checks `SongHandle-1` at `:1176`) |
| `Audio->StopMusic( SongHandle, FadeOutTime )` → `UBOOL` | `UnScript.cpp:1218` | `return 0` |
| `Audio->StopAllMusic( FadeOutTime )` → `INT` | `UnScript.cpp:1230` | `return 0` |
| `Audio->ModifySound( this, Id, Sound, Param, Value )` → `UBOOL` | `UnScript.cpp:1386` | `return 0` |
| `Audio->StopSound( this, Id, Sound, FadeOutTime )` → `UBOOL` (4-arg overload) | `UnScript.cpp:1406` | `return 0` |
| `StopSound( Actor, Sound )` (2-arg), `RegisterSound(USound*)`→`UBOOL`, `RenderAudioGeometry(FSceneNode*)`, `GetViewport()` | pure virtuals, no engine caller today | stub anyway (`RegisterSound` → `return 0`) |

The complete replacement for `Null.cpp:26-50`:

```cpp
class DLL_EXPORT UNullAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS(UNullAudioSubsystem, UAudioSubsystem, CLASS_Config, SoundDrv)
	// UAudioSubsystem interface (HP2 build 433, Engine/Inc/UnAudio.h:58-81).
	virtual UBOOL Init() { return 1; }
	virtual void  SetViewport( UViewport* Viewport ) { AudioViewport = Viewport; }
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog ) { return 0; }
	virtual void  Update( FSceneNode* SceneNode ) {}
	virtual UBOOL RegisterSound( USound* Sound ) { return 0; }
	virtual void  UnregisterSound( USound* Sound, INT Id=-1 ) {}
	virtual INT   PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch, INT Flags, FLOAT FadeDuration, FLOAT Priority=0.f ) { return -1; }
	virtual UBOOL StopSound( AActor* Actor, USound* Sound ) { return 0; }
	virtual INT   PlayMusic( FString Song, FLOAT FadeInTime ) { return 0; }
	virtual UBOOL StopMusic( INT SongHandle, FLOAT FadeOutTime ) { return 0; }
	virtual INT   StopAllMusic( FLOAT FadeOutTime ) { return 0; }
	virtual void  NoteDestroy( AActor* Actor ) {}
	virtual UViewport* GetViewport() { return AudioViewport; }
	virtual void  CleanUp() {}
	virtual void  PostRender( FSceneNode* Frame ) {}
	virtual UBOOL ModifySound( AActor* Actor, INT Id, USound* Sound, BYTE Parameter, FLOAT Value ) { return 0; }
	virtual UBOOL StopSound( AActor* Actor, INT Id, USound* Sound, FLOAT FadeOutTime=0.f ) { return 0; }
	virtual void  RenderAudioGeometry( FSceneNode* Frame ) {}
private:
	UViewport* AudioViewport = NULL;
};
```

Notes:
- `USound::Audio` (`UnAudio.h:155`, static) is set by the **real** driver (`ALAudioSubsystem.cpp:262`); the null one must leave it `NULL` — `USound::Destroy` (`UnAudio.cpp:196`) and the `FSoundData::Load` comments guard on it.
- `NO_DEFAULT_CONSTRUCTOR(UAudioSubsystem)` (`UnAudio.h:55`) is a protected `{}` ctor — the derived class's implicit default ctor is fine (the template did the same).
- Keep `IMPLEMENT_CLASS(UNullAudioSubsystem); IMPLEMENT_PACKAGE(SoundDrv);` (`Null.cpp:54,60`). Includes: `Engine.h` + `UnRender.h` (for `FSceneNode`).
- **Link hazard outside the driver (Phase A/Core):** HP2 declares `class CORE_API FFileStream` (`H/Core/Inc/Core.h:376-400`) and `GFileStream` (`:468`), allocates it unconditionally in `appInit` (`H/Core/Src/UnMisc.cpp:1996-1997`, sized from `[ALAudio.ALAudioSubsystem] Channels`), but implements `Read/Create/Destroy` and the worker thread **only in `UnVcWin32.cpp:667-991`**. `UnMisc.cpp:1611-1760` (`Init/Destroy/Enter/Leave/CreateStream/DestroyStream`) is portable and will reference `Create/Destroy/Read`. On GCC targets Core will not link until those three get portable definitions — recommended: a `Core/Src/UnFileStreamUnix.cpp` with `Read/Create/Destroy` returning `false` (no thread; the NOpenALDrv plan in §5.6 does not use `GFileStream` at all). `ALAudio` is the only consumer (`grep -rn GFileStream H/` → `ALAudio/Src/ALAudioSubsystem.cpp:364,389,432,470,488,540,652,670,1468,1485,1489,1506`).
- `H/Core/Src/AudioMutex.cpp` / `Core/Inc/AudioMutex.h` (Win32 `CRITICAL_SECTION` retyped by hand) have **zero users** (`grep -rln "audioFileMutex\|AudioMutexGrabber"` → only the two files themselves). Do not compile them; nothing to port.
- `GAudioMaxRadiusMultiplier=100.f` / `GAudioDefaultRadius=450.f` are Core globals (`H/Core/Src/Core.cpp:173-174`) — available to any driver.
- `appPauseMusic/appResumeMusic` are defined in `ALAudioSubsystem.cpp:1807-1817` and referenced nowhere in Core/Engine/Launch → the null build has no dangling symbol.

Verify: with `AudioDevice=SoundDrv.NullAudioSubsystem`, the log shows no `Audio initialization failed.`, the intro map plays, and a `MusicTrigger` does not crash (calls `PlayMusic` → 0 → `StopMusic(0,…)`).

---

## 5. NOpenALDrv — from the template's pattern to HP2's audio

### 5.1 What the template driver is (inventory)

`T/NOpenALDrv/NOpenALDrvPrivate.h` (167) + `NOpenALDrv.cpp` (1155) + `CMakeLists.txt` (89: libxmp
FetchContent `:16-47`; OpenAL = the `OpenAL` target from `cmake/OpenALSoft.cmake` on web/native `:49-56`,
`OpenALMingw` on Windows `:57-61`).

| Aspect | Template | HP2 equivalent |
|--------|----------|----------------|
| Class | `DECLARE_CLASS(UNOpenALAudioSubsystem, UAudioSubsystem, CLASS_Config)` `Private.h:51`; v400 interface `:79-92` (`Update(FPointRegion,FCoords&)`, `RegisterMusic/UnregisterMusic(UMusic*)`, `PlaySound(...)→UBOOL`, `GetLowQualitySetting`) | `UnAudio.h:58-81` — §4 table. No `UMusic` (`UnObj.h:19` "TG ALPHA class UMusic" commented out); music is `PlayMusic(FString Song,FLOAT FadeIn)` |
| Config props | `StaticConstructor` `NOpenALDrv.cpp:112-132` registers `OutputRate, MusicVolume, SoundVolume, MasterVolume, AmbientFactor, DopplerFactor, UseReverb, UseHRTF, MusicInterpolation` (section `[NOpenALDrv.NOpenALAudioSubsystem]`, template web ini `:169-178`) | ALAudio registers `UseEAX, CompatibilityMode(UseMMSYSTEM), UsePrecache, ReverseStereo, Channels(MaxChannels), MusicVolume, SoundVolume, DopplerFactor, RollOff` (`ALAudioSubsystem.cpp:46-59`; ini `Default.ini:149-158`, volumes are **floats 0..1**) — the HP2 menu (`HGame` options) writes these names; keep HP2's names, add `UseHRTF`/`OutputRate` |
| Device/context | `alcOpenDevice(NULL)`, `ALC_FREQUENCY/ALC_HRTF_SOFT/ALC_OUTPUT_MODE_SOFT` attrs `:190-197`; EFX resolved at run time `:74-105,210-223`; `AL_LINEAR_DISTANCE_CLAMPED`, `AL_METERS_PER_UNIT` `:234-237` | ALAudio: `alcOpenDevice("DirectSound3D"/"DirectSound"/"MMSYSTEM")` `:161-162` (OpenAL **1.0** device names — must become `NULL`), `ALC_FREQUENCY 44100` `:169`, `AL_INVERSE_DISTANCE_CLAMPED` `:221`, EAX **already disabled** (`#if 1 alEAXGet=alEAXSet=NULL` `:229-252`; `SetI3DL2Listener` body is `#if 0` `:1941-1993`) |
| Sources | fixed `MAX_SOURCES 64` `Private.h:14`; per-voice struct `FNVoice` `:134-148`; `Voice->Looping = SOUND_SLOT_IS(Id,SLOT_Ambient)` `:678` | `MAX_AUDIOCHANNELS 32` `ALAudioSubsystem.h:27`, `TArray<ALSource> Sources` sized by `Channels` ini `:194-205`; `ALSource` has fade state, `Flags`, `LipSyncAnimMgr` `:87-108` |
| Sound data | `RegisterSound` `:466-511`: `FWaveModInfo::ReadWaveInfo(Sound->Data)` → `alBufferData` → `Sound->Handle=(void*)Buf` → `Sound->Data.Empty()` | `USound::Handle` is **`INT`** (`UnAudio.h:152`), 1-based index into `Buffers` (`ALAudioSubsystem.cpp:588-592`); data is a lazy array `Load()/Unload()` (`:550,582`); streams (`SF_Streaming`) go through `Sound->AddStreamHandle/BindStream/GetHandle(Id)` (`UnAudio.h:209-225`) |
| Music | libxmp module from `UMusic::Data`, 4 queued buffers pumped from `Update()` (`:976-1015`), transitions driven by `Viewport->Actor->Song/SongSection/Transition` (`:902-971`) | `PlayMusic(Song,FadeIn)`: `new USound("..\\music\\<Song>.ogg", SF_Streaming\|SF_Music)` `:1136-1151` → `RegisterSound` (vorbis stream) → `PlaySound(NULL, 2*SLOT_None\|1, …, SF_Streaming\|SF_No3D\|SF_Music, FadeIn)`; returns source index+1 `:1156-1158`. `APlayerPawn` still has `Song/Transition` (`EngineClasses.h:1935-1936`) but nothing in HP2 drives music through them — `MusicTrigger.uc` uses the natives |
| Listener/ambience | reads `Viewport->Actor->XLevel->Actors`, `AmbientSound`, `WorldSoundRadius()`, `SoundVolume/SoundPitch`, `LightType/LightBrightness`, `Region.Zone->bReverbZone/MasterGain` (`:812-868,1047,1070`) | all fields exist in HP2 (`EngineClasses.h:850,941-944,960-962,2669-2673`; `AActor.h:35`); ALAudio additionally uses `ViewTarget` (`:1283`), `Level->Model->PointRegion` (`:1286`), `Pauser` (`:1295`), `IsAudibleAt/CalculateRadiusMultiplier` (`UnLevel.h:220-221`), `SoundOcclusion` (`:944`), `IsOwnedBy` (`:1363`) |
| Threading | none: `FMutex/FScopedLock` are no-ops (`Private.h:38-47`), music decoded in `Update()` | `GFileStream` background thread (`UnVcWin32.cpp:667-704`, `CreateThread :1489`) fills stream chunks; ALAudio polls `ChunksRemaining/RequestChunks` per frame (`:1485-1506`) |

Therefore the template contributes: the CMake (openal-soft fork + web backend), the EFX-at-runtime
pattern, the HRTF/output-mode attribute list, the "register on first play" fallback (`:632-633`), the
config-property registration idiom, and the single-threaded stream pump. **The game logic comes from
`ALAudio`.**

### 5.2 How HP2 stores sounds (`USound`) — from headers, code and bytes

`USound` (`UnAudio.h:131-232`): `FSoundData Data` (a `TLazyArray<BYTE>`), `FLipSyncData LipSyncData`
(also lazy), `OriginalSize`, `Duration`, `raw_NumSamples/raw_BitsPerSample/raw_NumChannels/raw_SampleRate`,
`OpenStreams`, `PlayingStreams`, `INT Handle`, `RefCount`, `FName FileType`, `FString Filename`,
`INT CoreFlags`, `INT WorkingFlags`. Serialisation (`UnAudio.cpp:150-191`): `FileType, CoreFlags, Duration`,
then `raw_NumSamples` if `Ver()>76`, `raw_BitsPerSample, raw_NumChannels` if `>77`, `raw_SampleRate` if
`>78`, then `Data` (lazy: seek-pos + array) and `LipSyncData` if `CoreFlags & SF_HasLipSync`. The shipped
packages are file version **79** (`Assets/System/Core.u` bytes 4-5 = `0x004f`, licensee 1), so every field is
present.

`ESoundFlags` (`UnAudio.h:26-39`): `SF_Looping 2, SF_Streaming 4, SF_Music 8, SF_No3D 16, SF_UpdatePitch 32,
SF_NoUpdates 64, SF_HasLipSync 128, SF_Compressed 256`.

**Measured on every `.uax` in `Assets/Sounds`** (29 packages, 3289 `Sound` exports, parser in the session
scratchpad `uax.py`; re-runnable — it only needs Python):

| FileType | CoreFlags | Count | Bytes | Meaning |
|----------|-----------|-------|-------|---------|
| `XA` | 260 = `SF_Compressed\|SF_Streaming` | 626 | | compressed SFX/ambience |
| `XA` | 388 = `SF_Compressed\|SF_HasLipSync\|SF_Streaming` | 2368 (all in `AllDialog.uax`) | | compressed dialog with lip-sync amplitude track |
| `wav` | 0 | 218 | | plain RIFF WAVE, PCM |
| `wav` | 128 = `SF_HasLipSync` | 77 (`AllDialog.uax`) | | plain dialog with lip-sync |
| **total XA** | | **2994** | **121,218,855** | |
| **total wav** | | **295** | **6,148,830** | 293× mono/22050/16-bit, 2× mono/22050/8-bit (`BioRifle.uax`) |

No sound has `SF_Looping` in `CoreFlags` (looping comes from the `Loop` arg of `PlaySound` → `SF_Looping`,
`UnScript.cpp:478`, or from a WAV `smpl` chunk → `SampleLoopsNum` → `SF_Looping`, `ALAudioSubsystem.cpp:556`).
Whole packages are XA-only (`Adv11_COS, Ch1Rictusempra, Dueling, Exec_demo_level_SFX, Hub1_sfx,
Music_Events, PrivetDr`) — **no audio at all without the XA decoder**.

**The XA bytes.** For all 2994 XA sounds: `raw_BitsPerSample=16, raw_NumChannels=1, raw_SampleRate=22050`,
and `Data.Num() / raw_NumSamples = 0.5357 = 15/28` (2992 exact, 2 at 0.537 from end-of-file rounding). The
first byte of every sampled sound has a high nibble in `0..3` (e.g. `17 e1 34 0d`, `23 11 fc 35`,
`0c 00 00 00`). That is the **EA-XA ("EAXA_BLK") ADPCM block**: 15 bytes → 28 samples, byte 0 = `(coef_index<<4) | shift`,
bytes 1..14 = 28 nibbles, 4 predictor pairs `{0,0},{240,0},{460,-208},{392,-220}` (/256), no container, no
per-sound header. This matches what produced it (`USound::CompressXA`, `UnAudio.cpp:498-624`:
`"sx -eaxa_blk -raw %s -=%s"` at `:580`, then `appLoadFileToArray(Data, "<name>.XA")`, `CoreFlags |=
SF_Compressed|SF_Streaming; FileType="XA"` `:617-618`; sounds under 32 KB are left as WAV `:513-518`), and
what consumes it (`CEAXABLKDec::SetState({0,0}); Feed(Data, Num, NumSamples); Decode(short*[], n)`,
`UnVcWin32.cpp:899-914,773-823`; `coda.h:173-189`). There are **no** `SCHl/EACS/1SNh/MPEG/OggS` magics in any
`.uax` — the MP2 toolchain in `Thirdparty/MPeg` and the `TooLame` line commented out at `UnAudio.cpp:579`
were never used for shipped content; PLAN §7.1's "most likely MPEG-1 Layer II" guess is **wrong**, it is EA-XA.

Open decoders for exactly this format: ffmpeg `libavcodec/adpcm.c` cases `AV_CODEC_ID_ADPCM_EA_R1/R2/R3`
and `AV_CODEC_ID_ADPCM_EA_MAXIS_XA` (LGPL) and vgmstream `src/coding/ea_xa_decoder.c` `decode_ea_xa_int()`
("EA-XA v2 mono/interleave", ISC). Either is ~50 lines; vgmstream's is the better licence fit. **Verify before
committing to it:** decode one short sound (e.g. `Adv11_COS.flame_shoot1`, 55,466 samples, 29,715 bytes) with
the ported routine and check (a) it consumes exactly `ceil(NumSamples/28)*15` bytes, (b) the peak is
< 32767 with no wrap-around, (c) the waveform's RMS envelope resembles the sound's name. A wrong coefficient
table produces audible buzzing, not silence — easy to hear in the first test.

`FWaveModInfo` (`UnAudio.h:313-374`, `ReadWaveInfo(TArray<BYTE>&)`) parses the RIFF sounds (also gives
`SampleLoopsNum/pSampleLoop` for looping) — reuse it unchanged for the 295 WAVs.

**Lip sync.** `FLipSyncData` (`UnAudio.h:105-125`, impl `UnAudio.cpp:1389-1509`) is a byte amplitude every
20 ms (`fAmpSampleInterval=0.020`, `:15`); `GetMouthPos(fTime)` returns a normalised mouth openness. The driver
side (`FLipSyncAnimMgr`, `ALAudioSubsystem.h:62-81`, `.cpp:2142-2221`) is small: on `PlaySound` with
`SF_HasLipSync && Actor` it `CreateAnimChannel(AAnimChannel::StaticClass(), AT_Replace, "mouth_root", …)`,
`PlayAnim("mouth_anim")`, then every 50 ms sets `AnimFrame = LipSyncData->GetMouthPos(elapsed)`
(`:2193-2210`); on stop it `PlayAnim("mouth_rest")` (`:2154-2157`); paused time is eaten via
`EatPauseTime` (`:1452`). It needs `AActor::CreateAnimChannel` (native, `EngineClasses.h:1067,3854`) and the
`AAnimChannel` class (`:1387`) — both HP2 engine, both fine to call from the driver. **Port as-is.** 2445 of
3289 sounds carry lip-sync data, so this is not optional for dialog scenes.

**Music.** 111 `.ogg` files, 59 MB, in `Assets/Music/` (magic `OggS` verified). Opened by `PlayMusic`
as `"..\\music\\" + Song [+ ".ogg"]` (`ALAudioSubsystem.cpp:1136-1140`) relative to the `System/` cwd through
`fopen` + `ov_open(FILE*)` (`UnVcWin32.cpp:867-869`). Two portability bugs to fix in the port: the
backslashes and the **lower-case `music`** (the directory is `Music`; case-sensitive on Linux/wasm). `Paths=`
is irrelevant for these (they are not Unreal packages) but the frontend's file manifest must include
`Music/*.ogg` (PLAN §6.8 `mods.extToFolder` already maps `.ogg Music`).

**Streaming.** ALAudio never loads a whole XA sound into an AL buffer: `RegisterSound` creates an
`FFileStream` stream (`ST_XA`/`ST_XALooping` for compressed, `ST_Ogg`/`ST_OggLooping` for music,
`Core.h:351-358`), primes `MAX_BUFFERS_PER_STREAM=3` buffers of `XA_STREAM_CHUNKSIZE=32768` /
`OGGVORBIS_STREAM_CHUNKSIZE=65536` bytes (`ALAudioSubsystem.h:40-44`), queues them, and refills one per
processed buffer from `Update` (`:1458-1545`). The background thread only decodes into `Stream.Data`
(`UnVcWin32.cpp:667-704`); the AL calls stay on the game thread. With 32 sources and 32 streams max
(`MAX_AUDIOSTREAMS`), memory is bounded (~3×32 KB per voice).

**Pause.** `Update` pauses/unpauses all sources on realtime/pause transitions (`:1299-1310`, `PauseSounds`
`:1208`); `Exec` handles `PAUSESOUNDS/UNPAUSESOUNDS/RESTARTSOUNDS/ROLLOFF/PLAYINGSOUNDSTATS` (`:1701-1799`).

**Occlusion.** `SoundPriority` (`:1894-1909`) and the radius smoothing (`:1615-1647`) call
`Level->IsAudibleAt` and `CalculateRadiusMultiplier` — HP2 engine, keep.

### 5.3 Where `coda` and the other closed audio bits are touched

| Symbol | Only user | Action |
|--------|-----------|--------|
| `coda.h`: `CEAXABLKDec`, `XA16STATE`, `CODASetNew/CODASetDelete` | `H/Core/Src/UnVcWin32.cpp:659,773-823,899-914,969,1482-1486` (+ `Core.dsp`) | never compiled on GCC targets; replaced by the open decoder inside NOpenALDrv (§5.6) |
| `coda.h` MPEG/VAG/GC/Xbox classes (`:57-105,222-306,554-660,772-1124`) | none | unused |
| `Thirdparty/OpenAL/Inc/{al,alc,eax}.h` (1.0 SDK, `ALubyte*` strings, `alcOpenDevice(unsigned char*)`) | `ALAudioSubsystem.h:16-18` | not used; openal-soft's `AL/al.h` etc. via the `OpenAL` target |
| `Thirdparty/Vorbis` (headers + MSVC libs) | `ALAudioSubsystem.cpp:18-19`, `UnVcWin32.cpp:657-658` | replaced by FetchContent `xiph/ogg` + `xiph/vorbis` (static; `vorbisfile` builds fine under emcc and gcc -m32) |
| `Thirdparty/MPeg`, `Thirdparty/ENCVAG` | `Engine/MPeg` tooling, `HAVE_VAG 0` | unused |

### 5.4 The interface the port must implement, member by member

All pure virtuals of `H/Engine/Inc/UnAudio.h:58-81` (the §4 stub list) with ALAudio's semantics:

| Method | ALAudio impl to port | Notes for the port |
|--------|----------------------|--------------------|
| `Init()` | `:154-268` | `alcOpenDevice(NULL)`; template attr list (`:190-195`) for HRTF; `alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED)`; `MaxChannels` from ini (`Channels=32`); `USound::Audio=this`; drop the `UseMMSYSTEM/UseEAX` device-name switch |
| `SetViewport` | `:274-300` | `UsePrecache` registers every `USound` — leave it off (web memory) |
| `RegisterSound(USound*)→UBOOL` | `:315-600` | three branches: ogg stream / XA stream / RIFF buffer. Replace `GFileStream->CreateStream/RequestChunks/…` with an in-driver `FStreamSlot {decoder state, chunk buffer, eof}` decoded synchronously on demand |
| `UnregisterSound(USound*,INT Id)` | `:609-693` | ref-counted; `OldStopSound` + stream teardown |
| `PlaySound(…,INT Flags,FLOAT FadeDuration,FLOAT Priority)→INT` | `:700-921` | returns the source index (0-based) — `PlayMusic` depends on it (`:1156-1158`); the `SLOT_Talk` priority boost `:748-751` matters for dialog |
| `StopSound(Actor,Sound)` | `:925-968` | |
| `StopSound(Actor,Id,Sound,FadeOut)` | `:979-1017` | fade-out via `FADE_Out` |
| `ModifySound(Actor,Id,Sound,Param,Value)` | `:1025-1095` | `SOUND_Volume/Radius/Pitch` (`EngineClasses.h:126-128`) |
| `PlayMusic(FString,FLOAT)→INT` | `:1127-1164` | fix the path (§5.2); the `USound` is heap-`new`ed with `Filename`, `SF_Streaming\|SF_Music`, `FileType="ogg"` |
| `StopMusic(INT,FLOAT)→UBOOL`, `StopAllMusic(FLOAT)→INT` | `:1167-1205` | |
| `NoteDestroy(AActor*)` | `:1105-1122` | |
| `GetViewport()` | `:305-310` | |
| `CleanUp()` | — (base no-op) | ALAudio does not override; fine |
| `PostRender(FSceneNode*)` | `ALAudioSubsystem.h:244` `{}` | no-op |
| `RenderAudioGeometry(FSceneNode*)` | `:1098-1102` `{}` | no-op |
| `Update(FSceneNode*)` | `:1238-1699` | the whole per-frame loop; listener from `SceneNode->Coords` (`:1250-1276` — keep the non-EAX branch); replace the `GFileStream` polling (`:1458-1545`) with the synchronous refill |
| `Exec` | `:1701-1799` | keep; add the template's nothing (its `MusicOrder/MusicInterp` are libxmp-specific) |
| `StaticConstructor` | `:46-59` | keep HP2's property names (the HGame options menu reads/writes them); add `UseHRTF`, `OutputRate` from the template `:122-130` |
| `PostEditChange`, `Destroy`, `ShutdownAfterError` | `:69-149` | `Destroy` iterates `TObjectIterator<USound>` to unregister — keep |

### 5.5 Everything else the template's file does that HP2 does not want

- `RegisterMusic/UnregisterMusic/PlayMusic()/StopMusic()`, libxmp, `MusicSection/Transition` logic
  (`NOpenALDrv.cpp:416-463,739-769,902-971,976-1035,1123-1152`) — drop; there is no `UMusic` and no `.umx`.
- `GetLowQualitySetting` (`Private.h:89`) — not in HP2's interface (and `FSoundData::Load` has the call
  disabled at `UnAudio.cpp:45-46`).
- `Update(FPointRegion,FCoords&)` signature — HP2 passes `FSceneNode*` (`UnGame.cpp:1549`).
- The `FMutex` no-ops (`Private.h:38-47`) — keep the idea (single-threaded), not the names.
- Distance constants `DISTANCE_SCALE 0.023255814f` (`Private.h:29`) vs ALAudio's `DISTANCE_FACTOR (0.0254f/2.f)` (`.h:53`) — use ALAudio's; the game's radii were tuned against it.

### 5.6 The plan (Phase E), concretely

1. **CMake** (`H/NOpenALDrv/CMakeLists.txt`): keep lines `:49-68` (OpenAL selection incl. the fork on
   web/native, `OpenALMingw` on Windows) and `:70-89`; replace the libxmp block (`:16-47`) with
   `FetchContent_Declare(ogg GIT_REPOSITORY https://github.com/xiph/ogg GIT_TAG v1.3.5)` +
   `FetchContent_Declare(vorbis GIT_REPOSITORY https://github.com/xiph/vorbis GIT_TAG v1.3.7)` (both have
   CMake; set `BUILD_SHARED_LIBS OFF`, `BUILD_TESTING OFF`; vorbis needs `OGG_INCLUDE_DIR/OGG_LIBRARY` or
   `Ogg::ogg` from the ogg build — ogg ≥1.3.5 exports the target). Link `vorbisfile vorbis ogg`. The top-level
   `H/CMakeLists.txt:365-373` already includes `OpenALSoft.cmake` and adds the subdirectory under
   `BUILD_NOPENALDRV` (forced ON for Emscripten at `:70`).
2. **Decoder**: `Source/NOpenALDrv/EaXaDecoder.{h,cpp}` — a port of vgmstream `decode_ea_xa_int()` (mono,
   15-byte frames, state = two previous samples), API `struct FEaXa { INT16 Prev1,Prev2; const BYTE* Src;
   INT Remaining; INT DecodeSamples(INT16* Dst, INT N); }`. Unit-check per §5.2.
3. **Driver**: `NOpenALDrv.cpp` = `ALAudioSubsystem.cpp` with: class renamed `UNOpenALAudioSubsystem`
   (`DECLARE_CLASS(…,UAudioSubsystem,CLASS_Config,NOpenALDrv)`), OpenAL 1.1 API (`alcOpenDevice(NULL)`,
   `const ALchar*`), openal-soft headers, the template's `Init` attribute list + EFX resolution
   (`NOpenALDrv.cpp:74-105,190-232`) kept for later reverb (HP2's `AZoneInfo` has `bReverbZone/MasterGain` and
   an `UI3DL2Listener ZoneEffect`, `EngineClasses.h:2669-2673,3710` — the template's `UpdateReverb`
   `:1040-1083` can be dropped in as a later enhancement), `FLipSyncAnimMgr` verbatim, and an in-driver
   stream table replacing `GFileStream`: `RegisterSound` creates the slot (XA: `FEaXa` over `Sound->Data`
   after `Data.Load()`; ogg: `ov_open`/`ov_read` — `ov_open` on a `FILE*` is fine on the web since the
   file manager is POSIX + FetchFS), primes 3 buffers; `Update` refills processed buffers synchronously
   (32 KB chunks = ~0.74 s at 22050 mono 16-bit; 3 buffers ≈ 2 s of cushion, ample for a 60 Hz pump).
   Keep `LoadConfig()` at `Init` (template `:159`) so the ini volumes apply.
4. **Config**: `Config/System/web/Default.ini` `AudioDevice=NOpenALDrv.NOpenALAudioSubsystem` +
   `[NOpenALDrv.NOpenALAudioSubsystem]` with HP2's names (`Channels=32 MusicVolume=0.5 SoundVolume=0.9
   DopplerFactor=1.0 RollOff=1.0 UseHRTF=False OutputRate=44100`); the launcher's `NormalizeSubsystem`
   preference list (template `Launch.cpp:542-547`) as `{NOpenALDrv.NOpenALAudioSubsystem,
   SoundDrv.NullAudioSubsystem}` so `ALAudio.ALAudioSubsystem` in the shipped ini falls through.
   `H/Core/Src/UnMisc.cpp:1996-1997` reads `[ALAudio.ALAudioSubsystem] Channels` only to size the (unused)
   `GFileStream` — harmless.
5. **Web specifics inherited from the template**: `Module.alsoftConfig`, HRTF as a player choice, no EAX
   symbols linked, mixer on a pthread + AudioWorklet (the openal-soft fork) — nothing HP2-specific.

Acceptance (PLAN §7): menu music (`Intro_Movie_Music_Edit.ogg`, a `MusicTrigger` in the startup map) and a
spell SFX (XA) audible natively and on the web; a dialog line moves the mouth (lip sync).

---

## 6. IpDrv

### 6.1 What the template's IpDrv is

epic v400 `IpDrv/` plus ut99dc/ufront hunks — hunk counts vs `E/IpDrv/`: `IpDrvPrivate.h` 12,
`UnSocket.h` 4, `UnSocket.cpp` 6, `TcpNetDriver.cpp` 7, `TcpLink.cpp` 18, `UdpLink.cpp` 16,
`UMasterServerCommandlet.cpp` 5, `UUpdateServerCommandlet.cpp` 1, `IpDrvClasses.h` 2, `AInternetLink.h` 1;
`IpDrv.cpp`, `InternetLink.cpp`, `ATcpLink.h`, `AUdpLink.h` unchanged. The hunks are: platform-not-compiler
socket selection (`IpDrvPrivate.h:14-23` `PLATFORM_WIN32→__WINSOCK__` else `__BSD_SOCKETS__`; `ws2tcpip.h`
`:33`; pthread resolver `:109-133`), `HasSocket()` (`AInternetLink.h:18-21`, used at every `GetSocket()` test
in `TcpLink.cpp`/`UdpLink.cpp`), `INVALID_SOCKET` instead of 0, `socklen_t`, `EINPROGRESS` on non-blocking
connect (`TcpLink.cpp:479-490`), the musl duplicate-case guard (`UnSocket.cpp:226-234`), the
**`ReceiveModePad[3]`** layout fix (`IpDrvClasses.h:61-70`), Dreamcast blocks (`UnSocket.h:52-76`,
`IpDrvPrivate.h:198-241`, `TcpNetDriver.cpp:219-259,357-369`, `UdpLink.cpp:82-86`, `UnSocket.cpp:24-37`),
and the WebRTCLan hooks (`UdpLink.cpp:13,38-40,56-73,138-145,178-182,207-230`).

### 6.2 Engine API inventory

| Class/API | Used at | HP2 |
|-----------|---------|-----|
| `UNetConnection( UNetDriver*, const FURL& )`, `State`, `MaxPacket`, `PacketOverhead`, `InitOut()`, `URL`, `Driver->ServerConnection->State`, `Driver->SendCycles`, `ReceivedRawPacket(void*,INT)`, `CreateChannel(CHTYPE_Control,1,0)`, `CurrentNetSpeed`, `USOCK_Pending/Open/Closed`, `LowLevelSend/LowLevelGetRemoteAddress/LowLevelDescribe` | `TcpNetDriver.cpp:52-64,93-150,189-193` | identical signatures (`H/Engine/Inc/UnConn.h:150,64-72,166-186`; the epic→HP2 diff of `UnConn.h` adds `RemoteVersion`, `FDownloadInfo`, `UDownload* Download`, `TArray<FDownloadInfo> DownloadInfo` and retypes 7 timestamps to `FTime`) |
| `UNetDriver::InitConnect/InitListen(FNetworkNotify*,FURL&,FString&)`, `TickDispatch(FLOAT)`, `LowLevelDestroy()`, `LowLevelGetNetworkNumber()`, `ServerConnection`, `ClientConnections`, `Notify`, `RecvCycles` | `TcpNetDriver.cpp:174-338` | identical (`UnNetDrv.h:41-43,59,72-78`); HP2 adds `MaxDownloadSize`, `TArray<FString> DownloadManagers` (`:60-61`) and `Time` is `FTime` |
| `FNetworkNotify::NotifyAcceptingConnection()→EAcceptConnection`, `NotifyAcceptedConnection(UNetConnection*)`, `ACCEPTC_Accept` | `TcpNetDriver.cpp:303-307` | `UnLevel.h:16-30` identical |
| `FURL` (`Host`, `Port`, `Map`, `Portal`, ctor `FURL(FURL*,const TCHAR*,ETravelType)`, `TRAVEL_Absolute`) | `TcpNetDriver.cpp:69-80,185`, `InternetLink.cpp:117-121` | `UnURL.h:21-25,44` identical |
| `AActor::Tick(FLOAT, ELevelTick)`, `Super::Destroy`, `GetLevel()->SpawnActor(Class,NAME_None,this,Instigator,Location,Rotation)`, `Instigator`, `Location`, `Rotation` | `InternetLink.cpp:56-59`, `TcpLink.cpp:203-206,347` | `AActor.h:42` same; `ULevel::SpawnActor` — verify its HP2 signature (`grep -n "SpawnActor" H/Engine/Inc/UnLevel.h`) — 433 may have extra defaulted args (still source-compatible) |
| Script glue: `P_GET_INT_OPTX, P_GET_UBOOL_OPTX, P_GET_STRUCT, P_GET_STR, P_GET_STR_REF, P_GET_INT_REF, P_GET_STRUCT_REF, P_GET_ARRAY_REF(BYTE,B), P_FINISH, RESULT_DECL, FFrame&, Stack.Logf, unguardexec, ProcessEvent, FindFunctionChecked` | `UdpLink.cpp:53-55,135-137,174-177,290-292,327-330`, `InternetLink.cpp:88-256`, `TcpLink.cpp:61-732` | all in `H/Core/Inc/UnScript.h`/`UnStack.h` (433 adds the `EX_DebugInfo` step to `P_FINISH`, PLAN §3 — transparent to callers) |
| `IMPLEMENT_FUNCTION` via `AUTOGENERATE_FUNCTION` (`IpDrv.cpp:19-25`), `FName(TEXT(#name),FNAME_Intrinsic)` (`UnSocket.cpp:70-75`) | | §1.5 — needs the template's `IMPLEMENT_FUNCTION` in `UnScript.h`; `FNAME_Intrinsic` exists in HP2 (`grep -n FNAME_Intrinsic H/Core/Inc/UnName.h`) |
| `UCommandlet` (`DECLARE_CLASS(UMasterServerCommandlet, UCommandlet, CLASS_Transient)` `UMasterServerCommandlet.cpp:26`, `UUpdateServerCommandlet.cpp:39`) | | 4-arg macro; these two commandlets are GameSpy/master-server tooling — **exclude them from `SRC_FILES`** for HP2 (no master server, no GameSpy `gsutils.lib`) |
| GameSpy: `GenerateSecretKey/gs_encrypt/gs_encode` (`InternetLink.cpp:224-234`, `GameSpyClasses.h`) | `execValidate` | keep compiling (the functions are in `GameSpyClasses.h`, header-only) or stub `execValidate` to return `""` |
| Core: `appStrcpy/appStrchr/appAtoi/appSleep/appToAnsi/appFromAnsi/appSprintf`, `Parse(appCmdLine(),"MULTIHOME="/"PORT="/"PRIMARYNET")`, `debugf(NAME_Init/DevNet/Exit)`, `FString::Printf`, `clock/unclock`, `LocalizeError`? (none) | | present |

### 6.3 Adaptation checklist (IpDrv)

- [ ] 4-arg `DECLARE_CLASS` at `TcpNetDriver.cpp:42,163` (`,IpDrv`), `IpDrvClasses.h:89,141,228` (`,IpDrv`), and the two commandlets if kept.
- [ ] `CMakeLists.txt`: drop `UMasterServerCommandlet.cpp`/`UUpdateServerCommandlet.cpp` from `SRC_FILES:4-12` (or keep and 4-arg them; they compile against `FSocketData/FOperationStats` only). Keep `../WebRTCDrv` include (`:22-25`) and `ws2_32` (`:34-36`). The Dreamcast pthread block is already gone in HP2's copy.
- [ ] Delete the `PLATFORM_DREAMCAST` blocks listed in §6.1 (PLAN rule 2) — optional, they are `#ifdef`-dead.
- [ ] `IpDrvClasses.h` is generated from `IpDrv/Classes/*.uc` against **UT99's** `Engine.u`. It only depends on `AInternetInfo` (exists: `H/Engine/Inc/EngineClasses.h:2592`, `Engine/Classes/InternetInfo.uc`) and `FIpAddr` (`IpDrvPrivate.h:62-66`), so the C++ layout is `AInternetInfo` + the listed members regardless of engine — **the header can be kept as is** as long as no `IpDrv.u` is loaded (nothing then checks the script layout). Keep the `ReceiveModePad[3]` (`:70`) if an `IpDrv.u` is ever compiled with HP2's `ucc` — HP2's `UStruct::Link` aligns to `PropertiesAlign` (PLAN §3) so the same MSVC-vs-GCC tail-padding issue applies; the layout guard will say.
- [ ] `TcpNetDriver.cpp:274` `select(Socket+1,…)`, `:278 recvfrom(..., GCC_OPT_INT_CAST &FromSize)` — unchanged; on Emscripten the file compiles but is never selected (WebRTCDrv replaces it, template `Launch.cpp:498`).
- [ ] `UnSocket.cpp:226-234` musl guard — keep.

### 6.4 Do we need `IpDrv.u` at all?

- `UTcpNetDriver`/`UTcpipConnection` (`TcpNetDriver.cpp`) are pure C++ `UNetDriver` subclasses with **no script class** — `StaticLoadClass(UNetDriver::StaticClass(),NULL,"ini:Engine.Engine.NetworkDevice")` finds them through the static registry (§1.6) with no `.u` file. The listen/connect path works without `IpDrv.u`.
- `AInternetLink/ATcpLink/AUdpLink` (`InternetLink.cpp`, `TcpLink.cpp`, `UdpLink.cpp`) **are** script-backed (`IpDrvClasses.h` is their generated mirror); their C++ compiles, but the classes can only be instantiated if `IpDrv.u` exists, and `UdpBeacon.uc`/`ClientBeaconReceiver.uc` (`T/IpDrv/Classes/*.uc`, 5 files + `IpDrv.upkg`) are the only stock users. HP2 ships **no** `IpDrv.u`, `UWeb.u`, `IpServer.u`, `UBrowser.u` (`ls Assets/System/*.u` → `Core Editor Engine Fire HGame HPModels HPParticle HPSounds HProps UWindow UnrealShare`), and no HP2 script references `UdpBeacon`/`InternetLink` (grep `HGame/Classes`).
- The shipped ini still lists `ServerActors=IpDrv.UdpBeacon`, `IpServer.UdpServerQuery`, `IpServer.UdpServerUplink …×3`, `UWeb.WebServer` (`Config/System/Default.ini:69-74`) and `DownloadManagers=IpDrv.HTTPDownload` + `Engine.ChannelDownload` (`:173-174`, section `[IpDrv.HTTPDownload]:176-180`). `UGameEngine` spawns `ServerActors` on listen — with the classes missing that is a `Can't find file for package 'IpDrv'` failure per actor (the template's launcher strips them, `T/…/Launch.cpp:573+`; do the same, PLAN §6.5 "Drop the GameSpy/master-server ServerActors"). `DownloadManagers` is read at `H/Engine/Src/UnConn.cpp:173-175` with `StaticLoadClass(…, LOAD_NoWarn)` → a missing `IpDrv.HTTPDownload` is skipped silently, `Engine.ChannelDownload` (`H/Engine/Src/UnDownload.cpp`) stays. **Keep only `DownloadManagers=Engine.ChannelDownload`** in the web ini to avoid the warning; there is no `UHTTPDownload` in the v400 template IpDrv anyway (that is a v436 class — HP2's ini was written against a 436-era IpDrv it never shipped).
- **Decision:** ship `IpDrv` **without** `IpDrv.u` first (Phase B). Compile `TcpNetDriver.cpp + UnSocket.cpp + IpDrv.cpp` plus `InternetLink.cpp/TcpLink.cpp/UdpLink.cpp` (they cost nothing and keep the option open). Later, if a LAN beacon is wanted, `ucc make` with `EditPackages=IpDrv` appended to `Default.ini:327-337` and the template's 5 `.uc` files: HP2's `UMakeCommandlet` exists (`H/Editor/Src/UMakeCommandlet.cpp`), but the `.uc` files declare `native` functions whose C++ mirror must match the **433** `Actor` layout — `ucc make` generates the header only with `-h`; the existing `IpDrvClasses.h` would then have to be regenerated (or diffed) against HP2's `Engine.u`. Unknown until tried; PLAN §4.6 already says "if it fails, ship without".

---

## 7. WebRTCDrv (+ WebRTCLan)

### 7.1 Inventory

`T/WebRTCDrv/WebRTCDrv.cpp` (482): `IMPLEMENT_PACKAGE(WebRTCDrv)` `:24`; ABI imports as `extern "C"`
(`:48-60`, native no-op inlines `:61-71`) — the same names as `vendor/glue/ue_abi.h:60-69` (vendored in
`H/vendor/glue/ue_abi.h`, `UE_ABI_VERSION 1`); `extern UEngine* GWebRTCEngine` (`:74`, defined by the
launcher, template `Launch.cpp:54`, set at `:595`); `UWebRTCConnection : UNetConnection` (`:91-160`);
`UWebRTCNetDriver : UNetDriver` (`:172-478`). `WebRTCLan.h` (41) / `WebRTCLan.cpp` (602): the synthetic
LAN for the stock server browsers; ABI discovery imports (`:46-57` = `ue_abi.h:70-78`); everything is under
`#ifdef __EMSCRIPTEN__` with no-op natives at `:587-598`.

### 7.2 Engine API inventory

| API | Used at | HP2 |
|-----|---------|-----|
| `UNetConnection` ctor/fields/`InitOut/FlushNet/Channels[0]->Close()/ReceivedRawPacket` | `WebRTCDrv.cpp:101-125,359,379` | identical (`UnConn.h:150,64-72,169,172,182`; `Channels[]` is `UChannel* Channels[MAX_CHANNELS]`, `UChannel::Close()` in `UnChan.h`) |
| `UNetDriver::InitConnect/InitListen/TickDispatch/LowLevelDestroy/LowLevelGetNetworkNumber`, `ConnectionTimeout`, `InitialConnectTimeout`, `ServerConnection`, `ClientConnections`, `Notify` | `:193-197,200-291,292-451` | identical (`UnNetDrv.h:46-47,72-78`) |
| `FNetworkNotify`, `ACCEPTC_Accept/Ignore` | `:326-336` | identical (`UnLevel.h:16-30`) |
| `FURL::Host`, `GetOption(Match,Default)` | `:209,259-260` | `UnURL.h:21,52` identical |
| `GConfig->GetString("Engine.GameReplicationInfo","ServerName")` | `:278` | HP2 ini `Default.ini` has `[Engine.GameReplicationInfo] ServerName=Another UT Server` (verbatim from UT) |
| `UGameEngine::GLevel`, `ULevel::URL.Map`, `GetLevelInfo()->Game`, `AGameInfo::MaxPlayers`, `GameName`, `GameReplicationInfo->ServerName`, `Cast<UGameEngine>` | `:407-425` | all present (`EngineClasses.h:2935-2953,2463`; `UnEngine.h` `UGameEngine`) |
| `appToAnsi`, `appSeconds`, `debugf(NAME_DevNet/Exit)`, `FString::Printf/Len`, `TArray<INT>` | | present; `appSeconds` → §1.2 |
| `emscripten_sleep` | `:444-447` | Asyncify; identical to the template's launcher requirement |
| WebRTCLan: `GConfig->GetString("UBrowser.UBrowserLocalFact","BeaconProduct")`, `("IpDrv.UdpBeacon","BeaconProduct"/"BeaconPort")` `:175-177,382-383`, `ENGINE_VERSION`, `ENGINE_MIN_NET_VERSION` `:350` | | `UnObjVer.h:11,14` = 400/433; the ini sections don't exist in HP2 → defaults ("unreal", 7776) |

### 7.3 Adaptation checklist (WebRTCDrv)

- [ ] `WebRTCDrv.cpp:93` `DECLARE_CLASS(UWebRTCConnection,UNetConnection,CLASS_Config|CLASS_Transient,WebRTCDrv)`; `:174` same for `UWebRTCNetDriver`.
- [ ] `:36` `#define WEBRTC_PEER_PREFIX "hp2"` — must equal `targets/hp2/target.json network.peerPrefix` (PLAN §2 says `"hp2"`).
- [ ] `:399-402` `FTime` (§1.2).
- [ ] `:429` fallback name `"UT Server"` → `"HP2 Server"` (cosmetic, shows in the frontend's server list).
- [ ] `WebRTCLan.cpp`: `DOUBLE` sites (§1.2). It is functionally **inert** on HP2 — no `UBrowser`, no `UdpBeacon`, no `ClientBeaconReceiver`, so `WebRTCLan_Bind/Send/NextReply` are never called by any `AUdpLink` (none can be spawned without `IpDrv.u`). Keep it compiled (PLAN §6.4) because `UdpLink.cpp:13` includes `WebRTCLan.h` and the symbols must link; the discovery imports (`ue_abi.h:70-78`) then stay unreferenced — fine for the ABI parity test, which keys on the header, not on use.
- [ ] `H/CMakeLists.txt:295-297` already adds it under `EMSCRIPTEN`; the launcher (Phase B step 4) must define `UEngine* GWebRTCEngine` and implement `ue_exec_command` (`ue_abi.h:85`) and `webrtc_deferred_open` (`:84`) exactly as the template's `Launch.cpp:52-54,138-141` does.
- [ ] Web ini: `NetworkDevice=WebRTCDrv.WebRTCNetDriver` + the `[WebRTCDrv.WebRTCNetDriver]` block copied from the template's `Config/System/web/Default.ini` (`AllowDownloads=True ConnectionTimeout=15.0 InitialConnectTimeout=500.0 … NetServerMaxTickRate=120 LanServerMaxTickRate=120`) — `UGameEngine::GetMaxTickRate` reads it for a listen host (template note in that ini). Add `DownloadManagers=Engine.ChannelDownload` there too (the driver inherits `UNetDriver`'s config property, `UnNetDrv.cpp:137`).

### 7.4 What to do without a stock server browser

HP2 has no in-game multiplayer UI at all (single-player game; `HGame` has no `ServerBrowser`/`UBrowser`).
Joining/hosting therefore has to come from the **frontend** (the ufrontend "Play" panel → travel URL with
`?room=`/`?ServerName=`, and a join by room → `open <room>` through `ue_exec_command`), exactly how the
UE1 targets already expose "host"/"join" outside the game. `UWebRTCNetDriver::InitListen` reads
`room=`/`ServerName=` from the URL (`:259-260`) and pushes `webrtc_set_listen_status` (`:397-438`) — the
frontend's own server list consumes that, so P2P is usable without any script. Gameplay-side caveat
(outside this report's scope, for the engine/content readers): HP2's `HGame.harry` `GameInfo`/`PlayerPawn`
classes were never networked (`Default.ini` `[Engine.DemoRecDriver] DemoSpectatorClass=HGame.harry`), so "multiplayer" means
co-observing at best until replication is tested. **Do not** plan on `WebRTCLan`; leave it as dead weight.

---

## 8. Ini / config implications (summary for Phase B/D)

| Key | HP2 shipped (`Config/System/Default.ini`) | Web/native port value | Where enforced |
|-----|-------------------------------------------|-----------------------|----------------|
| `[Engine.Engine] ViewportManager` | `WinDrv.WindowsClient` (`:35`) | `NSDLDrv.NSDLClient` | launcher `GConfig->SetString` (template `Launch.cpp:489`) |
| `GameRenderDevice` / `WindowedRenderDevice` | `D3DDrv.D3DRenderDevice` / `SoftDrv.SoftwareRenderDevice` (`:22,31`) | `XOpenGLDrv.XOpenGLRenderDevice` (web), switch table natively | launcher `-*RENDERER` + `NormalizeSubsystem` |
| `AudioDevice` | `ALAudio.ALAudioSubsystem` (`:23`) | `SoundDrv.NullAudioSubsystem` → `NOpenALDrv.NOpenALAudioSubsystem` (Phase E) | launcher preference list |
| `NetworkDevice` | `IpDrv.TcpNetDriver` (`:24`) | native: same; web: `WebRTCDrv.WebRTCNetDriver` | launcher (Emscripten branch) |
| `[Engine.GameEngine] ServerActors` | 6 GameSpy/UWeb/IpDrv actors (`:69-74`) | none | launcher strips (template `:573+`) / web ini omits |
| `[IpDrv.TcpNetDriver] DownloadManagers` | `IpDrv.HTTPDownload`, `Engine.ChannelDownload` (`:173-174`) | `Engine.ChannelDownload` only | web ini |
| `[ALAudio.ALAudioSubsystem]` (`:149-158`) | floats 0..1, `Channels=32` | mirror as `[NOpenALDrv.NOpenALAudioSubsystem]` with the same keys | web ini + `UnMisc.cpp:1996` still reads the old section for `GFileStream` sizing (harmless) |
| `Paths=../Music/*.umx` (`:49`) | there are no `.umx` | leave; `.ogg` is opened directly, not through `Paths` | — |
| `[NSDLDrv.NSDLClient]` | absent | copy the template's block (1280×720, `CaptureMouse=True` on web) | web ini; native falls back to `StaticConstructor` defaults (`NSDLClient.cpp:55-87`) |
| `[WebRTCDrv.WebRTCNetDriver]` | absent | template's block | web ini |

---

## 9. Verification checklist (per driver, log milestones)

- [ ] NSDLDrv: `Game.log` has `NSDLClient: 1280x720, Brightness=0.50, Decals=1, …` and `Opened viewport`; `SetRes 1024x768` works; menu cursor moves with `bShowWindowsMouse`; a spell gesture registers.
- [ ] NullDrv: `-NULLRENDERER` reaches the menu and ticks a map 30 s; `GetStats` returns "".
- [ ] SoundDrv: no `Audio initialization failed.`; `MusicTrigger` maps don't crash.
- [ ] NOpenALDrv: `NOpenALDrv: HRTF inactive (UseHRTF=False), reverb …` line; an XA sound decodes to a sane waveform in a unit test; `Intro_Movie_Music_Edit.ogg` audible; a dialog line lip-syncs.
- [ ] IpDrv (native): `TcpNetDriver on port 7777` on `?listen`; a second native client `open 127.0.0.1` joins (`Received 'HGame'` … `Welcomed`).
- [ ] WebRTCDrv (web): `[WebRTC] InitListen: room='…' name='…'`, `[WebRTC] Accepted connection connId=1`, guest reaches the map.
- [ ] Layout guard prints no `[layout]` line for `AInternetLink/ATcpLink/AUdpLink` **if** an `IpDrv.u` is ever built.

---

## 10. Open questions / things I could not verify without building

1. `MaxVertices()` return value for NullDrv — read `H/Render/Src` consumers first (a zero may be divided by).
2. `ULevel::SpawnActor` exact 433 signature (TcpLink's accept path) — source-compatible in v436; confirm with the compiler.
3. Whether HP2's `UClient::StaticConstructor` registers `ParticleDensity`/`NoFractalAnim` with a range; NSDLClient's defaults must agree with the `HGame` options menu expectations.
4. The EA-XA coefficient/shift convention (`shift = byte & 0x0F`, samples `(nibble << 28) >> (shift + 8)` in ffmpeg terms vs vgmstream's `>> (12 + shift)`… the two references agree numerically) — settle by decoding one file and listening; §5.2 gives the test.
5. `ov_open` on Emscripten with FetchFS: the template streams `.umx` from memory, not through `FILE*`; ALAudio's ogg path uses `fopen` + `ov_open`. FetchFS's synchronous first-touch open (`ue_abi.h:23-28`) makes this work, but a 5 MB ogg is fetched whole on `fopen` — acceptable (menu music is fetched once; the precache list should include the startup-map songs).
6. Multiplayer viability of HP2's game classes (replication) — out of scope here; the P2P transport itself is a mechanical port.

---

## 11. Independent second pass (Opus) — verified corrections, answers and additions

Two analysts wrote this key in parallel; sections 0–10 above are the first pass (kept verbatim). This
section is a second, independent reading of the same six drivers plus HP2's `ALAudio`, `UnAudio.cpp`,
`FFileStream`/`AudioMutex`, `coda.h` and the retail assets. Everything below was checked against the
source or the bytes; where it **contradicts** sections 0–10, this section wins (the reason is given).
Notation as above (`T/`, `H/`, `E/`).

### 11.1 Corrections to sections 0–10

| # | First-pass statement | Correction (evidence) | Action |
|---|---|---|---|
| C1 | §3.1/§10.1: NullDrv `MaxVertices()` "return 0 — check Render first" | **Must be > 0; use 256.** Render sizes buffers from it and writes into them *before* calling the device: `H/Render/Src/UnMeshRn.cpp:117-121` (`VertBuffer.Set(VBSize); IndexBuffer.Set(VBSize*6)`), `H/Render/Src/UnParticleRn.cpp:467-499` (`New<FTransTexture>(GMem, MaxVerts)`, flush at `VertCount >= MaxVerts-ClipVerts`, `check(VertCount <= MaxVerts…)`). 0 = heap overrun / failed `check`. HP2's own devices return 256: `H/SoftDrv/Src/DrawPoly.cpp:4366-4370`, `H/D3DDrv/Src/Direct3D7.cpp:30,1151-1154` (`ACTORPOLY_VERTEX_MAX 256`). | `INT MaxVertices() { return 256; }` |
| C2 | §2.1/§2.2/§10.3: `ParticleDensity=100` "check UnCamMgr.cpp" | Registered as a plain `UIntProperty` (`H/Engine/Src/UnCamMgr.cpp:65`), clamped `Max(ParticleDensity,0)` in `PostEditChange` (`:34`), **retail value `ParticleDensity=1`** (`Config/System/Default.ini` `[WinDrv.WindowsClient]`, line 102). `NoFractalAnim` registered at `:64`, retail absent → 0. | `NoFractalAnim = 0; ParticleDensity = 1;` in `UNSDLClient::StaticConstructor`; same in the `[NSDLDrv.NSDLClient]` ini blocks |
| C3 | §1.4 / §9: "cast a spell and confirm `CompareGesture` returns non-zero"; "a spell gesture registers" | **HP2's shipped script never calls the gesture matcher.** `native(426) CompareGesture` / `native(427) CompareGesturePoint` are declared in `Engine/Classes/Gesture.uc:13-14`, implemented in `H/Engine/Src/UnGesture.cpp:336,392`, and a grep of every `*.uc` in `Source/` finds **no caller**. `Gesture` objects are used only as particle emission patterns (`Engine/Classes/ParticleFX.uc:105 var(PS_Emission) gesture Pattern`, `HPParticle/Classes/SpellLearnFX.uc:69 Sparkle.Pattern = gesture`). CoS PC casting = aim the **SpellCursor** with the mouse axes (`aMouseX/aMouseY` → `PlayerPawn.uc:2692-2751` `SmoothMouseX/Y` → `HGame/Classes/BaseCam.uc:533,1159`) and click. `GestureSprite.uc` is only the on-target sprite. | Drop the `CompareGesture` debugf test. Verification = the spell cursor follows relative mouse motion in-game and `IK_LeftMouse` casts. NSDL needs nothing gesture-specific. |
| C4 | §10.4: EA-XA coefficient/shift convention "settle by decoding one file and listening" | **Settled numerically.** Decoder (header byte `b`: `C1={0,240,460,392}[b>>4]`, `C2={0,0,-208,-220}[b>>4]`, sample `= ((nib<<28)>>((b&15)+8) + C1*h1 + C2*h2 + 128) >> 8`, clamp16, **high nibble first**, 15-byte frames, 28 samples, `0xEE` = raw BE16 frame of 61 bytes — none occur in HP2 data) reproduces the **lip-sync amplitude tables** that `USound::CalcLipSyncData` computed from the original PCM at import (peak `|s|/128` over 5 ms every 20 ms, `H/Engine/Src/UnAudio.cpp:450-490,1226-1267,1332-1370`): correlation **0.9997 / 0.9959 / 0.9981 / 0.9994 / 0.9993** and mean abs error **0.43–1.36 / 255** on `AllDialog.uax` `pc_hry_gotoquid_01`, `PC_Ara_Adv9aForest_10`, `pc_ara_adv9blair_05/07/09`. An independent oracle, stored in the retail data itself. | Use exactly this arithmetic. Keep the oracle as the decoder's unit test (§11.4). |
| C5 | §7.3/§5.4: `RegisterSound` stub `return 0` vs §4 table "stub anyway" | Harmless either way (no engine caller of `RegisterSound` besides the driver itself), noted for completeness: ALAudio's `PlaySound` treats `RegisterSound()==false` as "steal a voice and retry" (`H/ALAudio/Src/ALAudioSubsystem.cpp:722-735`). | none |
| C6 | §6.2: "`ULevel::SpawnActor` — verify its HP2 signature" (§10.2) | **Identical** to v400: `H/Engine/Inc/UnLevel.h:195` = `E/Engine/Inc/UnLevel.h:189` (`SpawnActor(UClass*, FName=NAME_None, AActor* Owner=NULL, APawn* Instigator=NULL, FVector, FRotator, AActor* Template=NULL, UBOOL bNoCollisionFail=0, UBOOL bRemoteOwned=0)`). `TcpLink.cpp:347` compiles unchanged. | none |
| C7 | §6.4: missing `ServerActors` classes → "a `Can't find file for package 'IpDrv'` failure per actor" | It is **fatal**, not per-actor: `ULevel::Listen` loads each with `StaticLoadClass(…, LOAD_NoFail, …)` (`H/Engine/Src/UnLevel.cpp:988`) and `LOAD_NoFail` → `appErrorf` (`H/Core/Src/UnObj.cpp:2156`). Any `?listen` (the frontend's P2P host) with the retail `ServerActors` list kills the engine. Same class of failure for `AudioDevice`: `UEngine::InitAudio` uses `LOAD_NoFail` (`H/Engine/Src/UnEngine.cpp:75`), so a stale `ALAudio.ALAudioSubsystem` is a boot-time crash. | The launcher's ini normalisation **must** empty `ServerActors` (both native and web) and rewrite `AudioDevice`; not optional. |
| C8 | §1.1 lists `DLL_EXPORT_CLASS` only | HP2's `H/Core/Inc/UnGnuG.h:97` defines **`DLL_EXPORT` as `extern "C"`** (the template's is empty, `T/Core/Inc/UnGnuG.h:94`). Until Phase A takes the template's `UnGnuG.h`, `class DLL_EXPORT UNullRenderDevice` (`NullDrvPrivate.h:21`), `class DLL_EXPORT UNullAudioSubsystem` (`Null.cpp:26`), `class DLL_EXPORT UNOpenALAudioSubsystem` (`NOpenALDrvPrivate.h:49`), `class NSDLDRV_API …` (`NSDLDrv.h:9,24,107`) are syntax errors and `DLL_EXPORT UBOOL GTickDue;` (`NSDLDrv.cpp:7`) becomes a declaration with no definition. | Phase A prerequisite (PLAN §3 already takes the template's `UnGnuG.h`); if that ever changes, switch the drivers to `DLL_EXPORT_CLASS`. |
| C9 | §9 NullDrv/SoundDrv milestones fine; NSDLDrv "`SetRes 1024x768` works" | NSDL's `SetRes` always goes **fullscreen** (`MakeFullscreen(X,Y,1)`, `T/NSDLDrv/Src/NSDLViewport.cpp:1345-1351`). HP2's video menu calls `SetRes` on Apply (`HGame/Classes/Menu/FESoundVideoPage.uc:534,560`) — on the web that fights the page-owned canvas. | `SetRes`: resize windowed unless already fullscreen; ignore under `__EMSCRIPTEN__` (log it). |

### 11.2 Additions not covered above

**A1 — NSDL is missing three WinDrv exec commands HP2's options menu uses.** `FESoundVideoPage.uc:475-495`
calls `GetColorDepths` and `GetCurrentColorDepth` (and v400/HP2 WinDrv also answer `GetCurrentRenderDevice`,
`E/WinDrv/Src/WinViewport.cpp:143-157`, `H/WinDrv/Src/WinViewport.cpp:140-159`). The template's
`UNSDLViewport::Exec` (`NSDLViewport.cpp:1324-1363`) has only `ToggleFullscreen/GetCurrentRes/SetRes/Preferences`;
`UNSDLClient::Exec` adds `EndFullscreen/GetRes`. Without them the colour-depth combo is empty. ACTION: add
`GetCurrentColorDepth` → `Ar.Logf(TEXT("%i"), (ColorBytes?ColorBytes:2)*8)`, `GetColorDepths` → `Ar.Log(TEXT("16 32"))`,
`GetCurrentRenderDevice` → `Ar.Log(RenDev->GetClass()->GetPathName())` to `UNSDLViewport::Exec`.

**A2 — HP2's options menu treats audio volumes as floats 0..1.** `FESoundVideoPage.uc:314,341` read
`get ini:Engine.Engine.AudioDevice MusicVolume|SoundVolume` and multiply by 100; `:617,623` write
`Slider/100`. The template NOpenALDrv's volumes are BYTEs 0..255 (`T/NOpenALDrv/NOpenALDrvPrivate.h:56-59`) —
the HP2 menu would show 0–25500 % and write 0.00–1.00 into a BYTE. ACTION: NOpenALDrv keeps ALAudio's
`FLOAT SoundVolume/MusicVolume` (retail 0.9 / 0.5, `Default.ini:155-156`). Also: ALAudio's `SetVolumes()` is
empty (`ALAudioSubsystem.cpp:1823-1828`), so a slider change only applied to the *next* sound; apply it live in
`PostEditChange`. (`HGame/Classes/Harry.uc:543` also reads `AudioDevice Use3dHardware` — no such key, returns
empty → `false`; harmless.)

**A3 — `USound::CompressXA` does not compile on non-Windows (Engine, Phase A).** `hProcess` is declared inside
`#ifdef _WINDOWS` (`H/Engine/Src/UnAudio.cpp:584`) and used outside it (`:590,606,611`). ACTION (Engine owner):
wrap the tool-spawning body in `#ifdef _WINDOWS … #else return false; #endif` — it is import-time only.

**A4 — `FFileStream`: Core link blocker, and what replaces the Win32 streaming thread.** Confirming §4's note
with the full picture: the only mixer/streaming thread in HP2 is `FileStreamingThread`
(`H/Core/Src/UnVcWin32.cpp:667-704`, started by `CreateThread` in `appPlatformInit`, `:1489`, after
`CODASetNew(malloc)/CODASetDelete(free)`, `:1484-1486`). It loops every 1 ms over all streams and, per requested
chunk, calls `FFileStream::Read` (`:706-829`), which decodes **Ogg** (`ov_read`) or **XA** (`CEAXABLKDec::Decode`
into a single mono `short* buffer[1]`, `:780`; loop = re-`Feed` the whole `TLazyArray`, `:795-802`) straight into
the driver's chunk buffer. `Create` opens `fopen`+`ov_open` or builds the XA decoder (`:831-918`), `Destroy`
tears down (`:924-985`). The portable half is `H/Core/Src/UnMisc.cpp:1611-1810` (`Enter/Leave` is a spin on a
plain `INT Locked` — not thread-safe without atomics). ACTION: Core gets `Read/Create/Destroy` stubs
returning `false`; NOpenALDrv never uses `GFileStream` (it owns decoding, single-threaded, §11.3). `AudioMutex`
(`H/Core/{Inc,Src}/AudioMutex.*`) has zero users — do not compile it; PLAN §3's "port AudioMutex to
pthread_mutex/SDL_mutex" is unnecessary.

**A5 — struct packing vs libvorbis on wasm32.** The web build compiles every object with `-fpack-struct=4`
(`H/Source/CMakeLists.txt:77-79`). `OggVorbis_File` (the struct the driver allocates and passes to
`ov_open/ov_read`) contains `ogg_int64_t`/`double` members, 8-aligned on wasm32 (4 on i386 — natively the
mismatch would be invisible). ALAudio already guarded this with `#pragma pack(push,8)` around the vorbis
includes (`H/ALAudio/Src/ALAudioSubsystem.cpp:15-22`, same in `UnVcWin32.cpp:655-661`). ACTION: strip
`-fpack-struct=4` from the FetchContent ogg/vorbis targets (the same directory walk `cmake/OpenALSoft.cmake:81-98`
does for openal-soft), keep the `#pragma pack(push,8)` include guard in the driver, and add a guard: a
one-line C function compiled inside the vorbis target returning `sizeof(OggVorbis_File)`, compared with the
driver's `sizeof` in `Init` (`appErrorf` on mismatch).

**A6 — music file resolution.** ALAudio builds `"..\\music\\" + Song + ".ogg"` (`ALAudioSubsystem.cpp:1127-1161`):
backslashes and lowercase `music` vs the real `Assets/Music`. Song names come from map data and scripts; most
match file case exactly (`SpellLessonTrigger.uc:64-68` constants match `Assets/Music` byte-for-byte), but
`Ch1Rictusempra.unr` carries a song-like string `sm_cda_dadaBallSmashEdit_01V2` that exists in **no** file
under `Assets/Music` or `Assets/Sounds` (retail silently played nothing). ACTION: resolve `../Music/<name>.ogg`
case-insensitively against a one-time `GFileManager->FindFiles("../Music/*.ogg")` listing; log (do not fail)
misses. All 111 files are Ogg Vorbis (110 stereo 44.1 kHz, 1 stereo 22.05 kHz — per-stream rate must be honoured,
as ALAudio does with `Stream.Rate = VorbisInfo->rate`).

**A7 — web memory and `AllDialog.uax` (97.4 MB).** All 2,445 dialog lines live in one package that scripts load
by name (`Engine/Classes/Actor.uc:700,746`, `HGame/Classes/HChar.uc:624`: `DynamicLoadObject("AllDialog."$id,
class'Sound')`). The engine only deserialises the requested export (lazy arrays are loaded per object; the
client runs with `GLazyLoad=0` like the template, `H/Launch/Src/Launch.cpp:148`), but FetchFS moves **whole files**
through its sync channel in 4 MB windows (`ufrontend/packages/glue/src/fetchfs/sync-channel.js:64-65`) — the
first dialog line of a session would pull ~97 MB into MEMFS. ACTION (frontend/Phase D, flag now): exclude
`Sounds/AllDialog.uax` from `precache.json`, budget `memfsBudgetMb` for it, and consider range-read support in
FetchFS for `.uax`; in the driver, `Sound->Data.Unload()` after the last stream of an XA sound ends (ALAudio has
that `#if 0`, `UnVcWin32.cpp:973-979`) so decoded-on-demand dialog does not accumulate in the heap (the XA corpus
is 121 MB compressed, ~450 MB as PCM).

**A8 — coordinate system.** ALAudio's listener orientation (`Update`, `ALAudioSubsystem.cpp:1255-1283`) was tuned
against Creative's OpenAL 1.0 DS3D backend (hence its `ReverseStereo` knob and a separate `UseEAX` branch) and
passes Unreal positions unconverted. openal-soft is right-handed. ACTION: use the template's conversion (negate Z
of positions/velocities, orientation from `Coords.ZAxis`/`-YAxis`: `T/NOpenALDrv/NOpenALDrv.cpp:550,574` for sources, `:781-805` for the listener), keep
`ReverseStereo` as an escape hatch, and verify left/right with a known source (a pawn to Harry's left).

**A9 — reverb data in the retail maps.** No map references `I3DL2Listener`/`ZoneEffect` (0 of the 48 `.unr` contain the
names); `bReverbZone` is set in exactly two (`Quidditch.unr`, `Quidditch_Intro.unr`), `MasterGain` in none. Retail
had no reverb at all (EAX compiled out, `ALAudioSubsystem.cpp:225-252`; `SetI3DL2Listener` returns at `:1936`).
ACTION: keep the template's EFX machinery but default `UseReverb=False`; skip the I3DL2 mapping entirely.

**A10 — IpDrv.u is compilable in principle.** The v400 `IpDrv/Classes` (5 `.uc`, identical to epic) depend only on
`InternetInfo` (`H/Engine/Classes/InternetInfo.uc` identical to epic), `LevelInfo.GetAddressURL` (native,
`LevelInfo.uc:164`) and `GameInfo.GetBeaconText` (event, `GameInfo.uc:635`) — all present in HP2's `Engine`. So
`ucc make` with `EditPackages=IpDrv` appended after `HGame` (`Default.ini:327-337`) should succeed once UCC runs;
`ucc make -h` then emits a 433-style `IpDrvClasses.h` (4-arg `DECLARE_CLASS`) to diff against the hand edit. Still
not needed: nothing in HP2 script spawns an `InternetLink`, and `UTcpNetDriver`/`UTcpipConnection` are
script-less. Keep `ReceiveModePad[3]` only if an `IpDrv.u` is built and the layout guard agrees.

**A11 — `DownloadManagers`.** HP2's `UNetConnection` has 436-style autodownload (`UDownload* Download`,
`TArray<FDownloadInfo>`, `H/Engine/Inc/UnConn.h:42-49,135-137`; `UnDownload.h` absent from v400) and loads each
manager with `LOAD_NoWarn` (`H/Engine/Src/UnConn.cpp:173-175`); the template's v400 IpDrv has no `UHTTPDownload`.
Agree with §6.4: `DownloadManagers=Engine.ChannelDownload` only.

**A12 — WebRTCLan's `DOUBLE` clocks.** With HP2's `FTime appSeconds()` every `DOUBLE Now = appSeconds();` in
`T/WebRTCDrv/WebRTCLan.cpp` (six call sites: lines 188, 217, 430, 488, 504, 517; the `DOUBLE` statics/fields
`GLastDiscoveryAt` :89, `NextBeaconAt/SweepDeadline/LastPendingLog` :101-106, `GLastPumpAt` :211 can stay `DOUBLE`) fails to compile. The file is
compiled only under `__EMSCRIPTEN__`. ACTION: one helper `static DOUBLE LanNow() { return (DOUBLE)appSeconds().GetFloat(); }`
and replace the six calls (float seconds have ~1 ms resolution at 10⁴ s of uptime — fine for 2–30 s timers).

**A13 — template's `UnRenDev.h` is not epic's.** `T/Engine/Inc/UnRenDev.h:48` adds `BITFIELD VolumetricLightingMeshes`
(absent in `E/` and `H/`). NullDrv never touches it; flag for `renderer-forks.md` in case XOpenGL/UTGLR reference it.

**A14 — small ALAudio bugs to not port.** `delete PrimeData;` on `new BYTE[]` (`ALAudioSubsystem.cpp:350,358,371,392,435,460,477,491,544`
→ `delete[]`); `FLipSyncAnimMgr` leaves `m_LipSyncAnimChan` uninitialised (`:2142-2146`; safe only because
`Start()` follows immediately); `FALAudioStats` is all `#if 0`; streaming refills at most **one** processed
buffer per source per `Update` (`:1480-1545`) — at a throttled background tab that underruns; refill all processed
buffers per call.

**A15 — what still works under the null SoundDrv (so Phase B/D can judge "silent but correct").** Dialog and
cut-scene timing use `AActor::execGetSoundDuration` → `USound::GetDuration()` (`H/Engine/Src/UnScript.cpp:1356-1370`),
i.e. the `Duration` serialised in the package (every sound has `Duration > 0`, §11.4) — no audio device involved, so
conversations advance normally when silent. Lip-sync, however, is driven **only** by the audio driver
(`FLipSyncAnimMgr`, created in `PlaySound`, `ALAudioSubsystem.cpp:869-870,911`, updated from `Update` `:1555`) — with
the null driver mouths stay shut. Music handles: scripts keep `PlayMusic`'s return and pass it back to `StopMusic`
(`Engine/Classes/MusicTrigger.uc:24`, `HGame/Classes/Triggers/NewMusicTrigger.uc:44`); `0` is safe.
`FSoundData::Load` no longer calls `RegisterSound` (commented out, `UnAudio.cpp:105-106,115-116`), so a null driver never sees
sound registration at load time. `-NOSOUND` skips audio creation entirely (`UnEngine.cpp:71-73`) — a zero-code fallback
while SoundDrv is not yet built.

**A16 — what the copied launcher already does about C7, and the gap.** `H/Game/Src/Launch.cpp` (template copy)
clears `ServerActors` **only under `__EMSCRIPTEN__`** (`:572-585`, after `ConstructObject`, before `Init`) and runs
`NormalizeSubsystem` for `ViewportManager/AudioDevice/NetworkDevice` **only when not `__EMSCRIPTEN__`**
(`:528-549`, preference `NOpenALDrv…` then `SoundDrv.NullAudioSubsystem`); on the web it hard-sets
`AudioDevice=NOpenALDrv.NOpenALAudioSubsystem` (`:495`). For HP2: (1) clear `ServerActors` on **native too** — HP2
ships no `IpDrv.u`/`IpServer.u`/`UWeb.u`, so a native `?listen` with the retail list is the fatal `LOAD_NoFail`
of C7; (2) on the web, set `AudioDevice` to NOpenALDrv only `#if HP2_HAS_NOPENALDRV`, else
`SoundDrv.NullAudioSubsystem` (until Phase E lands the web build would otherwise die in `UEngine::InitAudio`);
(3) `NormalizeSubsystem` must also treat `ALAudio.ALAudioSubsystem` and `D3DDrv.D3DRenderDevice` as unloadable
(it probes loadability, so it will, but check the HP2 retail values are covered by the switch table's log lines).

### 11.3 NOpenALDrv — the ALAudio → NOpenALDrv replacement table (line level)

Agrees with §5.6 in direction (port HP2's `ALAudio`, not the template's game logic); this is the line-level
version. Base: `H/ALAudio/Src/ALAudioSubsystem.cpp` + `Inc/ALAudioSubsystem.h`.

| ALAudio | NOpenALDrv |
|---|---|
| `#include "../../OpenAL/Inc/{al,alc,eax}.h"` (1.0 SDK, `ALubyte*` strings) | `<AL/al.h> <AL/alc.h> <AL/alext.h>` from the `OpenAL` target; drop `(ALubyte*)` casts |
| `alcOpenDevice("DirectSound3D"/"DirectSound"/"MMSYSTEM")`, `{ALC_FREQUENCY,44100,0}` (`:161-169`) | template `Init`: `LoadConfig()`, `alcOpenDevice(NULL)`, `{ALC_FREQUENCY,OutputRate, ALC_HRTF_SOFT,UseHRTF, ALC_OUTPUT_MODE_SOFT,…}`, HRTF read-back log (`T/NOpenALDrv/NOpenALDrv.cpp:143-235`: `LoadConfig` :159, context :197, log :229) |
| `alEAXGet/alEAXSet`, `SetI3DL2Listener` (dead) | `ResolveEfx` (`T/…:69-106`), `UseReverb` default False (A9) |
| `AL_INVERSE_DISTANCE_CLAMPED`, `DISTANCE_FACTOR 0.0254/2`, ref-distance = radius (`:221,874`, `.h:53`) | **keep HP2's** (the game's radii were tuned for it) |
| listener orientation (`:1255-1283`) | template conversion (A8) |
| `GFileStream->CreateStream(Filename,…,ST_Ogg[Looping],OggFile)` (`:361`) | driver-owned `FAudioStream::CreateOgg(ResolveMusic(Song), Looping)` over `ov_open_callbacks`/`ov_fopen` (A6) |
| `GFileStream->CreateStream(&Sound->Data, raw_NumSamples, 32K, n, …, ST_XA[Looping])` (`:473`) + `CEAXABLKDec` | `FAudioStream::CreateXA(Sound, Looping)`: `Sound->Data.Load()`, in-tree `FEAXADecoder::Feed(Data, Num, raw_NumSamples)` (C4) |
| `ChunksRemaining/DecrementChunkCount/RequestChunks/IsStreamAlive` polled in `Update` (`:1480-1545`, unqueue at `:1491`), filled by the 1 ms thread | synchronous refill in `Update` for every processed buffer (A14); EOF → loop (re-feed XA / `ov_raw_seek(vf,0)`) or pad silence + `EndOfFile` |
| `MAX_BUFFERS_PER_STREAM 3`, 64 KB Ogg / 32 KB XA chunks (`.h:40-45`) | 4 × 32 KB both (~0.74 s each for 44.1 kHz stereo ×4 and 22.05 kHz mono); make them config for web tuning |
| `USound::BindStream/…/GetHandle(Id)` per-instance stream bookkeeping (`H/Engine/Src/UnAudio.cpp:215-400`) | keep (engine code the driver drives). Phase-2 option: decode short non-looping XA (≤ ~256 KB PCM) once into a plain AL buffer |
| WAV path: `FWaveModInfo::ReadWaveInfo` → `alBufferData` → `Data.Unload()`, loop from `smpl` (`:549-600`) | keep |
| `PlaySound` returns source index, `-1` = not played; `PlayMusic` returns `index+1`, `0` = failure | keep (script stores the music handle) |
| `FLipSyncAnimMgr` (`:2142-2222`) | port verbatim + init fix; optionally drive elapsed time from `AL_SEC_OFFSET` |
| config `UseEAX, CompatibilityMode, UsePrecache, ReverseStereo, Channels, MusicVolume, SoundVolume, DopplerFactor, RollOff` (`:46-59`) | keep all but `UseEAX/CompatibilityMode`; add `OutputRate, UseHRTF, UseReverb`; section `[NOpenALDrv.NOpenALAudioSubsystem]` |
| libxmp (template `CMakeLists.txt:16-47`) | removed; FetchContent `xiph/ogg v1.3.5` + `xiph/vorbis v1.3.7` (A5); **no coda, no MpegLib, no ffmpeg** — the EA-XA decoder is ~60 lines in-tree |

### 11.4 Decoder unit test (the acceptance oracle)

For 50 `AllDialog.uax` sounds with `CoreFlags & SF_HasLipSync` and `FileType=='XA'`: decode `raw_NumSamples`
samples; for `i` in `[0, LipSyncData.Num()-1)`: `peak_i = max |pcm[k]|/128` for `k` in
`[round(0.020·i·rate), round((0.020·i+0.005)·rate))` (bytes aligned to 2, exactly as `FWaveModInfo::OffsetFromTime`
does, `UnAudio.cpp:1311-1325`); assert Pearson(peak, LipSyncData) > 0.99 and mean |Δ| < 3. The package-level parse
needed: `USound::Serialize` (`UnAudio.cpp:150-191`) = tagged props (`None`), `FileType` (compact FName),
`CoreFlags` INT, `Duration` FLOAT, 4 × INT `raw_*` (package version 79 > 78), `Data` = INT seek-pos + compact count +
bytes, then `LipSyncData` the same way. Measured census it should reproduce: 3,289 sounds; 2,994 `XA`
(626 flags 260, 2,368 flags 388) = 121,218,855 bytes, 2,993 mono 16-bit 22,050 Hz + 1 at 44,100 Hz; 295 `wav`
(218 flags 0, 77 flags 128), 293 × 16-bit + 2 × 8-bit mono 22,050 Hz; every `Duration > 0`, XA `Duration ==
raw_NumSamples/raw_SampleRate` ±10 ms.

### 11.5 Consolidated action checklist (both passes)

Core/Engine prerequisites (Phase A owners):
- [ ] Template `UnGnuG.h` (`DLL_EXPORT` empty, `GCC_USED`, `IMPLEMENT_PACKAGE_PLATFORM` + `STATIC_EXPORT`) — C8.
- [ ] Template `IMPLEMENT_CLASS`/`IMPLEMENT_PACKAGE` (`THIS_PACKAGE`, autoclass + `STATIC_EXPORT`) in `UnObjBas.h`; keep the 4-arg `DECLARE_CLASS`.
- [ ] Template `IMPLEMENT_FUNCTION` with `STATIC_EXPORT` in `UnScript.h` (IpDrv's 22 name-bound natives).
- [ ] Template `UnUnix.cpp` (`appGetDllHandle` from the static registry) + `FFileStream::Read/Create/Destroy` stubs (A4); do not compile `AudioMutex.cpp`, `UnVcWin32.cpp`.
- [ ] `USound::CompressXA` `#ifdef _WINDOWS` (A3).
- [ ] `UGameEngine::Tick` whole-game pointer-lock block from the template (`T/Engine/Src/UnGame.cpp:1551-1641`).

Drivers:
- [ ] 4-arg `DECLARE_CLASS` at all 12 sites (§1.1 list).
- [ ] NullDrv: `DrawComplexSurface(…, DWORD, BYTE)`, `MaxVertices()` **= 256** (C1), `DrawTriangles` stub, include fix.
- [ ] SoundDrv: HP2 `UAudioSubsystem` body (§4); `PlaySound → -1`, `PlayMusic → 0`.
- [ ] NSDLDrv: `FTime` fixes (2 sites), `NoFractalAnim=0; ParticleDensity=1` (C2), `bWindowsMouseAvailable=!Capture` in `SetMouseCapture` (§1.4), `GetColorDepths/GetCurrentColorDepth/GetCurrentRenderDevice` (A1), `SetRes` windowed/web-ignore (C9), `LocalizeError("Failed3D")` literal.
- [ ] IpDrv: 4-arg macros, drop the two commandlets, delete Dreamcast blocks; no `IpDrv.u` in Phase B (A10).
- [ ] WebRTCDrv: 4-arg macros, `FTime` at `WebRTCDrv.cpp:399-402`, `WEBRTC_PEER_PREFIX "hp2"`, "HP2 Server", WebRTCLan `LanNow()` (A12).
- [ ] NOpenALDrv (Phase E): §5.6 + §11.3 + A2/A5/A6/A7/A8/A9/A14; decoder test §11.4.

Inis / launcher:
- [ ] `ServerActors` emptied (fatal on listen otherwise, C7); `AudioDevice` rewritten (fatal at boot otherwise, C7);
      `DownloadManagers=Engine.ChannelDownload`; `[NSDLDrv.NSDLClient]` incl. `ParticleDensity=1`;
      `[NOpenALDrv.NOpenALAudioSubsystem]` with FLOAT volumes; `GameRenderDevice` rewritten (D3DDrv absent).

Frontend (flag):
- [ ] `AllDialog.uax` 97 MB whole-file fetch (A7); `Music/*.ogg` in the manifest; `network.peerPrefix = "hp2"`.

### 11.6 Remaining open questions (second pass)

1. Does HP2's menu cursor get double-moved when NSDL emits both absolute `MousePosition` and `IK_MouseX/Y`
   deltas in absolute mode (`NSDLViewport.cpp:1247-1249`)? `HPConsole.uc:694-695` integrates `IK_MouseX` only when
   `bWindowsMouseAvailable` is false, so it should not — verify on the first native boot.
2. FetchFS behaviour for a 97 MB `.uax` inside the `memfsBudgetMb` (A7) — owned by the frontend analysis.
3. Whether libvorbis 1.3.7's CMake accepts the sibling `Ogg::ogg` target or needs `OGG_INCLUDE_DIR/OGG_LIBRARY`
   pre-seeded — find out at the first configure (A5).
4. Stereo placement sign (A8) — needs ears or a meter on the first native run.

## Status: complete

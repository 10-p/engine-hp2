# template-core — Core package portability delta the HP2 port must replicate

**Key:** `template-core` · **Written:** 2026-09-26 (relaunch; supersedes the 2026-09-25 run, re-derived
from the trees rather than copied) · **Scope:** `Source/Core` only (Inc/, Src/, the template-only files and
`Core/CMakeLists.txt`; `Classes/` noted where relevant).

**What this answers.** Every change `engine-ut99/Source/Core` (maximqaxd/ut99dc + the ufront web/Windows
work) makes relative to Epic's `ut99-v400/Core`, hunk by hunk: *what it does*, *why* (with the commit that
introduced it), and — for each hunk — the same location in HP2's build-433 `Source/Core`, classified
APPLIES-CLEANLY / NEEDS-ADAPTATION / NOT-APPLICABLE / ALREADY-PRESENT-IN-433, with the ACTION the port must
take. Then: HP2-only Core files, the 433 idioms that change the template's approach, the HP2-side
portability problems the template never had to solve, and a per-file merge strategy + ordered checklist.

## 0. Method, trees, classification legend

| Role in the 3-way merge | Path (all READ ONLY except HP2) | Line endings |
|------|------|------|
| **base** — common ancestor | `/home/ion/epic-sources/ut99-v400/Core` | CRLF → always `diff --strip-trailing-cr -w` |
| **theirs** — the template | `/home/ion/engines/engine-ut99/Source/Core` (branch `main`, HEAD `7c31a53`; Core working tree verified == HEAD) | LF |
| **ours** — the port target | `/home/ion/hp2-ufrontend/HarryPotter2/Source/Core` (HEAD `e47a3c7`) | LF |

How the evidence was produced (reproducible):
- `diff --strip-trailing-cr -w -u <epic> <template>` per file for every hunk; the same with `<epic> <HP2>` and
  `sed -n` views of HP2 at the same anchors.
- `tools/dev/merge3.sh <scratch>/merge Core` (the repo's dry-run helper: `git merge-file -p --diff3`,
  base=epic, ours=HP2, theirs=template) for the CLEAN / CONFLICTS=n column, and the conflict bodies read hunk
  by hunk.
- `git -C /home/ion/engines/engine-ut99 archive <sha> Source/Core` into a scratch directory for all 23 commits
  that touched `Source/Core` (read-only; nothing written to the template), so each hunk is attributed to the
  commit that introduced it; `git show -s` for the commit messages (quoted in §2).
- `grep -rn` over HP2 `Source/` for users of every HP2-only symbol.
- No file under `/home/ion/engines`, `/home/ion/epic-sources`, `/home/ion/ufrontend` was written; nothing was
  built; only this report was written in the HP2 repo.

**State of HP2 Core at HEAD `e47a3c7`** (important — not a pristine drop any more): the skeleton commit
already copied, *unadapted*, from the template: `Source/Core/CMakeLists.txt` (HP2 file list, template
structure), `Inc/UnStaticExports.h`, `Src/UnStaticExports.cpp`, `Src/UnMinGW.cpp` (all three byte-identical
to the template). Every other Core file is the 2002 drop, CRLF→LF.

**Classification tags**

| Tag | Meaning |
|-----|---------|
| **APPLIES-CLEANLY** (AC) | HP2's text at the hunk's site is still Epic's; `git merge-file` merges it, or it is a pure insertion with no HP2 counterpart |
| **NEEDS-ADAPTATION** (ADAPT) | same intent is wanted but HP2's text/signature differs at that site (433 rewrite); the *how* is stated |
| **NOT-APPLICABLE** (N/A) | Dreamcast / PSVita / LOW_MEMORY / DC-util only, or superseded by a 433 rewrite, or rejected by the template's own later commit |
| **ALREADY-PRESENT-IN-433** (PRESENT) | HP2's 433 tree already carries an equivalent (often under `__PSX2_EE__`/`__LINUX__`, from the Loki / PS2 work) |


## 1. Inventory: which Core files the template changed, and how far HP2 diverged

### 1.1 Files the template changed (epic → template non-empty)

Counts are changed lines (`<` + `>`) of `diff --strip-trailing-cr -w`. "Dry run" = `tools/dev/merge3.sh`
result for Core (re-run 2026-09-26). "Commits" = every template commit that touched the file (ut99dc
upstream: `cbdb054` first blood, `17b0864` Dreamcast support, `ee0ff8e` editor/dcutil, `03dc982` DC
unaligned, `a5e2533` DC SWORD, `d9c6cc4` DC net, `2673db1` DC mips; ufront: everything from `d5a6bfd` on).

| File | epic→tmpl | epic→HP2 | tmpl→HP2 | Dry run | Commits |
|------|---:|---:|---:|---|---|
| `Inc/Core.h` | 37 | 142 | 171 | CLEAN | cbdb054 17b0864 ee0ff8e 301a9ca  |
| `Inc/FFileManagerGeneric.h` | 9 | 21 | 30 | CLEAN | cbdb054 17b0864  |
| `Inc/FFileManagerLinux.h` | 28 | 20 | 48 | CLEAN | cbdb054 17b0864 301a9ca  |
| `Inc/FMallocAnsi.h` | 3 | 13 | 16 | CLEAN | cbdb054 17b0864  |
| `Inc/FOutputDeviceAnsiError.h` | 27 | 0 | 27 | OURS-UNCHANGED | cbdb054 17b0864  |
| `Inc/FOutputDeviceFile.h` | 19 | 3 | 22 | CONFLICTS=1 | cbdb054 d5a6bfd  |
| `Inc/UnCache.h` | 4 | 5 | 9 | CLEAN | cbdb054 17b0864  |
| `Inc/UnClass.h` | 130 | 44 | 174 | CLEAN | cbdb054  |
| `Inc/UnCoreNet.h` | 2 | 2 | 4 | CLEAN | cbdb054  |
| `Inc/UnFile.h` | 137 | 57 | 194 | CLEAN | cbdb054 17b0864 ee0ff8e d5a6bfd ec74328 1d3dfa5 2c25d55  |
| `Inc/UnGnuG.h` | 288 | 89 | 355 | CONFLICTS=12 | cbdb054 17b0864 ee0ff8e d5a6bfd b40994b 301a9ca 76df82e  |
| `Inc/UnMath.h` | 346 | 1410 | 1750 | CONFLICTS=5 | cbdb054 17b0864  |
| `Inc/UnMem.h` | 18 | 10 | 28 | CLEAN | cbdb054  |
| `Inc/UnObjBas.h` | 62 | 140 | 200 | CONFLICTS=3 | cbdb054 17b0864 a680288  |
| `Inc/UnScript.h` | 32 | 52 | 56 | CONFLICTS=6 | cbdb054 17b0864 03dc982  |
| `Inc/UnStack.h` | 1 | 60 | 61 | CONFLICTS=1 | cbdb054 17b0864  |
| `Inc/UnTemplate.h` | 74 | 679 | 749 | CONFLICTS=9 | cbdb054 17b0864 1f744b8  |
| `Inc/UnUnix.h` | 9 | 0 | 9 | OURS-UNCHANGED | cbdb054 301a9ca  |
| `Src/Core.cpp` | 6 | 23 | 29 | CLEAN | cbdb054 17b0864  |
| `Src/UnAnsi.cpp` | 165 | 29 | 194 | CONFLICTS=1 | cbdb054 17b0864 ee0ff8e  |
| `Src/UnClass.cpp` | 73 | 394 | 461 | CONFLICTS=3 | cbdb054 17b0864 bb90b47 c506ba7  |
| `Src/UnCorSc.cpp` | 15 | 208 | 215 | CONFLICTS=4 | cbdb054 17b0864 03dc982  |
| `Src/UnGUID.cpp` | 15 | 3 | 18 | CLEAN | cbdb054 17b0864  |
| `Src/UnLinker.h` | 32 | - | - | (HP2 has no such file) | cbdb054 d5a6bfd 1546db7  |
| `Src/UnMath.cpp` | 15 | 193 | 202 | CONFLICTS=2 | cbdb054 17b0864  |
| `Src/UnMisc.cpp` | 288 | 573 | 857 | CONFLICTS=2 | cbdb054 17b0864 a5e2533 2673db1 d5a6bfd 7164de7 1d3dfa5 2c25d55  |
| `Src/UnName.cpp` | 10 | 49 | 57 | CONFLICTS=1 | cbdb054 17b0864 ee0ff8e  |
| `Src/UnObj.cpp` | 195 | 374 | 565 | CONFLICTS=4 | cbdb054 17b0864 2673db1 d5a6bfd ec74328 a860dca a57d578  |
| `Src/UnProp.cpp` | 11 | 28 | 39 | CLEAN | cbdb054 17b0864  |
| `Src/UnUnix.cpp` | 309 | 224 | 517 | CONFLICTS=5 | cbdb054 17b0864 d9c6cc4 d5a6bfd 53cbaf1 301a9ca  |

Template-only files (no Epic base): `Inc/UnStaticExports.h`, `Src/UnStaticExports.cpp` (`17b0864`),
`Src/UnFilePool.cpp` (`17b0864`, `03dc982`), `Src/UnMinGW.cpp` (`301a9ca`), `CMakeLists.txt` (`cbdb054`,
`301a9ca`) — §3. (`Src/FOutputDeviceFile.cpp` existed in ut99dc's first blood and was folded back into
`Inc/FOutputDeviceFile.h` by `d5a6bfd`; nothing to port for it separately.)

Files the template did **not** change (33 in the dry run: `THEIRS-UNCHANGED`) need nothing from this
report — HP2 keeps its own version. They include several that HP2 changed heavily (`FMallocWindows.h`
475 lines, `FMallocDebug.h` 412, `UnVcWin32.cpp` 558, `UnBits.cpp` 157, `FFileManagerWindows.h` 103,
`FCodec.h` 88, `UnName.h` 45, `UnType.h` 39, `UnCorObj.h` 23, `UnObjVer.h` 19, `UnArc.h` 18, `UnBuild.h` 18,
`FFileManagerAnsi.h` 18, `Classes/Object.uc` 80). Those HP2 changes matter to this report only where they
block a GCC/Emscripten build — §8.

### 1.2 Headline

- 30 Epic files carry template deltas (18 Inc + 12 Src) + 5 template-only files.
- Dry run on those 30: **12 CLEAN**, **2 OURS-UNCHANGED** (take the template's file verbatim:
  `FOutputDeviceAnsiError.h`, `UnUnix.h`), **15 CONFLICTS** (61 conflict hunks), and `Src/UnLinker.h` which
  HP2 does not have (433 moved the linker to `Inc/UnLinker.h` + `Src/UnLinker.cpp`) — its hunks must be
  ported by hand.
- A dry-run **CLEAN is not the same as correct**: several clean merges pull in Dreamcast/PSVita-only code
  that is dead (harmless) and a few pull in code that references symbols HP2 lacks — each is called out
  per hunk in §4/§5.

## 2. Template git history: rationale of the ufront-era Core changes

`git -C /home/ion/engines/engine-ut99 log --oneline -- Source/Core` = 23 commits (of 108 in the repo). The
first seven are maximqaxd/ut99dc upstream (first-light desktop port + Dreamcast); `d5a6bfd` is the squash of
74 commits + 28 merges of the ufront web/native port; everything after is individually documented ufront
work. Rationale below is condensed from the commit bodies (`git show -s <sha>`).

| Commit | Date | Core files | What / why | HP2 relevance |
|--------|------|-----------|------------|---------------|
| `cbdb054` first blood | 2025-12-01 | whole tree + `CMakeLists.txt` | ut99dc's import. Already differs from Epic v400 in 18 files: the GCC/POSIX toolchain work (ASM off in `UnGnuG.h`, `UnMath.h` C paths, `UnTemplate.h` two-phase `this->`, `UnClass.h`/`UnObjBas.h` GCC macro fixes, `UnUnix.cpp` POSIX layer, `FFileManagerLinux.h`, `UnAnsi.cpp`) | **The core of what HP2 needs.** HP2's 433 drop carries its own (older, Loki-era) versions of some of these under `__LINUX__`/`__GNUG__`; see per hunk |
| `17b0864` Dreamcast support | 2025-12-03 | 27 files | Adds `UNREAL_STATIC` static package registry (`UnStaticExports.*`, `IMPLEMENT_PACKAGE`/`IMPLEMENT_CLASS` `STATIC_EXPORT`, `appGetDllHandle` via registry), `PLATFORM_DREAMCAST`/`PLATFORM_LOW_MEMORY` branches, the `--wrap=fopen` `UnFilePool.cpp`, unaligned-access helpers, the actor-constructor "STUPID HACK" in `StaticConstructObject` (later removed by `a860dca`) | **Static registry: take.** Everything `PLATFORM_DREAMCAST`/`LOW_MEMORY`/`PSVITA`: skip (PLAN rule 2) |
| `ee0ff8e` build editor, dcutil | 2025-12-04 | Core.h, UnFile.h, UnGnuG.h, UnAnsi.cpp (+135), UnName.cpp | Symbols the Editor/UCC need under GCC: `appStrnicmp`/`wcs*` ANSI shims in `UnAnsi.cpp`, `GIsUCC`-style exports | Take what the Editor (linked ON per PLAN §2) needs |
| `03dc982` DC unaligned | 2025-12-04 | UnScript.h, UnCorSc.cpp, UnFilePool.cpp | `PLATFORM_DREAMCAST`-only unaligned bytecode reads | N/A (HP2 has its own `__PSX2_EE__` equivalents, also to be dropped) |
| `a5e2533` DC SWORD | 2025-12-06 | UnMisc.cpp (−7) | removed a debug block with DC BSP compression | check per hunk (§5 UnMisc) |
| `d9c6cc4` DC network | 2025-12-06 | UnUnix.cpp (+6) | `PLATFORM_DREAMCAST` net init | N/A |
| `2673db1` DC mips | 2025-12-06 | UnMisc.cpp, UnObj.cpp | DC VRAM mip unload hooks | N/A |
| `d5a6bfd` web+native squash | 2026-07-18 | FOutputDeviceFile.h, UnFile.h, UnGnuG.h, UnLinker.h, UnMisc.cpp, UnObj.cpp, UnUnix.cpp (−FOutputDeviceFile.cpp) | Emscripten: GLog→browser console (`FOutputDeviceFile` also echoes to stdout on web), `appBaseDir()=getcwd`, exceptions-off **NULL-return sites** (`SafeLoadError`, `GetPackageLinker`, linker open failures), `__EMSCRIPTEN__` guards in `UnUnix.cpp` (no signals/fork/`dlopen`), `appRequestExit` | **Take all (web layer).** Adapt to HP2's split linker (`Inc/UnLinker.h`, `Src/UnLinker.cpp`) |
| `53cbaf1` worker (2.13.2) | 2026-07-30 | UnUnix.cpp (+9) | `appRequestExit` via `MAIN_THREAD_EM_ASM` (only the main thread can navigate) when the engine runs on a pthread/worker | Take (web worker build) |
| `ec74328` GC iterative | 2026-08-03 | UnFile.h, UnObj.cpp | `FArchiveTagUsed` drains a worklist instead of recursing (`RangeError: Maximum call stack` at 5,547 wasm frames on DM-Codex at -O0); `UT_REPLACEMENT_ALLOC` `always_inline` on global `operator new/delete` so WasmFS's static init does not allocate through an uninitialised `GMalloc` | **Take both — engine-generic, HP2 has the same recursive walk** |
| `b40994b` SoftDrv fog | 2026-08-05 | UnGnuG.h | `appRound`/`appFloor` portable versions used by the ASM=0 rasteriser (rounding bias) | Take (HP2 SoftDrv is built) |
| `7164de7` Paths= case | 2026-08-04 | UnMisc.cpp | `appFindPackageFile` tries each `Paths=` entry exactly **then** case-insensitively before the next (Windows semantics on case-sensitive FS) | **Take — HP2 assets have mixed case too** |
| `bb90b47` layout guard | 2026-08-08 | UnClass.cpp | warns when a native class's C++ `sizeof` ≠ script property size (`[layout] native …`) | **Take — the single most useful 433 diagnostic** (HP2 classes are hand-maintained vs shipped `.u`) |
| `301a9ca` MinGW (2.33) | 2026-08-08 | CMakeLists.txt, Core.h, FFileManagerLinux.h, UnGnuG.h, UnUnix.h, UnUnix.cpp, +UnMinGW.cpp | "the compiler is not the platform": `PLATFORM_WIN32` vs `_MSC_VER`; GCC-on-Windows keeps `UnUnix.cpp` + `UnMinGW.cpp`; `DWORD`/`SIZE_T`/`BITFIELD` from `<minwindef.h>`; `-fpack-struct=4` | Take (Windows target) |
| `76df82e` HW GL on Windows (2.34) | 2026-08-09 | UnGnuG.h (+8) | `#undef VOID` must not apply on Windows (`<winnt.h>` `#define VOID void`) | Take |
| `1546db7` save path | 2026-08-15 | UnLinker.h | `ULinkerLoad/ULinkerSave` "Error opening file" now names the file | Take into HP2 `Inc/UnLinker.h`/`Src/UnLinker.cpp` (HP2 saves games — this matters more for HP2 than UT) |
| `1d3dfa5` three defects | 2026-08-18 | UnFile.h, UnMisc.cpp | (1) `appUnwindf` no longer latches `GIsCriticalError` for every recoverable unwind (it gagged the log and skipped `~UObject`); (2) `appIsGlobalOptionToken()` so `Key=Value` first-arg is not taken as a map URL | **Take (1) and (2)**; (3) is NSDLDrv |
| `2c25d55` `-ini:` switch | 2026-08-18 | UnFile.h, UnMisc.cpp | implements `-ini:<File>:[<Section>]:<Key>=<Value>` (the frontend emits it for player name etc.) | **Take — ufrontend's hp2 target will emit the same switch** |
| `a680288` no value-init | 2026-07-29 | UnObjBas.h | `DECLARE_CLASS` `new(X)TClass()` → `new(X)TClass`: value-init zeroed the UObject header (hash chain cut) | **Take — adapt into HP2's 4-arg DECLARE_CLASS** |
| `c506ba7` native bool mask | 2026-09-13 | UnClass.cpp | `UClass::Register` link restores a native `UBoolProperty`'s `BitMask` as well as its `Offset` | **Take — NOpenALDrv/HP2 audio driver have native bools** |
| `a860dca` remove DC ctor undo | 2026-09-21 | UnObj.cpp | deletes 17b0864's "STUPID HACK" in `StaticConstructObject` | Never import the hack (HP2 does not have it); nothing to do beyond not taking 17b0864's version |
| `1f744b8` `FString::Mid` | 2026-09-22 | UnTemplate.h | `Mid(Start)` → `DWORD End = (DWORD)Start+(DWORD)Count` (signed overflow UB) | **Take — adapt** (HP2's `FString` differs) |
| `a57d578` version mismatch | 2026-09-22 | UnObj.cpp | web: GUID mismatch in `GetPackageLinker` → `SafeLoadError` + NULL | Take (web), low priority (HP2 has no MP content downloading) |

Commits after `a57d578` touching Core: none (latest template HEAD `7c31a53` is an Engine comment change).

## 3. Template-only files: UnStaticExports.cpp/.h, UnMinGW.cpp, UnFilePool.cpp, CMakeLists.txt

### 3.1 `Inc/UnStaticExports.h` + `Src/UnStaticExports.cpp` (ut99dc `17b0864`) — the static package registry

What it is: a link-time-constructed singly linked list of `{name, address}` pairs.
`STATIC_EXPORT(sexp, ssym)` defines a global `FPackageExport GStaticExport##sexp( #sexp, (void*)&ssym )`
whose constructor pushes itself on `GExportsTable` (static-init order does not matter: the list is only
walked after `main`). `appGetStaticExport(Name)` is a linear `appStrcmp` walk.

```cpp
// Inc/UnStaticExports.h:24 (template == HP2, copied in e47a3c7)
#define STATIC_EXPORT( sexp, ssym ) \
	extern "C" { GCC_USED FPackageExport GStaticExport##sexp( #sexp, (void*)&ssym ); }
```

It replaces `dlopen`/`dlsym` (and `LoadLibrary`/`GetProcAddress`) in a single binary: the three users are

| Registrant (template file:line) | Registered name | Looked up by |
|---|---|---|
| `IMPLEMENT_PACKAGE_PLATFORM` (`Inc/UnGnuG.h:220-223`) | `GLoaded<Pkg>` | `appGetDllHandle` (`Src/UnUnix.cpp:264-290`) strips path + extension, prefixes `GLoaded` → non-NULL handle = "package has native code" |
| `IMPLEMENT_FUNCTION` non-MSVC branch (`Inc/UnScript.h:31`) | `int<Class>exec<Func>` | `UFunction::Bind` → `UPackage::GetDllExport` → `appGetDllExport` (`Src/UnUnix.cpp:363-372` → `appGetStaticExport`) |
| `IMPLEMENT_CLASS` (`Inc/UnObjBas.h:351-352`) | `<TClass>` (e.g. `UObject`) — **note: not `autoclass<TClass>`** | `UClass::Bind` asks for `autoclass%s` (`Src/UnClass.cpp:843`) — the names do not match, so that lookup can only fail; it is never reached for a native class because the `PrivateStaticClass` object already carries `ClassConstructor` from `EC_NativeConstructor`. Harmless in the template; keep identical in HP2 (do not "fix" it without a reason) |

Requirements that come with it: `GCC_USED` (`Inc/UnGnuG.h:126`, `__attribute__((used))`) so `-O2` does not
drop the unreferenced globals; **`--whole-archive`** on the package static libraries (root CMake) so the
linker keeps objects nothing references; `THIS_PACKAGE` pasted per package (`Inc/Core.h:348-356`,
`GPackage##UPACKAGE_NAME`) so every package's `GPackage` symbol is distinct in one binary; `DLL_IMPORT`
emptied under `UNREAL_STATIC` (`Inc/UnGnuG.h:113-116`).

**HP2 state:** both files present and byte-identical to the template (`e47a3c7`). `Src/UnStaticExports.cpp`
line 2 still says "Dreamcast-specific routines" (stale ut99dc header comment; cosmetic).
**HP2 already has a *different* static scheme** (`__STATIC_LINK`, the Loki/PS2 one): `UnCoreNative.h`
lookup tables (`MAP_NATIVE`, `GNativeLookupFuncs[32]`), `AUTO_INITIALIZE_REGISTRANTS` (`Inc/UnObjBas.h:1079`,
called from `Src/UnObj.cpp:1332`), `GetPrivateStaticClass##TClass` in `DECLARE_CLASS`/`IMPLEMENT_CLASS`
(`Inc/UnObjBas.h:284-420`), `UPackage::GetDllExport` → `FindNative` (`Src/UnObj.cpp:4026`). Every one of
those is `#if __STATIC_LINK`; nothing in the CMake defines it, so today they are all dead code.

**ACTION (PLAN §2 decision, confirmed by the evidence):**
1. Keep `UnStaticExports.{h,cpp}` as is; include `UnStaticExports.h` from HP2's `Core.h` exactly where the
   template does (§4 Core.h).
2. Port the three registrants: `IMPLEMENT_PACKAGE_PLATFORM` (UnGnuG.h), `IMPLEMENT_FUNCTION` (UnScript.h),
   `IMPLEMENT_CLASS` (UnObjBas.h, HP2's non-`__STATIC_LINK` branch) and the two lookups in `UnUnix.cpp`.
3. Leave `__STATIC_LINK` undefined; delete (or leave dead) its branches. **Do not** try to use the
   `GNativeLookupFuncs` tables: they are hand-maintained lists that exist only for the PS2 build (see §6
   UnCoreNative, §7.2) and would have to be kept in sync with every native by hand.
4. Watch the one registry-specific risk for HP2: **two packages exporting the same name**. Names are
   `int<Class>exec<Func>`, `<TClass>`, `GLoaded<Pkg>` — unique as long as class names are unique across
   packages. HP2 adds packages (ALAudio→NOpenALDrv, HP2's `Engine` natives); a duplicate would silently
   return the most recently constructed entry. Verify with `nm Game | grep GStaticExport | sort | uniq -d`
   after the first link (must be empty).

### 3.2 `Src/UnMinGW.cpp` (ufront `301a9ca`, 2.33) — Win32 calls with no POSIX answer

Compiled only when `TARGET_IS_WINDOWS` and not MSVC (`Core/CMakeLists.txt:34-37`), body under
`#ifdef PLATFORM_WIN32`. Provides `appGetSystemErrorMessage` (FormatMessageA), `appGetGUID` (via
`CoCreateGuid`; `appCreateGuid` in `UnUnix.cpp` delegates here), `appWinSockInit` (WSAStartup, called by
IpDrv). Links `ole32` + `ws2_32` (root CMake `link_libraries(ws2_32 ole32)`, present in HP2).

**HP2 state:** present, byte-identical (`e47a3c7`). It includes `CorePrivate.h` and uses `guard`/`unguard`,
`appErrorf`, `appStrchr`, `ARRAY_COUNT` — all exist in HP2. Its three functions must be *declared* by HP2's
headers for the callers: `appGetSystemErrorMessage` is declared in HP2 `Inc/UnFile.h` (Epic API);
`appGetGUID` / `appWinSockInit` declarations come from the template's `UnUnix.h` / `UnFile.h` hunks
(§4 UnUnix.h — take-theirs) — verify after merging with `grep -n "appGetGUID\|appWinSockInit" Source/Core/Inc/*.h`.
**ACTION:** keep. **APPLIES-CLEANLY** once `UnUnix.h`/`UnUnix.cpp` are the template's.

### 3.3 `Src/UnFilePool.cpp` (ut99dc `17b0864`, `03dc982`) — Dreamcast `--wrap=fopen` file-handle pool

`__real_fopen`/`__wrap_fopen` interposition keeping ≤7 real FILE handles (KOS limit), compiled only
`if(TARGET_IS_DREAMCAST AND DREAMCAST_USE_FILE_POOL)`. The template's own `CLAUDE.md` forbids it ("Do NOT
cherry-pick … `--wrap=fopen` file pool").
**HP2 state:** absent (correctly not copied by `e47a3c7`). **ACTION: NOT-APPLICABLE — do not copy.**

### 3.4 `Core/CMakeLists.txt`

| Aspect | Template (`/home/ion/engines/engine-ut99/Source/Core/CMakeLists.txt`) | HP2 (`Source/Core/CMakeLists.txt`, `e47a3c7`) | Verdict |
|---|---|---|---|
| Base list | 15 files: UnProp UnObj UnCorSc UnName UnMem UnMisc UnMath UnCoreNet UnClass UnCache UnBits UnAnsi Core UFactory UExporter | same 15 **+ `UnLinker.cpp`** (433 split the linker out of UnObj.cpp) **+ `UnCoreNative.cpp`** | correct for 433. `UnCoreNative.cpp` compiles to an empty TU while `__STATIC_LINK` is undefined — harmless; may be dropped from the list (see §6) |
| Platform layer | MSVC → UnVcWin32; else (Windows→UnMinGW, else UnGUID) + UnUnix | identical | AC |
| `UnStaticExports.cpp` under `BUILD_STATIC` | yes | yes | AC |
| `UnFilePool.cpp` under Dreamcast | yes | removed | correct (N/A) |
| `-Og` on `UnCorSc.cpp`/`UnProp.cpp` for non-Debug GCC | yes (ut99dc FIXME: FFrame::Step reference juggling) | yes | AC — HP2's `UnCorSc.cpp` is +208 lines vs Epic (433 debugger/`EX_DebugInfo`, latent-action changes); the same GCC aliasing hazard applies. Keep `-Og`. Also consider `-fno-strict-aliasing` (already global) |
| `CORE_EXPORTS`, `UPACKAGE_NAME=${PROJECT_NAME}` | yes | yes | AC — `UPACKAGE_NAME` feeds `THIS_PACKAGE` (Core.h hunk) |
| SDL link for `USE_SDL` | yes | yes | AC (UnUnix.cpp uses `SDL_GetPerformanceCounter`, `SDL_GetPlatform`) |
| Files NOT listed | UnGUID on Windows; UnVcWin32 off-MSVC | also not listed: **`AudioMutex.cpp`**, `UnPSX2.cpp` (PS2), `UnVcWin32.cpp` | `AudioMutex.cpp` omission must be a decision — see §6 (it defines `GAudioMutex`-style symbols referenced from Engine/ALAudio?) |

**ACTION:** keep HP2's list; decide `AudioMutex.cpp` (§6) and optionally drop `UnCoreNative.cpp`.

## 4. Per-file hunk analysis: Inc/

Each file: a hunk table (template hunk → why → HP2 site → classification → ACTION), then file-level
strategy. Line numbers: **T** = template file, **H** = HP2 file, **E** = Epic file.

### 4.1 `Inc/Core.h` — dry run CLEAN (epic→tmpl 37, epic→HP2 142)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | Removes E:34-43 `#if !ASM && !__GNUG__ #define __asm ERROR_ASM_NOT_ALLOWED` and `#if __UNIX__ #include "UnUnix.h" <signal.h>` | cbdb054. `__UNIX__` is no longer defined (UnGnuG.h hunk 1); `UnUnix.h` is included only by `UnUnix.cpp`/`UnGUID.cpp` (T `Src/UnUnix.cpp:57`, `Src/UnGUID.cpp:30`) | H:71-80 identical to Epic | AC | take. Nothing else in HP2's built packages keys on `__UNIX__` except `Src/UnMisc.cpp:1629` (see §5 UnMisc/§8 FFileStream — **`#if !__UNIX__` becomes TRUE** and `FFileStream::Destroy` would spin forever waiting for a streaming thread that does not exist: must be re-keyed) and `Core.h:55` `SUPPORTS_PRAGMA_PACK` (below) |
| 2 | T:63-69 `#elif defined(PLATFORM_WIN32) typedef ANSICHARU TCHARU;` | 301a9ca (MinGW `<tchar.h>` sets `_TCHAR_DEFINED`) | H:108-111 identical to Epic | AC | take |
| 3 | T:348-356 `THIS_PACKAGE` = `GPackage##UPACKAGE_NAME` under `UNREAL_STATIC`; `extern "C" DLL_EXPORT char THIS_PACKAGE[]` | 17b0864 — one `GPackage` symbol per package in one binary | H:473 Epic's `extern "C" DLL_EXPORT TCHAR GPackage[];` | AC | take; then **every `GPackage` use in HP2 must become `THIS_PACKAGE`** — the template changed `IMPLEMENT_PACKAGE` (UnObjBas.h) and `IMPLEMENT_CLASS`. Check HP2 for other direct `GPackage` users: `grep -rn "GPackage" Source/{Core,Engine,Render,Fire,SoftDrv,Editor}` (HP2 `UnObjBas.h` `IMPLEMENT_CLASS` uses `GPackage` in both branches — §4.9) |
| 4 | T:365 `#include "UnStaticExports.h"` before `UnObjBas.h` | 17b0864 | H:482 insertion point identical | AC | take (header already in HP2) |
| 5 | `#include "UnCid.h"` → `"UnCId.h"` | cbdb054 — case-sensitive FS | H:492 **already `UnCId.h`** | PRESENT | none |
| 6 | T:381-384 `CORE_API TArray<FString> appFindFiles( const TCHAR* Spec );` | ee0ff8e (DCUtil) — implemented in T `Src/UnAnsi.cpp:220` | none | AC but optional | take (harmless; only DCUtil calls it). If HP2's `UnAnsi.cpp` merge drops the definition, drop this too |

**HP2-only content in Core.h to keep / fix (no template counterpart):**
- `class FTime` (H:24-53, 32.32 fixed point in `long long` under `__GNUG__`) — keep; it is the 433 time type
  (see §7.4). It is defined **before** `UnGnuG.h` is included, which is why `UnGnuG.h` can declare
  `FTime appSeconds()`.
- `SUPPORTS_PRAGMA_PACK` (H:55-59) is keyed on `__LINUX__`, which is only ever defined *after* this line (by
  UnGnuG.h) and not at all with the template's UnGnuG.h → **0 on every GCC/clang target**. Its only users wrap
  third-party codec headers in `#pragma pack(push,8)` (`Src/UnVcWin32.cpp:654`, `ALAudio/Src/ALAudioSubsystem.cpp:15`).
  Under the web/MinGW `-fpack-struct=4` that wrap is exactly what keeps `OggVorbis_File` (doubles, `ogg_int64_t`)
  laid out like the separately compiled libvorbis. **ACTION:** `#if _MSC_VER || __ICC || __GNUC__` → 1
  (GCC and clang both implement `#pragma pack(push,n)`). Owner: whoever ports ALAudio→NOpenALDrv; flag it there.
- `FFileStream`/`FStream`/`EFileStreamType` (H:344-411) + `GFileStream` — keep the declarations; the
  implementation is split between `Src/UnMisc.cpp` (portable) and `Src/UnVcWin32.cpp` (Win32-only
  `Read`/`Create`/`Destroy(INT)` + the streaming thread) → **Core does not link on GCC** until §8.1 is done.
- `GSecondsPerCycle` is `FLOAT` (H:437), `GTempTime` is `FTime` — keep; the template's `UnGnuG.h`/`UnUnix.cpp`
  declare/define `DOUBLE` versions that must be adapted (§4.2 hunk 11, §5 UnUnix.cpp).
- `GCurrentViewport`, `GEncryptLoad`, `GCurrentEncrypt`, `GDebugger`, `GAudioMaxRadiusMultiplier`,
  `GAudioDefaultRadius` (defined in `Src/Core.cpp:169-174`) — keep.
- `FMalloc::MemSize/SetTag/GetTag` virtuals (H:224-226) — keep; FMallocAnsi (template hunk) must still compile
  (they have defaults).
- `FFileManager::Init(UBOOL Startup)` virtual (H:340) — keep; the template's FFileManagerLinux hunk does not
  override it (fine, default is empty).
- `#if __STATIC_LINK #include "UnCoreNative.h"` (H:496-498) — dead; delete with the other `__STATIC_LINK`
  branches (§6).
- `class UDebugger` (H:501-505) — keep (UnCorSc.cpp calls `GDebugger->DebugInfo`).

**File strategy:** take the CLEAN merge output as is, then apply the `SUPPORTS_PRAGMA_PACK` fix and delete
the `__STATIC_LINK` include. Verify: `grep -n "__UNIX__\|__LINUX__" Source/Core/Inc/Core.h` → only the pragma line.

### 4.2 `Inc/UnGnuG.h` — dry run CONFLICTS=12 (epic→tmpl 288, epic→HP2 89)

The GCC platform header. The template's file is a rewrite (cbdb054 + 17b0864 + ee0ff8e + d5a6bfd +
b40994b + 301a9ca + 76df82e); HP2's is Epic's plus a `__PSX2_EE__` branch, a bigger signal set and the
FTime API. Hunks of epic→template:

| # | Template hunk | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:10-17 drop `#ifdef __LINUX_X86__ … #else #error Unsupported platform`; define `__WIN32__` on `PLATFORM_WIN32`; `__INTEL__`/`__INTEL_BYTE_ORDER__` unless `PLATFORM_BIG_ENDIAN`; **no `__UNIX__`/`__LINUX__`, no `ASMLINUX 1`** | cbdb054/17b0864 — the build no longer passes `-D__LINUX_X86__`; the platform comes from CMake's `PLATFORM_*` | H:10-33 Epic's `__LINUX_X86__` + a `__PSX2_EE__` branch | ADAPT | take the template's; delete the PSX2 branch (PLAN rule 2). HP2 root CMake defines no `__LINUX_X86__` either, so HP2's own file hits `#error Unsupported platform` today |
| 2 | T:20-46 remove `class __Context` (setjmp/`sigaction` SIGSEGV guard); add `<stdarg.h> <math.h> <stddef.h> <stdint.h> <stdio.h> <alloca.h> <string.h> <strings.h> <ctype.h> <malloc.h> <dirent.h> <limits.h>`, `PATH_MAX 4096` fallback, `<minwindef.h>` on Win32 | cbdb054 (guard via setjmp removed; guard/unguard are try/catch or nothing — UnFile.h hunk), d5a6bfd (`PATH_MAX` for Emscripten musl), 301a9ca (MinGW headers) | H:36-59 `__Context` with 9 `sigaction`s (`HandleSignal`, `Act_SIGHUP…SIGTERM`), `<sys/wait.h>` | ADAPT | take the template's. HP2's `__Context` is consumed only by HP2's `UnFile.h` guard macro (H `UnFile.h:117-118` `setjmp(__Context::Env)`) and defined in `UnUnix.cpp:778-850`; both are replaced by the template's versions (§4.8 UnFile.h, §5 UnUnix.cpp). setjmp/longjmp across C++ frames is UB and does not exist on wasm without Asyncify tricks — do not keep it |
| 3 | T:62-70 `#undef VOID` only when `!PLATFORM_WIN32` | 76df82e (winnt.h `#define VOID void` used by GL headers) | H:~78 Epic's unconditional `#undef VOID` | AC | take |
| 4 | T:72-75 `HANDLE`/`HINSTANCE` = `uintptr_t` unless `_WINDOWS_` | cbdb054/301a9ca | H:79-80 Epic's `DWORD` | AC | take |
| 5 | T:80 `enum {DEFAULT_ALIGNMENT = 8}` unchanged; HP2 changed it to **16** (H:83) | HP2 (PS2 VU0 16-byte vectors: `UnMath.h:657 #define FVECTOR_ALIGNMENT DEFAULT_ALIGNMENT`) | H:83 | ADAPT (HP2-side) | use **8** — the retail Windows value (`Inc/UnVcWin32.h:37`). 16 only served `__PSX2_EE__`'s 16-byte `FVector`; users (`UnMem.h` `New<T>`, `UnCache.h` `Create/Get`, `Render/Src/UnLight.cpp:2098,2165`) are alignment requests, not layouts, so 8 is safe and matches retail |
| 6 | T:83 `#define GCC_ALIGN(n) __attribute__((aligned(n)))` | 17b0864 | H:87 **same definition**; H also has `GCC_MOVE_ALIGN(n)` (empty) | PRESENT | keep `GCC_ALIGN`; drop `GCC_MOVE_ALIGN` (no user in HP2 `Source/`, verified by grep) |
| 7 | T:85-89 `DISABLE_OPTIMIZATION`/`ENABLE_OPTIMIZATION` = `_Pragma("GCC push_options") optimize("O0")` / `pop_options` | cbdb054 | H: Epic's empty macros | AC | take. HP2's users (`Engine/Src/UnPhysic.cpp:1421`, `UnAudio.cpp:22`, `UnScript.cpp:1235`, `Editor/Src/UnEdRend.cpp:11`, `UnEdAct.cpp:11`) spell it `#pragma DISABLE_OPTIMIZATION` exactly like Epic/the template (`Engine/Src/UnPhysic.cpp:1129` etc.), so behaviour is identical to the template's whatever GCC makes of it (an unknown `#pragma` is not macro-expanded → ignored under `-Wno-unknown-pragmas`) |
| 8 | T:92-126 `DLL_EXPORT` → **empty** (Epic: `extern "C"`); `STDCALL __stdcall`/`__cdecl` on Win32 only; `DLL_IMPORT` emptied under `UNREAL_STATIC`; `GCC_HIDDEN` (hidden visibility for dynamic builds only); `GCC_USED` | cbdb054/17b0864/301a9ca | H: Epic's | AC | take. `DLL_EXPORT` no longer carries `extern "C"`: every HP2 site that relied on it for C linkage must spell `extern "C"` (Epic already does at most sites; see UnObjBas.h `autoclass`, UnScript.h `int##cls##func`) |
| 9 | T:146-150 `COMPILER` = "Compiled with GCC[ (Debug)]" | cbdb054 — `"…("__VERSION__")"` is a C++11 user-defined-literal error | H:19,31 moved into the platform branches, still `"("__VERSION__")"` | ADAPT | take the template's (HP2's spelling does not compile as C++17) |
| 10 | T:153-202 `<stdint.h>` fixed-width typedefs; `DWORD`/`SIZE_T` from `<minwindef.h>` on Win32; `BITFIELD` == `DWORD` on Win32; drop `typedef unsigned int size_t`; `static_assert((char)-1 < 0)` | cbdb054/301a9ca | H:124-152: Epic's plus **`OWORD __attribute__((mode(TI)))`** and **without `typedef double DOUBLE`** | ADAPT | take the template's (which **keeps `DOUBLE`** — HP2 needs it: used in `Core/Inc/UnFile.h`, `Core/Src/UnAnsi.cpp`, `Engine/Src/UnParticleFX.cpp`, `Editor/Src/UnMeshLP.cpp`, ALAudio). Drop `OWORD`: no user, and `mode(TI)` (128-bit) is rejected by gcc `-m32` ("unable to emulate 'TI'") |
| 11 | T:244-246 `extern CORE_API DOUBLE GSecondsPerCycle; CORE_API DOUBLE appSecondsSlow();` | Epic API | H:193-195 **`FLOAT GSecondsPerCycle; FTime appSecondsSlow();`** | ADAPT (433 idiom) | keep HP2's types (they must match `Core.h:437` and `Src/Core.cpp:143`, and 70+ Engine call sites use `FTime` arithmetic). §7.4 |
| 12 | T:251-266 `appRound` = `(INT)floorf(F+0.5f)` | b40994b — Epic's GCC `(INT)F` truncates; caused the SoftDrv streaks | H:201-218: PSX2 asm / `#else return (INT)(F);` **with the parameter renamed `f` → does not compile** | ADAPT | take the template's body (drop PSX2) |
| 13 | T:271-287 `appFloor` = `(INT)floorf(F)` | b40994b | H:224-241 same PSX2 / `return (INT)F;` with parameter `f` → does not compile | ADAPT | take the template's |
| 14 | T:292-298 `appCycles()` and `appSeconds()` become **out-of-line** `CORE_API` declarations (defined in `UnUnix.cpp` via `SDL_GetPerformanceCounter`) — rdtsc asm removed | cbdb054 (ASM off; wasm has no rdtsc) | H:245-273 Epic's rdtsc inlines wrapped in `#if ASMLINUX`, `appSeconds` returns **`FTime`** | ADAPT | declare `CORE_API DWORD appCycles();` and **`CORE_API FTime appSeconds();`**; define both in UnUnix.cpp (template body, FTime return). Without `DEFINED_appSeconds` the generic fallback in `UnFile.h` would be used — make sure `DEFINED_appCycles`/`DEFINED_appSeconds` are still `#define`d as in T:291,296 |
| 15 | T:303-307 `appMemcpy` → `memcpy` | cbdb054 (no asm) | H:278-306 Epic's asm under `#if ASMLINUX` (so undefined → generic `appMemcpy` from `UnFile.h`) | AC | take |
| 16 | T:318-338 `strupr` inline, `strnicmp`→`strncasecmp`, `stricmp`→`strcasecmp` macros | cbdb054/ee0ff8e — replace the `UnUnix.cpp` definitions | H: none (HP2 `Src/UnUnix.cpp:736-770` defines them as functions) | ADAPT | take the macros and **delete** HP2 UnUnix.cpp's `stricmp`/`strnicmp`/`strupr` definitions (they would collide with the macros). Goes away naturally if the template's UnUnix.cpp is taken |
| 17 | T:216-223 `IMPLEMENT_PACKAGE_PLATFORM` → `extern "C" {BYTE GCC_USED DLL_EXPORT GLoaded##pkg;} STATIC_EXPORT(GLoaded##pkg, GLoaded##pkg)` under `UNREAL_STATIC`; Epic's (with `hInstance`) otherwise | 17b0864 — the registry entry `appGetDllHandle` looks for | H:175-176 `BYTE GLoaded##pkgname;` (HP2 dropped `hInstance`) | ADAPT | take the template's static branch. For the non-static branch keep HP2's (no `hInstance`) — a dynamic GCC build is not a target, so this barely matters |
| 18 | T:352 `extern CORE_API UBOOL GTimestamp` dropped from the `extern "C"` block (declared once above) | cbdb054 — duplicate declaration with conflicting linkage is an error in modern GCC | H:333 still duplicated | AC | take |

**File strategy — do NOT three-way merge.** Take the template's `UnGnuG.h` verbatim, then re-apply the
HP2 433 API on top (a ~10-line patch):
```cpp
// after the typedefs — HP2 433 time API (Core.h defines FTime before this header is included)
extern CORE_API UBOOL GTimestamp;
extern CORE_API FLOAT GSecondsPerCycle;      // HP2: FLOAT (Core.h:437, Core.cpp:143)
CORE_API FTime appSecondsSlow();
...
#define DEFINED_appCycles 1
CORE_API DWORD appCycles();
#define DEFINED_appSeconds 1
CORE_API FTime appSeconds();                 // HP2: FTime (Epic/template: DOUBLE)
```
Everything else HP2 added (`__PSX2_EE__`, the 9-signal `__Context`, `DEFAULT_ALIGNMENT 16`, `OWORD`,
`GCC_MOVE_ALIGN`) is dropped. `GCC_OPT_INLINE`, `appAlloca`, `TCHAR_TO_ANSI(str) str`, `ANSI_TO_TCHAR` are
identical in all three trees.

### 4.3 `Inc/FFileManagerGeneric.h` — CLEAN

| # | Template hunk | Why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | `DeleteDirectory`: `for(INT i…)` → `INT i; for(i…)` (the loop reuses `i` afterwards) | cbdb054 — pre-standard MSVC for-scope | H: Epic's | AC | take |
| 2 | trailing blank lines | 17b0864 | — | AC | irrelevant |

HP2-only content: `#include <sys/types.h> <sys/stat.h>` and a stat-based `GetGlobalTime`/stub `SetGlobalTime`
moved here from FFileManagerLinux.h. **HP2 bug (GCC blocker):** H:129 `SQWORD FFileManagerGeneric::GetGlobalTime( … )`
is written *inside the class body* with a qualified name → GCC: "extra qualification … on member". ACTION: drop
the `FFileManagerGeneric::` qualifier. Behavioural note for the web: HP2's `appInit` regenerates `<Game>.ini`/`User.ini`
whenever `GetGlobalTime("Default.ini") > GetGlobalTime(ini)` (`Src/UnMisc.cpp:1918,1937`, "//ft"); with a real
`stat` mtime and a Default.ini preloaded into MEMFS at every page load (mtime = now), **every boot would overwrite the
player's persisted User.ini/Game.ini**. Epic/the template return 0 from `FFileManagerLinux::GetGlobalTime`
(T `Inc/FFileManagerLinux.h:266-273`), which disables the comparison. ACTION: on `__EMSCRIPTEN__` return 0 (or keep
Epic's Linux stub on all GCC targets); verify on the second web boot that `User.ini` survives (see §10).

### 4.4 `Inc/FFileManagerLinux.h` — CLEAN (used on **every** GCC target incl. MinGW — the launcher picks it)

| # | Template hunk (T lines) | Why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:11-12 `#include <sys/stat.h>`, `<cerrno>` | cbdb054 (musl/emscripten do not pull them transitively) | Epic's | AC | take |
| 2 | T:233-238 `chmod(…, S_IRUSR|S_IWUSR)` instead of glibc-internal `__S_IREAD|__S_IWRITE`, skipped on Dreamcast | 17b0864 | Epic's | AC | take (the DC guard is dead code, harmless) |
| 3 | T:257-262 same in `Delete` | 17b0864 | Epic's | AC | take |
| 4 | T:287-293 `mkdir` mode fixed (Epic passed `__S_IREAD && __S_IWRITE && __S_IEXEC` = **1**, i.e. mode 0001) and Windows `mkdir(path)` without mode | cbdb054 + 301a9ca | Epic's | AC | take — HP2 saves games; a 0001-mode `Save` dir is unwritable natively |
| 5 | T:427-431 `getcwd` NULL fallback to "." | cbdb054 | Epic's | AC | take |

HP2-only: `FArchiveFileWriter` ctor sets `ArIsSaving = ArIsPersistent = 1` (H:140-142) — keep (433 behaviour; the
merge keeps it). HP2 deleted the Linux `GetGlobalTime`/`SetGlobalTime` stubs (see 4.3).

### 4.5 `Inc/FMallocAnsi.h` — CLEAN

| # | Template hunk | Why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | `DumpAllocs`: `malloc_stats()` under `PLATFORM_DREAMCAST` | 17b0864 | Epic's | N/A (dead code) | merge brings it in; harmless |

HP2-only: `#include "UnStat.h"` + `Clock(GStat.MemTime)` in `Malloc`/`Realloc`/`Free`, `check`→`checkSlow`. This
is the allocator both launchers use (`Game/Src/Launch.cpp:154`, `UCC/Src/UCC.cpp:81,86`). With `STATS 1`
(`Inc/UnBuild.h:56`) every allocation calls `appCycles()` twice; the template's `appCycles()` is
`SDL_GetPerformanceCounter()` (T `Src/UnUnix.cpp:912-919`) which on Emscripten is a JS `performance.now()` call.
**Perf hazard on web, not a correctness one.** ACTION (after first light): measure; if it shows, compile the web
build with `Clock` in FMallocAnsi disabled (e.g. `#if STATS && !defined(__EMSCRIPTEN__)`).

### 4.6 `Inc/FOutputDeviceAnsiError.h` — OURS-UNCHANGED → take the template's file verbatim

All three hunks are `PLATFORM_DREAMCAST` (forward-declared `FatalError`, a no-exceptions `Serialize`/`HandleError`)
→ **N/A** functionally, harmless as dead code. HP2's file == Epic's. ACTION: copy the template's file (the dry run
already did) — or keep HP2's; both are identical on every HP2 target. Prefer the template's (fewer diffs to track).

Note for the web: this device's `Serialize` does `throw( Msg )` on `appErrorf`; under Emscripten's default
`-fignore-exceptions` that is `abort()`. That is the template's accepted behaviour (errors abort; recoverable
load failures are patched to return NULL one site at a time — §5 UnObj.cpp).

### 4.7 `Inc/FOutputDeviceFile.h` — CONFLICTS=1 (trivial)

| # | Template hunk (T lines) | Why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:9 `#include <stdio.h>` | d5a6bfd | H:8 HP2 deleted the blank line at the same spot → the only conflict | AC (textual conflict only) | take the include |
| 2 | T:85-101 **GLog→stdout echo**: every non-`NAME_Title` line also goes to `stdout` as `Event: Data\n` (Module.print → browser console on web, terminal natively) | d5a6bfd (ufront 2.8) — "GLog→console" | H: Epic's | AC | take — this is how the web E2E and the frontend read engine milestones |

HP2-only: log filename starts from `appUserDir()` instead of `appBaseDir()` (H:42). Keep, but it requires the
Unix core to define `appUserDir()` (§5 UnUnix.cpp, §8.2). With `appUserDir()==""` the log lands in the cwd
(`System/Game.log`), same place as the template's.

### 4.8 `Inc/UnFile.h` — CLEAN (epic→tmpl 137)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:101-109 guard/unguard: the GCC `__Context` + `setjmp` variant is deleted; every compiler uses `try{…}catch(TCHAR*)…catch(...){appUnwindf…}` | cbdb054 — setjmp/longjmp across C++ frames is UB, and there is no SIGSEGV→longjmp on wasm | H:106-124 Epic's (with `__Context`) | AC | take (pairs with removing `__Context` from UnGnuG.h and `__Context::StaticInit` from UnUnix.cpp) |
| 2 | T:190-201 `Localize*( …, Package=THIS_PACKAGE …)` | 17b0864 — static build: each package's own `GPackage<Pkg>` | H: Epic's | AC | take |
| 3 | T:279 `appStrstrFs` declaration | 17b0864 (Dreamcast FATFS) — implemented in T `Src/UnAnsi.cpp` | none | N/A-ish | harmless declaration; take only if UnAnsi.cpp's definition comes along (it is DC-motivated; not needed) |
| 4 | T:288-290 `appSpc/appStrncpy/appStrncat( …, INT …)` instead of `int` | cbdb054 — must match the definitions exactly | H: Epic's `int` | AC | take |
| 5 | T:308 `typedef INT QSORT_RETURN` | cbdb054 | H: Epic's | AC | take |
| 6 | T:353-369 `appIsGlobalOptionToken()` + `appApplyIniOverrides()` declarations | 1d3dfa5 / 2c25d55 — `Key=Value` first token is not a map URL; `-ini:<File>:[<Section>]:<Key>=<Value>` | none | AC | take (definitions in UnMisc.cpp §5; callers: the launcher / UGameEngine::Init — template-launcher / template-engine reports) |
| 7 | T:420-431 `appMemzero` always declared; `appMemmove/appMemset/appMemcpy` declared unless DC (DC maps them to libc) | 17b0864 | H: Epic's `#ifndef DEFINED_appMemcpy` guards | AC | take |
| 8 | T:435-459 `appMalloc/appFree/appRealloc` become inline functions that fall back to libc while `GMalloc` is still the pre-init error allocator (`GetMallocError()`) | d5a6bfd — static-init allocations (C++ runtime, WasmFS, SDL) before `appInit` installs GMalloc | H: Epic's `#define appMalloc GMalloc->Malloc` | AC | take; **requires** `CORE_API FMalloc& GetMallocError()` in `Src/Core.cpp` (Core.cpp hunk §5) |
| 9 | T:465-535 `operator new/new[]` take `size_t` (wasm32 `size_t` is `unsigned long`); the four replaceable forms get `UT_REPLACEMENT_ALLOC` = `always_inline` | d5a6bfd (2.7b) + ec74328 (2.19d: -O0 WasmFS `bad_weak_ptr`) | H: Epic's `unsigned int` | AC | take |
| 10 | (d5a6bfd/ec74328) `FArchiveTagUsed`-related declarations? — no: ec74328's UnFile.h part is the operator new change above | — | — | — | — |

HP2-only declarations the merge keeps and the **Unix core must define** (they are only defined in `UnVcWin32.cpp`
today, which GCC never compiles): `appUserDir()` (callers: `Src/UnMisc.cpp:1915,1935,2002`, `Inc/FOutputDeviceFile.h:42`,
`Engine/Src/UnCamera.cpp:665,899`, `Engine/Src/UnReplay.cpp:40`), `appProcessSeconds()` (callers
`Engine/Src/UnGame.cpp:1465`, `Render/Src/UnRender.cpp`), and the 433 `void* appCreateProc( URL, Parms, UBOOL bRealTime )`
+ `UBOOL appGetProcReturnCode( void*, INT* )` (callers `Engine/Src/AStatLog.cpp`, `Engine/Src/UnAudio.cpp` — HP2's
Loki `UnUnix.cpp:318-350` has them). `appMsgf`/`appGetLastError`/`appDebugMessagef` have no caller outside
`UnVcWin32.cpp` → declaration only, no definition needed. `appSaveSlotDir`/`appSetSaveSlotDir`,
`Save/LoadPersistentActorCache`, `appTrimSpaces`, `appFromAnsi(TArray<TCHAR>&,…)` are defined in portable
`Src/UnMisc.cpp`; `appAcos`, `appRandRange`, `appFrand(From,To)` in `Src/UnAnsi.cpp`. HP2 also changes
`check(expr)` to expand to `0` when `!DO_CHECK` — keep. `FPushMemTag` (H:506-517) calls `GMalloc->SetTag` — keep
(default no-op in `FMalloc`).

### 4.9 `Inc/UnObjBas.h` — CONFLICTS=3

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:50 `enum ENoInitConstructor {EC_NoInitConstructor};` | cbdb054 | H:50 has `EStaticConstructor` instead (433 static-link) | AC | take (add beside HP2's enum); used by nothing in Core — harmless |
| 2 | T:297-319 `DECLARE_CLASS`: `InternalConstructor` does `new((EInternal*)X)TClass;` — **no parentheses** | a680288 — value-init zeroed the UObject header (Class==NULL, cut hash chains → `Failed to find object` on joins) | H:329-337 **4-arg** `DECLARE_CLASS(TClass,TSuperClass,TStaticFlags,TPackage)`, `new(…)TClass();` | ADAPT | keep HP2's 4-arg signature; drop the parens. (Conflict 1) |
| 3 | T:334-352 `IMPLEMENT_CLASS`: single GCC/MSVC-agnostic form using `THIS_PACKAGE`, `DLL_EXPORT UClass* autoclass##TClass = …; STATIC_EXPORT( TClass, autoclass##TClass )` | 17b0864 (static registry) | H:347-414 `#if _MSC_VER` / `#elif !__STATIC_LINK` / `#else` (PS2 `GetPrivateStaticClass`) | ADAPT | replace all three branches with the template's single definition. (Conflict 2) |
| 4 | T:354-357 `IMPLEMENT_PACKAGE(pkg)` = `extern "C" { DLL_EXPORT GCC_HIDDEN TCHAR THIS_PACKAGE[] = TEXT(#pkg);} IMPLEMENT_PACKAGE_PLATFORM(pkg)` | 17b0864 | H:425-434 Epic's two variants with `GPackage` | ADAPT | take the template's single definition. (Conflict 3) |
| 5 | T:439 `UObject::InternalConstructor` → `new(…)UObject;` | a680288 | H: Epic's `UObject()` in HP2's UObject | AC | take |
| 6 | T:~893 `ConstructObject<T>(UClass*,…)` **moved out** to UnClass.h (needs a complete `UClass` for `Class->IsChildOf`) | cbdb054 (GCC two-phase lookup) | H:989-995 Epic's position **plus** an HP2-only overload `ConstructObject<T>(UObject* Parent, FName, DWORD)` (H:998-1001) that calls the first | ADAPT | move **both** templates to UnClass.h after `class UClass` (first the `UClass*` one, then HP2's). The clean UnClass.h merge already adds the first there → if UnObjBas.h keeps its copy the build fails with a redefinition |

HP2-only content to keep: `EStaticConstructor` + the `UObject(EStaticConstructor,…)` ctor declaration (harmless),
`SetClass()`, `ImportPostProcess`, the 433 natives (`execDynArrayCount`, `execRandomSpreadVector`, `execStringToName`,
`execRandRange`, `execGetLanguage`, `execDebugInfo`). Delete: the `__STATIC_LINK` `DECLARE_BASE_CLASS` branch
(H:302-325), the `__STATIC_LINK` `IMPLEMENT_CLASS` branch (H:386-420), `AUTO_INITIALIZE_REGISTRANTS` (H:1079-1110).

Resolved form (what HP2 should end up with):
```cpp
#define DECLARE_BASE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
public: enum {StaticClassFlags=TStaticFlags}; \
	private: static UClass PrivateStaticClass; public: \
	typedef TSuperClass Super; typedef TClass ThisClass; \
	static UClass* StaticClass() { return &PrivateStaticClass; } \
	void* operator new( size_t Size, UObject* Outer=(UObject*)GetTransientPackage(), FName Name=NAME_None, DWORD SetFlags=0 ) \
		{ return StaticAllocateObject( StaticClass(), Outer, Name, SetFlags ); } \
	void* operator new( size_t Size, EInternal* Mem ) { return (void*)Mem; }
#define DECLARE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
	DECLARE_BASE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
	friend FArchive &operator<<( FArchive& Ar, TClass*& Res ) { return Ar << *(UObject**)&Res; } \
	virtual ~TClass() { ConditionalDestroy(); } \
	static void InternalConstructor( void* X ) { new( (EInternal*)X )TClass; }   /* a680288: no parens */
// IMPLEMENT_CLASS / IMPLEMENT_PACKAGE: the template's (THIS_PACKAGE + STATIC_EXPORT), verbatim.
```
Optional convenience (decide once, in §7.1): make the fourth parameter variadic
(`DECLARE_CLASS( TClass, TSuperClass, TStaticFlags, ... )`) so the template's drivers copied UNADAPTED (NSDLDrv,
NullDrv, SoundDrv, NOpenALDrv, IpDrv, WebRTCDrv, XOpenGLDrv/UTGLR forks) compile with their 3-arg calls. The
package argument is ignored by the non-`__STATIC_LINK` path, so nothing changes at run time.

### 4.10 `Inc/UnScript.h` — CONFLICTS=6

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:28-31 non-MSVC `IMPLEMENT_FUNCTION` adds `STATIC_EXPORT( int##cls##func, int##cls##func )` | 17b0864 — `UFunction::Bind` finds `int<Class>exec<Func>` through the registry | H:27-30 Epic's | AC | take — this is what replaces HP2's `MAP_NATIVE` tables (§6) |
| 2 | T:65-67 `P_GET_SKIP_OFFSET` reads the `_WORD` with `__builtin_memcpy` on **every** platform | 17b0864 (unaligned) | H:64-68 `#if __PSX2_EE__` appMemcpy / `#else` Epic's deref | ADAPT | take the template's unconditional memcpy; drop the PSX2 guard |
| 3 | T:123-175 `FFrame::ReadInt/ReadObject/ReadFloat/ReadWord/ReadName` memcpy under `PLATFORM_DREAMCAST`, plain deref otherwise | 17b0864/03dc982 | H:125-181 same idea under `__PSX2_EE__` | N/A (both guards are console-only) | keep the plain deref (the template proves it on x86 and wasm — wasm loads tolerate misalignment), delete both guards. (Optional, stricter: unconditional `memcpy` — same codegen on x86/wasm, no UB) |

HP2-only content to keep: `P_FINISH` also steps an `EX_DebugInfo` token (`Stack.Code++; if(*Stack.Code==EX_DebugInfo) Stack.Step(…)`,
H:70-71) — **required**: the shipped HP2 `.u` bytecode can contain the 433 debugger opcode `EX_DebugInfo` (0x38,
UnStack.h); `FFrame::Parent` initialisation in both ctors (H:109,116); `P_GET_FLOAT_REF` `0.f`.

### 4.11 `Inc/UnStack.h` — CONFLICTS=1

| # | Template hunk | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:207 `virtual ~FFrame() {}` after the ctors | 17b0864 (derived frames destroyed through `FFrame*`; GCC `-Wdelete-non-virtual-dtor`) | H:218-219 HP2 changed the ctor on the neighbouring line (`FFrame* InParent = NULL`) → textual conflict | AC (semantic) | keep HP2's ctor + add the virtual dtor. FFrame already has a vtable (FOutputDevice), so no layout change |

HP2-only (keep, **never** take Epic/template values): the 433 opcode renumbering — `EX_LineNumber 0x15`,
`EX_DynArrayCount 0x37`, `EX_DebugInfo 0x38`, `EX_GlobalFunction 0x39`, conversions shifted to `0x3a…0x46`,
`EX_StringToName 0x5a`; `FFrame::Parent`, `FFrame::Describe()`. The shipped `.u` files were compiled against these
numbers (see §7.3).

### 4.12 `Inc/UnCoreNet.h` — CLEAN

| # | Template hunk | Why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | `INT FClassNetCache::GetRepConditionCount()` → `INT GetRepConditionCount()` (extra qualification inside the class) | cbdb054 — GCC error | H:39 Epic's | AC | take |

HP2: `DECLARE_CLASS(UPackageMap,UObject,CLASS_Transient,Core)` 4-arg — keep.

### 4.13 `Inc/UnCache.h` — CLEAN

| # | Template hunk | Why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | `HASH_COUNT` 8192 under `PLATFORM_LOW_MEMORY` | 17b0864 (Dreamcast) | H: Epic's | N/A | merge brings in dead code; harmless |

HP2-only: `GHash( QWORD Val )` folds the 64-bit id to a DWORD first (H:103-107) — keep (it is the correct
signature for `QWORD` cache ids; Epic truncated implicitly).

### 4.14 `Inc/UnClass.h` — CLEAN

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | `TFieldIterator<T>` moved from before `UStruct` to after it (T:214-272) | cbdb054 — its inline members dereference `UStruct` (`InStruct->Children`, `GetInheritanceSuper()`), invalid on an incomplete type under two-phase lookup | H: Epic's position | AC | take |
| 2 | T:500-507 `ConstructObject<T>(UClass*, …)` defined after `UClass` | cbdb054 — needs complete `UClass` | none (HP2 has it in UnObjBas.h) | ADAPT | see 4.9 #6 — move HP2's two `ConstructObject` overloads here, delete them from UnObjBas.h |

HP2-only (keep): 4-arg `DECLARE_CLASS(…,Core)` everywhere; `EStaticConstructor` ctors on UField/UStruct/UState/UClass;
**`UStruct::PropertiesAlign`** (433 — struct alignment computed in `Link`, H:204; interacts with the layout guard,
§5 UnClass.cpp); `UStruct::SerializeTaggedPersistentProperties` (HP2 save-game persistence); `UClass::ClassCRC` +
`GetClassCRC()`; `AddDependency` moved out-of-line; `GetScriptTextCRC()` removed.

### 4.15 `Inc/UnMath.h` — CONFLICTS=5 (epic→tmpl 346, epic→HP2 1410)

Epic→template hunks (`diff --strip-trailing-cr -w`, E line → T line):

| # | Template hunk | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | E:8 → T:8-12 `#ifdef PLATFORM_DREAMCAST #include "dc/fmath.h" #elif PLATFORM_LOW_MEMORY #include <math.h>` | 17b0864 (DC) | none | N/A | skip |
| 2 | E:837 → T:842-884,905,909,915 `FGlobalMath` gets DC / LOW_MEMORY `Sqrt/SinTab/CosTab/SinFloat/CosFloat` computed forms and drops the trig/sqrt tables under `PLATFORM_LOW_MEMORY` | 17b0864 (DC RAM) | HP2 `FGlobalMath` is structurally different (433/436-era approximations: `SqrtManTbl`, `DivSqrtManTbl`, `DivManTbl`, …`ExpTbl`, `SqrtApprox/DivSqrtApprox/DivApprox/Power`; the MSVC `POWER_ASM` block is already `#if 0` at H:1542-1586, with the C `#else` at H:1587+) | N/A | keep HP2's `FGlobalMath` as is |
| 3 | E:905-1168 deleted (T:953): the `#if ASM` MSVC `ASMTransformPoint/ASMTransformVector` and the `#elif ASMLINUX` AT&T versions | cbdb054 — ASM off; the `ASMLINUX` bodies are multi-line string literals (`asm volatile("\n…`) | H:1679-1820 (`#if ASM`), H:1822-1878 (`#if ASM`), H:1880-1937 (`#if ASMLINUX`) | ADAPT (optional) | with `ASM`/`ASMLINUX` undefined (template UnGnuG.h never defines them) these are dead preprocessor groups. **Verified 2026-09-26:** HP2's *unmodified* `UnMath.h`, included through the template's other headers (`syn/` isolation set, see §10), passes `g++ -m32 -std=gnu++17 -fsyntax-only` and `clang++ --target=i686-linux-gnu` / `--target=wasm32-unknown-emscripten -fsyntax-only` with zero errors — the unterminated multi-line literals in skipped groups are tolerated by both. So deletion is hygiene, not a blocker. Recommended: delete them anyway (they are ~450 lines of noise that every future diff drags along) |
| 4 | E:1174-1204 → T:958-970 `TransformPointBy/TransformVectorBy` keep only the C body | cbdb054 | H:1943-2011 / H:2018-2079 `#if ASM / #elif ASMLINUX / #elif __PSX2_EE__ (VU0 asm) / #else` C body | ADAPT | keep only the `#else` bodies (H:2008-2010, H:2077-2078); delete the three other branches. Also dead code today — same "hygiene" status as #3 |
| 5 | E:1231,1254,1296,1305 → T:997,1020,1062,1071 `FPointsAreSame/FPointsAreNear/FParallel/FCoplanar` return `UBOOL` (Epic: `int`) | cbdb054 — match the `friend UBOOL` declarations in `FVector` | H:2112, 2135, 2177, 2186 still `inline int …` | AC | take for tidiness. Not a blocker: the template's `UnGnuG.h:177` has `typedef int32_t UBOOL;` and `int32_t` is `int` on gcc -m32, MinGW i686 and wasm32, so `friend UBOOL f()` + `inline int f()` is the same declaration (confirmed by the 0-error syntax check). It would only break on a target where `int32_t` is `long` (none of ours) |

HP2-only content (keep / delete):

- `#if __PSX2_EE__` PS2 `FVector` (16-byte, with `W`, VU0 `asm volatile("…` bodies) H:192-652 → **delete**; keep the `#else`
  `FVector` H:654-925 (12 bytes `X,Y,Z` — the on-disk and `-fpack-struct=4` layout the shipped packages expect).
  `FVECTOR_ALIGNMENT` (H:193/657) has **no user** in HP2 `Source/` (grep) → delete with the PS2 block.
- `RandomSpreadVector` (H:2515-2558, "Math added by Legend on 4/12/2000") — keep; it calls `appAcos` (declared in
  HP2 `Inc/UnFile.h`, defined in HP2 `Src/UnAnsi.cpp`) and backs the 433 native `execRandomSpreadVector`. When the
  template's `UnAnsi.cpp` hunks are merged, make sure `appAcos` survives (it is HP2-only).
- `TTypeInfo<FVector/FPlane/FCoords/FRotator/FScale/FQuat/FPlace>::FastSerialize()` under `__INTEL_BYTE_ORDER__`
  (H:2940-2972) — keep. The template's UnGnuG.h defines `__INTEL_BYTE_ORDER__` unless `PLATFORM_BIG_ENDIAN`
  (§4.2 hunk 1), so fast bulk serialization stays on; it is correct only while these structs have no padding —
  true under both natural and `-fpack-struct=4` layout (all members are 4-byte). Add a `static_assert(sizeof(FVector)==12)`
  etc. next to them (cheap insurance; the static single binary makes it trivial).
- `(DWORD&)F` / `(FLOAT&)A` type punning in `SqrtApprox`/`Power` (H:1588-1600) — relies on `-fno-strict-aliasing`
  (global in the template's root CMake — keep it global in HP2).

**File strategy:** do **not** 3-way merge (1410 HP2-changed lines against 346 template lines, all template lines
are DC or ASM deletions). Keep HP2's file; hand-apply hunk 5; delete the PS2 `FVector` and the ASM/PS2
branches of the transform code (optional but recommended). Verify: `grep -n "asm\|__PSX2_EE__\|ASMLINUX\|^#if ASM" Source/Core/Inc/UnMath.h` → only the `#if 0` `POWER_ASM` block (or nothing if that too is deleted).

### 4.16 `Inc/UnMem.h` — CLEAN (epic→tmpl 18, epic→HP2 10)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:62-64 the three `friend void* operator new( size_t, FMemStack&, …)` declarations lose their default arguments; T:125,132,141 the definitions gain them | cbdb054 — GCC: "default argument given for parameter N of … after previous specification" (Epic had defaults on both) | H: Epic's text at the same place (HP2's delta is elsewhere in the file) | AC | take (the clean merge does) |
| 2 | T:114 `NewOned`: `BYTE* Result = Mem.PushBytes(…)` replaces Epic's `return (T*)Mem.PushBytes(…); appMemset( Result, … )` (dead code referencing an undeclared `Result`) | cbdb054 — hard error once the template is instantiated/parsed by a conforming compiler (`Result` is non-dependent → error at definition) | H: Epic's (same bug) | AC | take |
| 3 | T:137 `appMemzero(Result,…)` → `appMemset(Result,0,…)`; T:146 `0xff` → `255` | cbdb054, cosmetic | H: Epic's | AC | take |

HP2-only content: `inline BYTE* PushBytes` (harmless); `#if __PSX2_EE__ Align = Max(Align,16);` (H:35-37) → **delete**;
`Init( INT DefaultChunkSize, const TCHAR* InTag )` + `const TCHAR* Tag` member (H:60, H:88) → **keep** (HP2 callers pass a tag,
e.g. `GMem.Init( 65536, TEXT("GMem") )` in `Src/UnMisc.cpp` `appInit`; every `FMemStack::Init` call in Engine/Render must
then pass two args — the template's drivers copied in (NSDLDrv, XOpenGLDrv, UTGLR, SoftDrv) do not call `FMemStack::Init`,
verify with `grep -rn "\.Init(\s*[0-9A-Z_]*\s*)" Source/*/Src | grep -i mem` after copying them).

**File strategy:** take the CLEAN merge output, delete the PSX2 3-liner.

### 4.17 `Inc/UnTemplate.h` — CONFLICTS=9 (epic→tmpl 74, epic→HP2 679)

**The 433 idiom that changes everything here:** HP2 inverted Epic's array hierarchy. Epic: `class FArray` (untyped
`void* Data; INT ArrayNum, ArrayMax;`) ← `template<T> class TArray : public FArray`. HP2 433:
`template<T> class TRefArray { protected: T* Data; INT ArrayNum; }` (H:206-352, "static array, cannot grow")
← `template<T> class TArray : public TRefArray<T>` (H:353, adds `INT ArrayMax`) ← `class FArray : public TArray<BYTE>`
(H:682, exposes only the `ElementSize` overloads). Memory layout is unchanged (`Data, ArrayNum, ArrayMax` = 12 bytes,
same as Epic and as the serialized/`UArrayProperty` view — `(FArray*)` casts still work), **but `TArray<T>` now has a
*dependent* base**, so every unqualified `ArrayNum`/`Data` inside `TArray<T>` members is a two-phase-lookup error under
GCC/clang. The template's `this->` hunks were written against Epic's non-dependent `FArray` base and do not cover this.

Evidence (reproduced 2026-09-26, `tcore-private/syn`): the 3-way-resolved HP2 `UnTemplate.h` included from the template's
headers gives **28 errors** under `g++ -m32 -fsyntax-only` (`'ArrayNum' was not declared in this scope` at H:373, 379,
384, 394-404, 456-556, 595-649 and `'Pairs'` at H:1639); adding the two using-declarations below makes the whole Core
header set compile with **0 errors** under g++ -m32, clang i686 and clang wasm32.

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:230-236, 257-263 `#ifdef PLATFORM_LOW_MEMORY` array growth `ArrayNum/8+8` in `FArray::Add/Insert` | 17b0864 (DC) | H: `TArray<T>::Add/Insert` at ~H:479/499 (moved by 433) | N/A | skip (it is part of 2 of the 9 conflicts: resolve to HP2's text) |
| 2 | T:683, 720, 729-733, 753 `TTransArray::Empty/AddUniqueItem/RemoveItem/ModifyAllItems` use `this->ArrayNum`, `this->Num()` | cbdb054 — dependent base `TArray<T>` | H:799, 829-841, 856 | AC (in conflicts) | take (resolved text shown in the diff below) |
| 3 | T:944 `TLazyArray::Unload` → `this->Empty()` | cbdb054 | H:1034 | AC | take |
| 4 | T:1100-1110 `FString::Left/LeftChop/Right` use `Clamp<INT>(…)` | cbdb054 — `Clamp(INT,int,INT)` deduction conflict on targets where `INT` is `int32_t` ≠ `int` (DC); harmless elsewhere | H:1208-1218 | AC | take |
| 5 | **T:1112-1116 `FString::Mid`: `DWORD End = (DWORD)Start + (DWORD)Count;`** | **1f744b8** — "`Mid(Start)` defaults `Count=MAXINT`; signed `Start+Count` overflows (UB); clang -O2 turned it into a 2 GB allocation" | H:1220 `DWORD End = Start+Count;` — identical bug | AC | **take**, with the comment |
| 6 | T:1358, 1400, 1463, 1472 `typename TTypeInfo<TK>::ConstInitType` in `TMapBase::TPair` ctor, `Add`, `Set`, `Remove` | cbdb054 — dependent type names need `typename` | H:1466, 1502, 1553, 1560 | AC (in conflicts: HP2 removed adjacent `guard` lines) | take |
| 7 | T:1566-1604 `TMultiMap::MultiFind/Add/AddUnique/RemovePair/FindPair` use `this->Hash/HashCount/Pairs/Relax` + `typename` | cbdb054 | H:1653-1685 | AC (in conflicts) | take |

HP2-only fixes the template never needed (**required**, GCC/clang blockers):

```cpp
// Inc/UnTemplate.h, inside template<class T> class TArray : public TRefArray<T>  (right after "public:", H:355)
	using TRefArray<T>::Data;
	using TRefArray<T>::ArrayNum;
// Inc/UnTemplate.h, inside template<class TK,class TI> class TMap : public TMapBase<TK,TI>  (after "public:", H:1628)
	using TMapBase<TK,TI>::Pairs;      // HP2 added TMap::Num() { return Pairs.Num(); }
```
The resulting file is saved at `…/scratchpad/tcore-private/syn/UnTemplate.fixed.h` (resolved merge + these 3 lines);
its diff against HP2's current file is exactly hunks 2-7 above plus the using-declarations.

Latent instantiation-time hazards (`-fsyntax-only` does not instantiate templates; check at the first real compile):
- `TTransArray<T>::Insert` (H:785-790) calls `FArray::Insert( Index, Count, sizeof(T) )`, but `TTransArray<T>` derives
  from `TArray<T>`, **not** from `FArray` → ill-formed if instantiated. Engine instantiates `TTransArray<AActor*>`,
  `<FPoly>`, `<FBspNode>`, `<FVert>`, `<FVector>`… (Engine `UnLevel.h`, `UnModel.h`); the Editor's BSP code may call
  `.Insert(` on them. ACTION: change to `TArray<T>::Insert( Index, Count )` (same semantics, `sizeof(T)` implied).
- Everything else in `TTransArray` casts `(FArray*)this` for `GUndo->SaveArray` — a reinterpret between unrelated
  classes of identical layout; fine with `-fno-strict-aliasing`, keep.
- Any other member of `TArray<T>`/`TLazyArray<T>`/`TTransArray<T>` calling a base member function unqualified
  (e.g. `Realloc`, `Num`, `Remove`) resolves at instantiation only if the name is also visible as a member of the
  class itself; the first Engine compile will list them — fix with `this->` one by one.

**File strategy:** 3-way merge (`git merge-file`, base=epic, ours=HP2, theirs=template), resolve the 9 conflicts toward
"HP2 text + template's `this->`/`typename`/`Clamp<INT>`/`Mid` tokens" (i.e. take `syn/UnTemplate.fixed.h` wholesale),
then compile.

### 4.18 `Inc/UnUnix.h` — OURS-UNCHANGED → take the template's file verbatim

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:9 `#include <cstring>` | cbdb054 — `strcasecmp` etc. used by the UnGnuG.h shims when this header is included first | HP2 == Epic | AC | take |
| 2 | T:25-31 `#ifdef PLATFORM_WIN32 CORE_API void appWinSockInit(); #endif` | 301a9ca (2.33) — defined in `Src/UnMinGW.cpp` (already in HP2), called from `appComputerName`/`appGetLocalIP` (UnUnix.cpp) and IpDrv | absent | AC | take |

`appGetGUID( void* )` (T/H:22-24) is identical in both. **File strategy:** copy the template's file.

## 5. Per-file hunk analysis: Src/ (pending)

## 6. HP2-only Core files and what to do with each (pending)

## 7. Build-433 idioms that change the template's approach (pending)

## 8. HP2 Core portability problems the template's hunks do not cover (pending)

## 9. Merge strategy per file + ordered checklist (pending)

## 10. Open questions / how to verify (pending)

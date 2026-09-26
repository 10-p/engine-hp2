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

## 5. Per-file hunk analysis: Src/

Same format as §4. `T`/`H`/`E` line numbers are into the template / HP2 / Epic file of the heading. "cc probe" =
the previous run's syntax-only compile of the dry-run merge output (`merge3.sh` result with the conflicts
resolved toward the template, headers = the §4 resolutions) under `g++ -m32 -std=gnu++17 -fsyntax-only` and
emsdk `clang++ --target=wasm32-unknown-emscripten -fpack-struct=4 -fsyntax-only`
(`<scratchpad>/tcore-private/cc/{gcc,wasm}_<file>.log`); every error quoted from it was re-mapped to the
**HP2** line below and re-read in the HP2 file on 2026-09-26.

### 5.1 `Src/Core.cpp` — dry run CLEAN (epic→tmpl 6)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:33-38 `CORE_API FMalloc& GetMallocError() { return MallocError; }` right after the `FMallocError … } MallocError;` static | d5a6bfd — accessor used by the inline `appMalloc/appRealloc/appFree` of the template's `UnFile.h` (T `Inc/UnFile.h:438-462`: while `GMalloc == &GetMallocError()` the call goes to libc, so C++-runtime / WasmFS / SDL static initialisers that allocate before `appInit` installs `GMalloc` do not hit `FMallocError::Called` → `appErrorf`) | H:21-39 `class FMallocError … } MallocError;` identical to Epic; H:130 `GMalloc=&MallocError` | AC | take (merges clean). **Must** land together with §4.8 hunk 8 (UnFile.h), otherwise either the header references an undefined symbol or HP2 keeps `#define appMalloc GMalloc->Malloc` and the web static-init hazard stays |

HP2-only content (keep, no template counterpart): runtime `PACKAGE_FILE_VERSION`/`PACKAGE_MIN_VERSION` globals
(`Core.cpp:14-15`), `FLOAT GSecondsPerCycle` (H:143), `FTime GTempTime`, `GStat`, `GFileStream`, `GCurrentViewport`,
`GEncryptLoad`, `GCurrentEncrypt`, `GDebugger`, `GAudioMaxRadiusMultiplier`, `GAudioDefaultRadius` (H:169-174).
cc probe: 0 errors (gcc, wasm).

### 5.2 `Src/UnAnsi.cpp` — CONFLICTS=1 (epic→tmpl 165, epic→HP2 29)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:190,200 `#ifndef PLATFORM_DREAMCAST` around `appMemmove`/`appMemset` definitions | 17b0864 — DC maps them to libc macros in `UnFile.h` | H:215-223 Epic's | N/A | merge brings in dead guards; harmless |
| 2 | T:215-341 `appFindFiles( const TCHAR* Spec )` (opendir/readdir wildcard lister) | ee0ff8e — only user is `DCUtil/Src/Main.cpp:134,699` | none | N/A | optional; harmless if the clean merge brings it (it pairs with the Core.h declaration, §4.1 #6). Recommended: **drop both** (no HP2 caller) |
| 3 | **T:392 `appSprintf` non-MSVC branch gains `return Result;`** | cbdb054 — Epic's GCC branch `int Result; GET_VARARGS_RESULT(Dest,1024,Fmt,Result); #endif }` falls off the end of a non-void function (UB; modern GCC/clang at -O2 may emit `ud2`/`unreachable` and every caller of `appSprintf` crashes or gets garbage) | **H:282-290 — the same bug** (quoted: `int Result; GET_VARARGS_RESULT(Dest,1024,Fmt,Result);` then `#endif }`) | AC | take — this is a **hard runtime bug on every GCC target**, not cosmetic |
| 4 | T:423-431 `appStrstr` → `const_cast<TCHAR*>(strstr(…))` (UNICODE: C cast) | cbdb054 — C++ `<cstring>` overloads `const char* strstr(const char*, const char*)`; returning it as `TCHAR*` is ill-formed | H:316-323 Epic's | AC | take |
| 5 | T:428-435 `appStrstrFs` (case-insensitive `strcasestr` under `PLATFORM_CASE_SENSITIVE_FS`) | 17b0864 (DC FATFS) — used only by `appFindFiles` | none | N/A | skip with #2 (and drop the `UnFile.h:279` declaration if it came in) |
| 6 | T:437-442 `appStrchr( const TCHAR*, INT c )` + `const_cast` | cbdb054 — same `const` issue; `INT` matches the declaration | H:325-334: signature `int c`, body has a `#if __PSX2_EE__ FString s = FString::Printf(TEXT("%c"),c); return appStrstr( String, *s );` branch — **the one conflict** | ADAPT | resolve to the template's body; delete the PSX2 branch (it would also return a pointer into a destroyed temporary). HP2 `Inc/UnFile.h:306` already declares `INT c` |
| 7 | T:536-551 `appQsort(…, QSORT_COMPARE Compare)` + a DC `QsortWrapper` | cbdb054 (typedef) / 17b0864 (wrapper: DC `INT`≠`int`) | H:427-430 Epic's spelled-out pointer type; HP2 `Inc/UnFile.h:336-337` already uses `QSORT_COMPARE` | AC | take the typedef spelling (same type on our targets); the DC wrapper is dead code |

HP2-only content to keep: `appAcos` (H:116, used by `UnMath.h` `RandomSpreadVector`), `appFrand(From,To)` (H:156),
`appRandRange` ×2 (H:162-166, back the 433 native `execRandRange`). Delete the empty `#if __PSX2_EE__ #else … #endif`
wrapper in `appGetGMTRef` (H:75-89; the `#else` is the whole function body, so it compiles either way — hygiene).
cc probe after merge + #6 resolution: 0 errors (gcc, wasm).

### 5.3 `Src/UnGUID.cpp` — dry run CLEAN (epic→tmpl 15)

Compiled on Linux and Emscripten (not MinGW: CMake swaps it for `UnMinGW.cpp`, §3.4).

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:98-102 `ip32 = 0x0100007f` under `PLATFORM_DREAMCAST` | 17b0864 | H:99 `ip32 = appGetLocalIP();` | N/A | dead code |
| 2 | **T:105-112 `*(unsigned32*)&nodeid[0] = ip32; *(unsigned16*)&nodeid[4] = random16;`** (DC: `memcpy`) | cbdb054 — Epic writes `*(unsigned32*)nodeid[0] = ip32;`: it casts the **byte value** of `nodeid[0]` to a pointer and stores through it — a wild write to address 0..255 the first time `appCreateGuid()` runs (retail Windows never compiled this file; `UnVcWin32.cpp` used `CoCreateGuid`) | **H:102-103 — same bug** | AC | take. On wasm a write to address <256 silently corrupts the bottom of linear memory instead of faulting — exactly the kind of bug that surfaces far away. `appCreateGuid` runs on every package save (`ULinkerSave` → new `Guid`) and HP2 **saves games** |

HP2-only: empty `#if __PSX2_EE__ #else … #endif` in `get_system_time` (H:174-189) — delete (hygiene). cc probe: 0 errors.

### 5.4 `Src/UnProp.cpp` — dry run CLEAN (epic→tmpl 11)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:142, T:183 `#ifndef PLATFORM_LOW_MEMORY` around `UProperty::ExportCpp` | 17b0864 (DC) | H:140-150 | N/A | dead guard; harmless |
| 2 | T:482-489 `UBoolProperty::Serialize`, transactional branch: `DWORD Mask = BitMask; Ar << Mask; if( Ar.IsLoading() ) BitMask = Mask;` | cbdb054 — `BITFIELD` vs `DWORD` `operator<<` overload ambiguity on compilers where `BITFIELD` is a distinct type (Epic: `typedef unsigned long BITFIELD` on MSVC, `unsigned int` elsewhere) | H:479-480 `if( !Ar.IsLoading() && !Ar.IsSaving() ) Ar << BitMask;` Epic's | AC | take (the merge does). Only undo/redo transactions (editor) hit this branch |

HP2-only: `#ifdef __PSX2_EE__` 16-byte `Offset` alignment for `Vector/Plane/Coords` struct properties in
`UStructProperty::Link` (H:1416-1423) → **delete** (on PC the `#else` path `Align(…, ElementSize>=4 ? 4 : …)` is
the retail layout; keeping it under a never-defined macro is harmless but it is exactly the code a future reader
would mistake for relevant). Keep HP2's `%.7g` float export, quoted-name export, `NetSerializeItem` `ElementSize`
fix, `ImportText` `Buffer++` fix. Build note: the template compiles this file with `-Og` in non-Debug GCC builds
(§3.4) — keep. cc probe: 0 errors.

### 5.5 `Src/UnName.cpp` — CONFLICTS=1 (epic→tmpl 10, epic→HP2 49)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:42-44 the `Hardcoded name %i was duplicated` `appErrorf` commented out ("maximqad: fix DCUtil") | ee0ff8e — DCUtil registers names twice | H:42-43 Epic's check | N/A | **keep HP2's check** (a duplicate hardcoded name in HP2 is a real bug worth an error) |
| 2 | T:110-111 namespace-scope `CORE_API FNameEntry* AllocateNameEntry(…);` forward declaration | cbdb054 — the only declaration is the `friend` inside `FNameEntry` (`Inc/UnName.h:46`), which ordinary lookup does not see; the template needs it because `FName::StaticInit` calls `AllocateNameEntry(TEXT(#namestr),num,RF_Native,NULL)` (T:129-131) where no argument has class type, so ADL cannot find the friend | HP2's only use (H:108, `FName::FName`) passes `NameHash[iHash]` (`FNameEntry*`) → found by ADL; HP2's `StaticInit` goes through `HardcodedNames[]`, not `AllocateNameEntry(…,NULL)`. Definition H:263 | AC (optional) | take anyway for robustness, **placed before the first use (before H:~25)**, not at the template's position. This is also the dry run's single conflict (HP2 rewrote the surrounding hardcoded-name block) — resolve to HP2's block + the declaration. The previous run's claim that the template "only works by accident" is wrong: it is ADL in HP2's case and the forward declaration in the template's |
| 3 | T:189 `FName::DeleteEntry( int i )` (`INT` → `int`) | cbdb054 cosmetic (matches a header spelling) | H:205 `INT i` | N/A | skip; HP2's `Inc/UnName.h` declares `INT` and `INT`==`int` on all our targets |

HP2-only (keep): entry-pointer `FName` (`FNameEntry* Entry`, `HardcodedNames[]`, `FNAME_Define`, `NAME_SIZE 128`).
cc probe: 0 errors.

### 5.6 `Src/UnMath.cpp` — CONFLICTS=2 (epic→tmpl 15, epic→HP2 193)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:23, T:36 `#ifndef PLATFORM_LOW_MEMORY` around the table init | 17b0864 (DC RAM) | HP2 `FGlobalMath::FGlobalMath()` (H:44-69) builds different 433 tables (`TrigFLOAT` + `SqrtManTbl` + `SetupTable(DivSqrtManTbl…)`, `SetupTable(DivManTbl…)`) | N/A | nothing — both conflicts resolve to HP2's text |
| 2 | T:25-35 `int i;` hoisted; `SqrtFLOAT` loop body rewritten (two unused locals `S`, `Temp`) | cbdb054 (scoping) | HP2 has no `SqrtFLOAT` loop | N/A | nothing |

**HP2-only GCC/clang blocker in this file** (cc probe: gcc 1 error, wasm 7 errors, all the same):
`SetupTable` (H:19-41) declares `for( DWORD i=0; … )` at H:23 and reuses `i` in the second loop `for( i=0; … )`
at H:33 — pre-standard MSVC6 for-scope. **ACTION:** hoist `DWORD i;` above the first loop (exactly the
template's cure for Epic's constructor). The `union {FLOAT F; DWORD D;} Temp;` punning is fine (union punning is
supported by GCC/clang; `-fno-strict-aliasing` is global anyway).

### 5.7 `Src/UnClass.cpp` — CONFLICTS=3 (epic→tmpl 73, epic→HP2 394)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | **T:295-302 layout guard, part 1**: `UStruct::UStruct( ENativeConstructor, INT InSize, … )` body records `(this, InSize)` into `GLayoutStructs[4096]` / `GLayoutSizes[]` / `GLayoutCount` (plain BSS globals defined right after the ctor) | bb90b47 — native classes are constructed at static-init time; recording the C++ `sizeof` there is the only moment it is known | H:288-303 Epic's ctor (`{}` body) + HP2's `EStaticConstructor` twin at H:304-318 | AC (textual conflict because of the twin) | put the recording code in the **`ENativeConstructor`** ctor only. The `EStaticConstructor` twin is used only by the `__STATIC_LINK` `IMPLEMENT_CLASS` branch (`Inc/UnObjBas.h:394`) which §4.9 deletes — leave it untouched (or delete it with that branch) |
| 2 | **T:366-385 layout guard, part 2**: after `PropertiesSize` is final in `UStruct::Link` (loaded/compiled branch), look `this` up in `GLayoutStructs` and `debugf( NAME_Warning, "[layout] native %s: C++ sizeof=%i but script layout=%i -- member-offset shift risk" )` | bb90b47 ("NECESSARY BUT NOT SUFFICIENT": a derived member placed in a base's tail padding passes it — only a per-property offset check sees that) | H:394 `PropertiesSize = Align(PropertiesSize,PropertiesAlign);` — HP2 aligns to `UStruct::PropertiesAlign` (433 member, `Inc/UnClass.h:204`; set to 4 at H:364 or inherited at H:369; 16 only in the `__PSX2_EE__` blocks) | ADAPT | insert the check **immediately after H:394** (the template inserts it after its own `Align(PropertiesSize,4)`). Keep `PropertiesAlign`; delete the three `#if __PSX2_EE__` blocks (H:383-391, H:408-416, H:420-441 — the last one *adds* 4/12/48 bytes to `Vector`/`Plane`/`Coords` sizes, which would be catastrophic if ever enabled on PC). **This hunk is the acceptance instrument for Phase B** ("zero `[layout]` warnings" with `-fpack-struct=4` on web/MinGW) — HP2's C++ class headers were hand-maintained against the shipped `.u` files, so it is the cheapest way to find a drifted member |
| 3 | **T:389-411 native `UBoolProperty` BitMask restore** in the `else` (native, `Props=0`) branch: `UBoolProperty* Bool = Cast<UBoolProperty>(Property); BITFIELD SavedBitMask = Bool ? Bool->BitMask : 0;` … after `Property->Link(Ar,Prev); Property->Offset = SavedOffset;` → `if( Bool ) Bool->BitMask = SavedBitMask;` | c506ba7 (ufront 2.70) — `UBoolProperty::Link` merges adjacent native `CPP_PROPERTY` bools into one word as if they were script bitfields, so the second native bool got BitMask 2, the third 4…; a ctor that writes `=true` (bit 1) can then never be turned off by config (`UseReverb=False` measured on NOpenALDrv) | H:396-416 — Epic's text (`INT SavedOffset = Property->Offset; Property->Link( Ar, Prev ); Property->Offset = SavedOffset;` at H:403-406) plus a PSX2 block | AC | take. Relevant to every HP2 driver that registers several consecutive `UBOOL` config properties via `CPP_PROPERTY` (NSDLDrv/NOpenALDrv/XOpenGLDrv/UTGLR `StaticConstructor`s, and HP2's own ALAudio/Engine client classes) |
| 4 | T:1112-1142 `XferAligned` + DC `XFER(T)` | 17b0864 (DC) | H:1396 `#define XFER(T) {Ar << *(T*)&Script(iCode); iCode += sizeof(T); }` Epic's | N/A | dead code; resolve the conflict to HP2's text |
| 5 | T:1335-1347 `EX_Case` reads the `_WORD` via `__builtin_memcpy` under DC | 17b0864 | H:1644-1652 — **HP2 already reads it with `appMemcpy` unconditionally** (`XFER(_WORD); appMemcpy(&W, &Script(iCode-sizeof(_WORD)), sizeof(_WORD));`) | PRESENT | nothing |

**HP2-only GCC/clang blocker in this file** (cc probe: gcc 2 errors, wasm 3 errors):
- H:1439 `if((wcscmp(nameComp,badTeleport1)!=0)&&(wcscmp(nameComp,badTeleport2)!=0))` inside `UStruct::SerializeExpr`
  ("kludgecity gk" Teleporter debugger workaround). HP2 retail was built `UNICODE` (`TCHAR`=`wchar_t`; cf.
  `SHGetFolderPathW` in `UnVcWin32.cpp:1057`), so `wcscmp` compiled there; with the template's ANSI `TCHAR`=`char`
  it is a type error. **ACTION:** `appStrcmp`. (The macro `HANDLE_OPTIONAL_DEBUG_INFO()`, H:1400-1415, declares
  `int RemPos, OldiCode, NextCode, GVERSION` without braces — every use is in its own block, so it compiles; do not
  "fix" it into a `do{}while(0)` without checking each use site.)
- The cc probe's third wasm error (`#endif without #if` at the merged line 1687) was an artefact of that run's
  conflict resolution, not HP2 source — resolve conflict 3 carefully (the conflict spans a `#if __PSX2_EE__` region).

HP2-only content to keep: `UStruct::PropertiesAlign`, `SerializeTaggedPersistentProperties`, `UClass::ClassCRC`,
the `EX_DebugInfo` handling in `SerializeExpr`, `UClass::Bind`'s `autoclass%s` lookup (H:1059-1063) and
`UFunction::Bind`'s `int%sexec%s` (H:1794-1796) — both resolve through the static registry once §5.12 lands.

### 5.8 `Src/UnCorSc.cpp` — CONFLICTS=4 (epic→tmpl 15, epic→HP2 208)

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:294 `execArrayElement`: `Index = Clamp<INT>( Index, 0, (INT)GProperty->ArrayDim - 1 );` | cbdb054 — explicit template argument for a mixed `INT`/`int` call (DC's `int32_t`≠`int`); harmless elsewhere | H:316 Epic's `Clamp( Index, 0, GProperty->ArrayDim - 1 )` (`ArrayDim` is `INT`, `Inc/UnType.h:26`) | AC | take (defensive, zero cost) |
| 2 | T:331 `execDynArrayElement`: same `Clamp<INT>` | cbdb054 | HP2 **rewrote** `execDynArrayElement` (H:364-403: negative index → warn + 0; index ≥ Num → `Array->AddZeroed( ArrayProp->ElementSize, Index+1-Num )`, i.e. 433 dynamic arrays **grow on write**) and added `execDynArrayCount` (H:406-418) | N/A | keep HP2's (the shipped bytecode relies on grow-on-access). Conflict → HP2's text |
| 3 | T:348-353 `execBoolVariable` reads the `UBoolProperty*` operand via `__builtin_memcpy` under DC | 17b0864 | H:427-432 `#if __PSX2_EE__ appMemcpy(&Property, Stack.Code, sizeof(INT)) #else UBoolProperty* Property = *(UBoolProperty**)Stack.Code; #endif` | PRESENT (as PSX2) | keep the plain read; delete the PSX2 branch (it copies `sizeof(INT)` bytes into a pointer — only correct on 32-bit). x86 and wasm tolerate the unaligned load |
| 4 | T:2223-2229 `execVectorConst` via `__builtin_memcpy` under DC | 17b0864 | H:2312-2318 reads `X/Y/Z` with three `Stack.ReadFloat()` calls | PRESENT (different form) | nothing |

HP2-only content to keep (433 script VM): `execDebugInfo`, `FFrame::Describe`, parent-frame chaining, the
`EX_DebugInfo` peeks in `ProcessInternal`/`CallFunction`, `execRandRange`, `execStringToName`, `execGetLanguage`,
`execDynArrayCount`, Legend native ids 1030/1031/1033. Build: keep the template's `-Og` for this file (§3.4; the
ut99dc FIXME about `FFrame::Step` reference juggling applies to HP2's larger VM just the same).
cc probe: 0 errors (gcc, wasm).

### 5.9 `Src/UnLinker.h` (template) → HP2 `Inc/UnLinker.h` + `Src/UnLinker.cpp` — manual port (no dry-run pairing)

433 split the linker out of `UnObj.cpp` into `Inc/UnLinker.h` (declarations) + `Src/UnLinker.cpp` (bodies), so
`merge3.sh` cannot pair the template's `Src/UnLinker.h` with anything. All four hunks land in `Src/UnLinker.cpp`.

| # | Template hunk (T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:335-337 `ULinkerLoad::ULinkerLoad`: `appThrowf( TEXT("%s: %s"), LocalizeError("OpenFailed"), InFilename );` | 1546db7 (ufront 2.35) — "Error opening file" with no filename is unactionable, and with exceptions off it is the last log line | H `Src/UnLinker.cpp:168` `appThrowf( LocalizeError("OpenFailed") );` | ADAPT (new location) | apply by hand |
| 2 | T:552-564 `VerifyImport`, nested import (`PackageIndex<0`) branch: `#ifdef __EMSCRIPTEN__ if( !Import.SourceLinker ) { if( Cast<UPackage>(LinkerRoot) ) Cast<UPackage>(LinkerRoot)->PackageFlags |= PKG_BrokenLinks; return; } #else check(Import.SourceLinker); #endif` | ufront 2.12 (in d5a6bfd) — on the web `GetPackageLinker` returns NULL instead of throwing (§5.11 #5), so the `check` would abort the module | H `Src/UnLinker.cpp:399-400` (`Import.SourceLinker = ImportMap(-Import.PackageIndex-1).SourceLinker; check(Import.SourceLinker);`) | ADAPT | apply by hand (HP2's `VerifyImport` body is one indentation level shallower than Epic's, same statements) |
| 3 | T:573-585 after the if/else, before "Find this import within its existing linker": the same NULL bail-out | ufront 2.12 — otherwise `Import.SourceLinker->ExportHash[iHash]` dereferences NULL | between H:406 (`Pkg = CreatePackage( NULL, *Top->ObjectName );` `}`) and H:408-409 (`// Find this import…` / `UBOOL SafeReplace = 0;`) | ADAPT | apply by hand. Note the `PackageIndex==0` branch (H:376-392) still `try{ GetPackageLinker( …, LOAD_Throw …) } catch(…)` — on the web nothing is thrown (SafeLoadError no longer re-throws), the call returns NULL and this bail-out is what catches it |
| 4 | T:1062-1063 `ULinkerSave::ULinkerSave`: OpenFailed names the file | 1546db7 | H `Src/UnLinker.cpp:892` | ADAPT | apply by hand — HP2 writes `.usa` save games through `ULinkerSave`; a failed save must name the path |

Not changed by the template, keep as is: `CreateImport`'s `check(Import.SourceLinker)` (H:709; `VerifyImport`
bails before `SourceIndex` is set, so it is not reached for a broken import). HP2-only: 433 `FPackageFileSummary`
with licensee version, runtime `PACKAGE_FILE_VERSION`/`PACKAGE_MIN_VERSION` checks, `GEncryptLoad` — keep.
cc probe: 0 errors (gcc, wasm).

### 5.10 `Src/UnMisc.cpp` — CONFLICTS=2 (epic→tmpl 288, epic→HP2 573)

| # | Template hunk (E→T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | E:38-50→T:37-50 `FOutputDevice::Logf` ×2: `GET_VARARGS` moved before the `SafeSuppressed` test | cbdb054, cosmetic (formats even when suppressed) | H:36-50 | AC | optional; no behavioural value. Take whatever the merge produces |
| 2 | E:62→T:62-63 `FArray::Realloc`: `INT TotalSize = ArrayMax*ElementSize;` | cbdb054, cosmetic | HP2's `FArray` has **no** out-of-line members — `Realloc` lives in `TArray<T>` (`Inc/UnTemplate.h`, 433 restructure §4.17) | N/A | one of the 2 conflicts: resolve to HP2's absence |
| 3 | E:130→T:131 `FString::RightPad`: `return *this + (const TCHAR*)Ch;` | cbdb054 — `FString::operator+( const TCHAR* )` is a **non-const** member and `FString` has `operator UBOOL()`, so `*this + Ch` with a `TCHAR*` also matches the built-in `ptrdiff_t + char*` (via `UBOOL`); clang reports it ambiguous | H:94 `return *this + Ch;` | AC | take. **Same pattern at an HP2-only site:** H:1839 `GSys->SaveSlotPath = GSys->SavePath * TEXT("Slot") + itoa;` (`TCHAR itoa[32]`) — cc probe wasm: *"use of overloaded operator '+' is ambiguous (with operand types 'FString' and 'TCHAR[32]')"*; gcc accepts it. ACTION: `+ (const TCHAR*)itoa` (and grep Engine/Editor for `FString + <TCHAR array>` when they first compile under clang) |
| 4 | **E:686→T:688-696 `appThrowf` echoes `[appThrowf] <msg>` to stdout under `__EMSCRIPTEN__` before `throw`** | d5a6bfd (2.7b/2.7c) — with catching disabled an escaping throw is an anonymous `Aborted()`; the echo names each uncatchable site to patch one by one | H:674-678 Epic's | AC | take. First thing to read when an HP2 web boot aborts |
| 5 | **E:1066→T:1077-1152 `appIsGlobalOptionToken()` + `appApplyIniOverrides()` bodies** | 1d3dfa5 (a first-arg `Key=Value` token is not a map URL) / 2c25d55 (2.38: `-ini:<File>:[<Section>]:<Key>=<Value>` was emitted by the frontend and implemented nowhere) | insert before `ParseToken` (H:1058) | AC | take (declarations come with the §4.8 UnFile.h hunk; callers are the launcher — template-launcher report) |
| 6 | **E:1445-1589→T:1532-1676 `appFindFileNoCase()` + `appFindPackageFile()` restructured: each `Paths=` entry is tried exactly, then case-insensitively, before the next; the GUID cache dir is not case-searched** | 7164de7 — the old second full pass let a later path's exact hit beat an earlier path's case-variant (`Music/Credits.umx` over `Textures/credits.utx`) → "Can't find Package" → abort with no message on the web | H:1438-1583 Epic's two-pass algorithm verbatim + `FPushMemTag Push(TEXT("appFindPackageFile"))` at H:1444 | AC | take (this region merges clean); keep the `FPushMemTag`. HP2 relevance: `Default.ini:45-50` lists `../System/*.u`, `../Maps/*.unr`, `../Textures/*.utx`, `../Sounds/*.uax`, `../Music/*.umx`, **`../save/*.usa`** (lower-case `save` while `SavePath=../Save`, `Default.ini:42`) — case-insensitive resolution per path is exactly what this content needs |
| 7 | E:1723,1742→T:1805,1824 `LocalizeError("MissingIni Default.ini")` message tweaks | cosmetic (and fixes Epic's `MisingIni` key typo only in spirit — the key string changes) | H:1926, H:1946 — inside HP2's rewritten ini-regeneration block (`GetGlobalTime` "//ft", `bDefaultIniChanged`) | N/A | skip; the conflict is HP2's larger rewrite — keep HP2's block |
| 8 | **E:1748→T:1831 `appApplyIniOverrides();` immediately after `GConfig->Init(…)`** | 2c25d55 — must run before any class loads config | H:1952 `GConfig->Init( GIni, GUserIni, RequireConfig );` **followed by HP2's "gk 10/15/01" block (H:1955-1963) that overwrites `[Engine.Engine] Language` from a second config cache** | ADAPT | insert **after** the gk block (i.e. just before `// Language.` at ~H:1966), not after H:1952 — otherwise a `-ini:System:[Engine.Engine]:Language=…` override is silently clobbered by the gk block |
| 9 | E:1759→T:1842 `GMem.Init( 32768 )` | 17b0864 (DC RAM) | H:1977 `GMem.Init( 65536, TEXT("GMem") )` (433 tagged `FMemStack::Init`) | N/A | keep HP2's |
| 10 | E:1887→T:1970-1971 `appFromUnicode`: `INT Count;` hoisted out of the `for` (`Ch[Count] = 0;` after the loop) | cbdb054 — MSVC6 for-scope | H:2274-2276 same code | AC | take |
| 11 | **E:1904-1912→T:1988-2026 `appUnwindf` no longer sets `GIsCriticalError = 1`; `GErrorHist` accumulates only when already critical** | 1d3dfa5 (2.38) — every recovered load failure latched permanent-critical mode: `~UObject` then skips table cleanup (`UnObj.cpp` H:388 `if( Index!=INDEX_NONE && GObjInitialized && !GIsCriticalError )`) → dangling object-table entries, and `FOutputDeviceFile::Serialize` (`if( !GIsCriticalError || Entry )`, H `Inc/FOutputDeviceFile.h:33`) **gags the log** | H:2289-2304 Epic's text | AC | take. Precondition verified: HP2's `FOutputDeviceAnsiError::Serialize` sets `GIsCriticalError = 1` itself before throwing (H `Inc/FOutputDeviceAnsiError.h:39-42,77`), so real fatals are still flagged |

**HP2-only blockers / hazards in this file (not template hunks, but they sit between them — fix in the same pass):**

| HP2 line | Problem | Evidence | ACTION |
|---|---|---|---|
| H:2047-2054 | `for( const TCHAR* p = …; …; p-- ) {…}` then `p` used after the loop (`GSys->Paths(i).Left( p - *GSys->Paths(i))`) — MSVC6 for-scope | cc probe: gcc & wasm `'p' was not declared in this scope` | declare `const TCHAR* p;` before the loop |
| H:2083/2097, H:2119/2134 | `for( INT j=0; …)` then `for( j=0; …)` in `SavePersistentActorCache` / `LoadPersistentActorCache` | cc probe: 10 errors | hoist `INT j;` in both functions |
| H:2030 | `debugf( TEXT("Save Slot Path: %s"), GSys->SaveSlotPath );` passes an **`FString` object** through `...` | cc probe wasm: *"cannot pass object of non-trivial type 'FString' through variadic method; **call will abort at runtime**"* — clang emits a trap; MSVC/GCC-x86 happen to push the object whose first word is `Data`, so `%s` "works" there | `*GSys->SaveSlotPath`. **This line runs on every boot** — unfixed, the web build aborts in `appInit`. ⚠ The template's root CMake **silences** this diagnostic for the web build (`/home/ion/engines/engine-ut99/Source/CMakeLists.txt:147-149`: `-Wno-error=non-pod-varargs -Wno-non-pod-varargs`, "mostly script-export/debug paths") — HP2 inherits that line, so these traps are invisible at compile time. ACTION: keep the template's flags for the normal build, but do one web build of every package with `-Wnon-pod-varargs` (warning on) and fix every site on a boot/gameplay path; record the list in the Core/Engine porting notes |
| H:1839 | `FString + TCHAR[32]` ambiguous under clang | see hunk 3 | `+ (const TCHAR*)itoa` |
| H:1629-1633 | `FFileStream::Destroy()` spins `while (Destroyed == 1) appSleep(1);` under `#if !__UNIX__`. With the template's `UnGnuG.h` `__UNIX__` is no longer defined → the wait becomes live, and only the Win32 streaming thread (`UnVcWin32.cpp:667-701`, sets `Destroyed = 2`) ever ends it → **infinite loop at exit** on every GCC target | code read | re-key to `#if _MSC_VER` (the only build that starts `FileStreamingThread`, `UnVcWin32.cpp:1489`) |
| H:1644-1810 + `UnVcWin32.cpp:706-940` | `FFileStream::DestroyStream` (portable, H:1754-1770) calls `Read(INT,INT)` and `Destroy(INT)`, which — with both `Create(…)` overloads — are defined **only in `UnVcWin32.cpp`** (vorbisfile/`coda` readers). Any GCC link that pulls `UnMisc.o` (all of them; `--whole-archive`) fails with undefined `FFileStream::Read/Create/Destroy(int)` | code read (`grep -n "FFileStream::" Src/*.cpp`) | §8.1: add Unix definitions (stubs returning 0 in `UnUnix.cpp`, or move the portable bodies into a new `Src/UnFileStream.cpp`). Only `ALAudio` uses `GFileStream` (`ALAudio/Src/ALAudioSubsystem.cpp:364-670`), and ALAudio is replaced by NOpenALDrv — stubs are sufficient for Core |
| H:1955-1963 | "gk 10/15/01": `DConfig->Init(TEXT("default.ini"), …)` — **lower-case**; on a case-sensitive FS `FConfigCacheIni::Find` (H `Inc/FConfigCacheIni.h:127`) only opens it if `FileSize>=0`, so `GetString` returns 0 having set `*Value=0` (H `Inc/FConfigCacheIni.h:162`) and `SetString(…Language, "")` **empties the language** → every `Localize()` misses; `appFindPackageFile` treats the game as international (`"" != "int"`). Also leaks `DConfig` | code read | `TEXT("Default.ini")` (or delete the block; its purpose is to force `Language` from `Default.ini` on every boot) |
| H:2002-2011 | `if( *appUserDir() ) { SavePath = appUserDir()*"Save"; GSys->Paths[5] = …*.usa; } else SavePath = TEXT("..\\Save");` — the `else` literal has a **backslash**; `FFileManagerLinux` converts `\` only in `FindFiles` (T `Inc/FFileManagerLinux.h:320-323`), not in `CreateFileWriter`/`MakeDirectory`, so on Linux/web this creates a directory literally named `..\Save` in `System/`. The `if` branch indexes `Paths[5]` unconditionally (six `Paths=` lines required) | code read | use `TEXT("..") PATH_SEPARATOR TEXT("Save")`; decide `appUserDir()` for Unix (§8.2) — with `""` the else-branch is taken |
| H:1912-1950 | ini (re)generation when `GetGlobalTime("Default.ini") > GetGlobalTime(Game.ini)` — see §4.3: with a real `stat` mtime and a freshly preloaded `Default.ini` on the web, every page load would overwrite the persisted `Game.ini`/`User.ini` | code read | `GetGlobalTime` returns 0 on `__EMSCRIPTEN__` (§4.3), or make the web boot set identical mtimes |
| H:2040-2056 | `MapPath` = `appBaseDir()` cut at the first `System` + `"Maps"`; `InStr` returns -1 when the base dir has no `System` component (native binary run from a build dir) → `Left(-1)` = "" → `MapPath="Maps"` (relative) | code read | launcher convention: always run with cwd/exe in `…/System/` (the template's native run-tree and the web's `/System` both satisfy it) |

cc probe after the merge: gcc 3 errors, wasm 12 errors + 1 runtime-trap warning — all in the table above.

### 5.11 `Src/UnObj.cpp` — CONFLICTS=4 (epic→tmpl 195, epic→HP2 374)

| # | Template hunk (E→T lines) | Commit / why | HP2 site | Class | ACTION |
|---|---|---|---|---|---|
| 1 | E:598→T:599-603 `InitExecution`: `printf("[ROLE DBG] InitExecution class null …")` | d5a6bfd leftover debug print | H:626-637 (`check(GetClass()!=NULL)` right after `guard`, so the print could never fire in HP2 anyway) | N/A | skip |
| 2 | E:723→T:729-732 `FindFunctionChecked`: extra `debugf("DEBUG: FindFunctionChecked …")` before `appErrorf` | debug leftover | H:760-770 | N/A | skip (harmless if the merge brings it) |
| 3 | E:942→T:951-955, E:3448→T:3594-3598 `TCHAR Buffer[8192]` under DC in `LoadConfig`/`CacheDrivers` | 17b0864 (DC stack) | H:~928/986 (`TCHAR Buffer[32767]` in the config-reset path), H:~3717 | N/A | skip. Note for the web: 32 KB stack arrays are fine with the template's `-sSTACK_SIZE=2097152` (`UnrealTournament/CMakeLists.txt:116`) — HP2's Game target must keep that value |
| 4 | **E:1978→T:1991-2001 `SafeLoadError`: under `__EMSCRIPTEN__` do not `appThrowf` on `LOAD_Throw`** (`(void)Error;`) | d5a6bfd ("2.6f") — catching is disabled on the web (`-fignore-exceptions`; `-fexceptions` overflows V8 through the deep loader, `-fwasm-exceptions` is incompatible with ASYNCIFY); a re-thrown load error escapes every catch and aborts the module. Callers already turn NULL into a graceful failure | H:2137-2160 — HP2 adds a Legend `if( GIsStrict ) LoadFlags = LOAD_NoFail;` block before `guard` (keep); the throw is H:2155 | AC | take (the `#ifndef __EMSCRIPTEN__` wraps H:2155 only). Consequence for HP2 on the web: `-STRICT` turns every load warning into `appErrorf` → abort; do not pass it in the web launcher |
| 5 | **E:2020-2034→T:2043-2068 `GetPackageLinker`: `PackageResolveFailed`, `PackageNotFound`, `FileNotFound` → `return NULL;` under `__EMSCRIPTEN__`** | d5a6bfd (2.6f) | H:2197, H:2203, H:2210 — Epic's `appThrowf`s | AC | take all three |
| 6 | **E:2081→T:2116-2129 `PackageVersion` (GUID mismatch) → `SafeLoadError(…)` + `return NULL` under `__EMSCRIPTEN__`** | a57d578 (2.79o) — one `LocalizeError` per statement (it answers in a shared static buffer) | H:2253-2254 `if( CompatibleGuid && Result->Summary.Guid!=*CompatibleGuid ) appThrowf( LocalizeError("PackageVersion"), … );` | AC | take (low value for HP2 — no multiplayer content download — but it keeps the file aligned with the template). HP2 moved the `Sandbox` check after the `try` (H:2261-2266) — keep that |
| 7 | **E:2197→T:2245-2257 `StaticLoadObject`: `ObjectNotFound` → `EndLoad(); return NULL;` under `__EMSCRIPTEN__`** | d5a6bfd (2.6g) — skips the package-remap retry on the web | H:2377 | AC | take |
| 8 | E:2261→T:2321-2323 `#ifndef PLATFORM_DREAMCAST` around `Linker->LoadAllObjects()` in `LoadPackage` | 17b0864 (DC lazy load) | H:2440 | N/A | dead guard; harmless |
| 9 | E:2979→T:3040-3041 `check(Removed!=0); check(Removed==1);` commented out | ut99dc hack | H:3215-3216 | N/A | **keep HP2's checks** |
| 10 | **E:3172-3180→T:3233-3285 `StaticConstructObject`: allocate → run the C++ constructor for every class (retail path) + a self-detecting safety net: if a native `CLASS_Config` object's ctor wiped `Class` (value-init regression), restore the header and `LoadConfig()`** (native: whole header; web: `Class` + `LoadConfig`) | a860dca (removes 17b0864's "STUPID HACK" that saved/restored every actor's property bytes around its ctor — it undid `AInternetLink`'s `Socket=INVALID_SOCKET`) + d5a6bfd 2.12 Part B | H:3400-3420 Epic's two lines + `FPushMemTag Push(InClass->GetName())` | AC (optional) | HP2 never had the hack. With a680288 in `DECLARE_CLASS` (§4.9 #2) the net is a no-op; take it for parity (keep the `FPushMemTag`) or leave Epic's two lines — both are correct |
| 11 | **E:3210-3390→T:3316-3409, +3510, +3535 `FArchiveTagUsed` iterative: `operator<<` only claims the object and pushes it on a `TArray<UObject*> Worklist`; `DrainWorklist()` pops and serialises; `CollectGarbage` and `IsReferenced` call `TagUsedAr.DrainWorklist()` right after `SerializeRootSet`** | ec74328 (2.19d) — DM-Codex at -O0 died with `RangeError: Maximum call stack size exceeded` at 5,547 wasm frames (a 202-link `UField::Next` chain = 1,010 frames); "the engine worked by luck of the inliner". "A missed drain would leave everything past the root set unmarked and PurgeGarbage would delete the live world" | H:3448-3547 — HP2's class has extras: a `RefObj` ctor argument + `GetReferencing()` (used by `IsReferenced(…, IgnoreReference=0)` to return the **referencer**, H:3654-3663), a `appMemcpy(&Obj,&Object,sizeof(INT))` in/out copy of the pointer ("misaligned pointer", PS2), a `__PSX2_EE__` `FName` copy | ADAPT | rewrite HP2's class on the template's shape, preserving `RefObj` semantics — see the sketch below. Add both drains: after H:3632 (`CollectGarbage`) and after H:3656 (`IsReferenced`). cc probe: without the class rewrite the merged file fails with `'class FArchiveTagUsed' has no member named 'DrainWorklist'` ×2 (gcc & wasm) — i.e. the clean-looking merge of the two call sites is **not** safe on its own |

Resolved `FArchiveTagUsed::operator<<` for HP2 (keeps `RefObj` exact: the referencer is recorded at *claim* time,
when `Context` is still the object whose `Serialize` is running — LIFO draining cannot change that):
```cpp
FArchive& operator<<( UObject*& Obj )          // HP2: drop the appMemcpy in/out copy (PS2 alignment only)
{
	guardSlow(FArchiveTagUsed<<Obj);
	GGarbageRefCount++;
	if( Obj && (Obj->GetFlags() & RF_EliminateObject) )
		Obj = NULL;                                // dereference it
	else if( Obj && (Obj->GetFlags() & RF_Unreachable) )
	{
		Obj->ClearFlags( RF_Unreachable | RF_DebugSerialize );
		if( Obj->GetFlags() & RF_TagGarbage )
			Worklist.AddItem( Obj );               // ec74328: defer instead of recursing
		else
		{
			debugfSlow( NAME_Log, TEXT("%s is referenced by %s"), Obj->GetFullName(), Context ? Context->GetFullName() : NULL );
			if( RefObj == Obj )                    // HP2 433: IsReferenced() reports the referencer
				RefObj = Context;
		}
	}
	return *this;
	unguardSlow;
}
// DrainWorklist(): the template's body (T:3359-3383) minus its else-branch (handled above).
// operator<<( FName& ): HP2's #else branch only (Name.ClearFlags( RF_Unreachable )).
```
Also HP2-only and recursive in the same way: `FArchiveTraceRoute` (H:~1690-1740, the `OBJ REFS`/trace-route
debugging walk, `Depth` counter). It is only reached from console debugging commands — leave it recursive, note it.

HP2-only blockers in this file:
- H:3313-3314 `const TCHAR* ClassName = GObjRegisterCount > 0 ? InClass->GetName() : reinterpret_cast<const TCHAR* const&>(InClass->GetFName());`
  — `GetFName()` returns a prvalue; binding it to a `reinterpret_cast` reference is ill-formed (cc probe gcc: *"invalid
  cast of a prvalue expression of type 'const FName'"*; clang: *"reinterpret_cast from rvalue to reference type"*). It
  only feeds `FPushMemTag`/`appMalloc`'s tag (a no-op outside `FMallocDebug`). ACTION: use
  `TEXT("StaticAllocateObject")` in the `else` arm (what MSVC effectively produced was the `FNameEntry*` read as a string).
- H:1426 `for( i=0; …)` after `for( INT i=0; …)` in the profiling dump — inside `#if DO_GUARD_SLOW` (H:1407), i.e.
  only `_DEBUG` builds (`Inc/UnBuild.h:41-46`). Hoist `INT i;` when the Debug preset is first built.
- `__STATIC_LINK` branches (H:1332-1333 `AUTO_INITIALIZE_REGISTRANTS`, H:1587, H:4002, H:4016-4027 `FindNative`) —
  dead with `__STATIC_LINK` undefined; delete with the rest (§7.2). The live `#else` of `UPackage::GetDllExport` calls
  `appGetDllExport`, which the registry answers (§5.12 #7-9).

Keep: HP2's reverse-order `Destroy`/purge in `PurgeGarbage` (433 behaviour), `FPushMemTag`s, `GIsStrict`.

### 5.12 `Src/UnUnix.cpp` — CONFLICTS=5 (epic→tmpl 309, epic→HP2 224) — **take the template's file, re-add the 433 pieces**

HP2's file is the Loki-era Linux core (`dlopen`, `fork`+`system()`, nine-signal `__Context` + `longjmp`, `rdtsc`
calibration via `appSleep(1)`); the template's is ut99dc's SDL rewrite plus ufront 2.7b/2.12/2.13.2/2.33. The dry
run's 5 conflicts understate the gap: the cc probe of the merged file (template headers) produced **62 gcc / 19 wasm
errors**, all from HP2-only regions (`__Context`, `sigaction`/`SIG*` without `<signal.h>`, `waitpid` without
`<sys/wait.h>`, `strupr` redefinition vs the `UnGnuG.h` macros, `jmp_buf`/`longjmp`). Strategy: **copy
`/home/ion/engines/engine-ut99/Source/Core/Src/UnUnix.cpp` and port HP2's additions into it** (list below).

| # | Template content (T lines) | Commit / why | HP2 (H lines) | Class | ACTION |
|---|---|---|---|---|---|
| 1 | T:12-60 includes: `<SDL2/SDL.h>` under `PLATFORM_SDL` (DC: `<kos.h>`; else `#error`), `<emscripten.h>` under `__EMSCRIPTEN__`, `<dlfcn.h>` only `#ifndef UNREAL_STATIC`, Win32 `<winsock2.h> <windows.h> <process.h>` else `<netdb.h>`, `#include "UnUnix.h"` | cbdb054 (SDL platform layer), d5a6bfd, 301a9ca | H:12-32 `#if __GNUG__`, `<dlfcn.h>`, `<netdb.h>` | ADAPT | take; drop the DC branch. If HP2's process-exit tracking is kept (#6), add `<signal.h>` + `<sys/wait.h>` under `#if !defined(PLATFORM_WIN32) && !defined(__EMSCRIPTEN__)` |
| 2 | T:81-104 `USystem::USystem()` + `StaticConstructor()` | Epic | H:62-84 — **identical** to the template and to HP2's own `UnVcWin32.cpp:134-156` (433 `USystem` adds `LicenseeMode`, `SaveSlotPath`, `MapPath` members, `Inc/UnCorObj.h:130-160`, none of them config-registered) | PRESENT | nothing |
| 3 | T:105-200 `USystem::Exec`: `RELAUNCH` via `_spawnl` on Win32 / `execl(…, (char*)NULL)` else; no DC | 301a9ca (2.33), cbdb054 (varargs sentinel must be a pointer) | H:86-165 Epic's (`execl(…, NULL)`) | AC | take |
| 4 | **T:201-245 `appRequestExit(0)` on the web navigates back to the launcher** (`EM_ASM`/`MAIN_THREAD_EM_ASM` `setTimeout(function(){ window.location.href = window.location.pathname; }, 0)`) | d5a6bfd (2.12) + 53cbaf1 (2.13.2: only the main thread can navigate under `PROXY_TO_PTHREAD`) | H:167-187 Epic's | AC | take. HP2's in-game "Quit" goes through `appRequestExit(0)` exactly like UT's |
| 5 | T:247-259 `ClipboardCopy/Paste` static-FString stubs | Epic | H:189-200 same; HP2 also stubs `appClipboardCopy/Paste` (H:569-582) | PRESENT | nothing |
| 6 | **T:263-292 `appGetDllHandle` under `UNREAL_STATIC`: strip path + extension, build `GLoaded<Pkg>`, `return appGetStaticExport( Test );`** | 17b0864 — the "handle" is the address of the `GLoaded<Pkg>` byte registered by `IMPLEMENT_PACKAGE_PLATFORM`; NULL = "no native code linked" → `UObject::BindPackage` loads the `.u` only | H:210-262: `#if __STATIC_LINK INT* FalseHandle; return (void*)FalseHandle; #endif` (returns an **uninitialised pointer**) then `dlopen(NULL)`/`dlsym`/`dlopen(file)` | ADAPT | take the template's; delete HP2's body |
| 7 | T:345-360 `appFreeDllHandle`: nothing under `UNREAL_STATIC`, `FreeLibrary` on Win32, `dlclose` else | 17b0864/301a9ca | H:266-273 `dlclose` | AC | take |
| 8 | **T:363-385 `appGetDllExport` under `UNREAL_STATIC`: `return appGetStaticExport( ProcName );`** | 17b0864 — resolves `int<Class>exec<Func>` (natives, from `IMPLEMENT_FUNCTION`) and `autoclass<Class>` lookups | H:279-298 `dlsym` | ADAPT | take |
| 9 | T:388-395 `appDebugBreak()` empty (or `__builtin_trap`) | cbdb054 | H:303-310 `#ifdef ASMLINUX asm("int $03"); #endif` — `#ifdef` is true even for `ASMLINUX 0`, and `int $03` does not assemble for wasm32 | ADAPT | take the template's |
| 10 | T:400-440 `void appCreateProc( URL, Parms )` (`fork`+`execv` / `_spawnv` on Win32 / DC stub) | 301a9ca | H:314-360 **433 API**: `void* appCreateProc( URL, Parms, UBOOL bRealTime )` (`fork` + `_exit(system(...))`), `UBOOL appGetProcReturnCode( void*, INT* )` fed by a `SIGCHLD` handler `HandleChild` into `static TMap<pid_t,int>* ExitCodeMap` (created in `appPlatformInit`, H:659, handler installed H:670-675). Declared by HP2 `Inc/UnFile.h:82-83`; **called by `Engine/Src/AStatLog.cpp:24,35,45,72,88` and `Engine/Src/UnAudio.cpp:584-611`** | ADAPT (433 idiom) | **keep HP2's signatures** (the template's would break Engine). Bodies: native Linux = HP2's; `__EMSCRIPTEN__` = `return NULL;` and **`appGetProcReturnCode( NULL, … )` must return 1 with `*ReturnCode=-1`** — `UnAudio.cpp:590,611` spin `while(!appGetProcReturnCode(h,&r)) appSleep(0.1);` (those sites are editor-only XA encoding and currently `check(0)` off `_WINDOWS`, but a stat-log call would otherwise hang); MinGW = `_spawnv(_P_NOWAIT, …)` returning the process handle + `GetExitCodeProcess`, or the same NULL stub |
| 11 | T:445-465 `appTimestamp` | Epic | H:366-384 same | PRESENT | nothing |
| 12 | T:466-478 `appGetTime` — `utime(Filename,&FileTime)` with an **uninitialised** `utimbuf` | Epic bug kept by both | H:387-398 same | PRESENT (bug in both) | not a port task; note: `utime()` **sets** the file's times to stack garbage — never call `appGetTime` on a save/ini (no caller in HP2 `Source/` outside Win32/PS2 files, grep 2026-09-26). Fix to `stat()` if a caller appears |
| 13 | T:482-500 `DOUBLE appSecondsSlow()` (gettimeofday accumulator) | Epic | H:403-422 **`FTime appSecondsSlow()`** (same body, `return TimeCounter;` converts through `FTime(double)`) | ADAPT (433 idiom) | keep HP2's return type (declared `FTime` in `Inc/UnFile.h:262`) |
| 14 | T:504-509 `#if !DEFINED_appCycles DWORD appCycles(){ return (DWORD)appSeconds(); }` | Epic | H:424-429 same text — **`FTime` has no conversion to `DWORD`** → compile error whenever `DEFINED_appCycles` is 0 | N/A once #22 lands | the template's `UnGnuG.h` defines `DEFINED_appCycles 1` (§4.2 #14), so the block is compiled out; delete it anyway |
| 15 | T:537-550 `appSleep( FLOAT )` | Epic `usleep((INT)(Seconds*1000000))` | H:458-465 `appRound(Seconds*1000000)` | PRESENT | keep HP2's (it needs the fixed `appRound`, §4.2 #12). Web note: `usleep` under ASYNCIFY yields to the browser — same as the template |
| 16 | T:556-561 `appLaunchURL` empty body | Epic | H:477-482 same empty body (HP2's `appClipboardPaste`, H:576-583, returns an empty `FString` — also fine) | PRESENT | nothing (correction of the 2026-09-25 run's note: there is no `return` in HP2's `appLaunchURL`) |
| 17 | T:589-645 `appCleanFileCache`, `appCreateGuid` (Win32 → `appGetGUID` from `UnMinGW.cpp`) | 301a9ca | H:510-566 Epic's | AC | take |
| 18 | **T:666-705 `appBaseDir()`: `getcwd()+"/"` on Emscripten ("the launcher chdir's to /System"), `SDL_GetBasePath()` natively, DC list, `./` fallback; returns `const char*`** | d5a6bfd (2.7b: `SDL_GetBasePath()` is "/" under Emscripten) | H:589-602 `getcwd` always | ADAPT | take; drop the DC list. HP2's `appInit` derives `MapPath` from it (§5.10 table) |
| 19 | T:711-758 `appComputerName`/`appUserName`: Win32 `appWinSockInit()` + `GetUserNameA`; `getlogin()` NULL-safe | 301a9ca | H:605-624 (H:621 `appStrncpy( Result, getlogin(), … )` — `getlogin()` is NULL in containers and on wasm → crash) | AC | take |
| 20 | **T:778-818 `appPlatformInit`: `SDL_GetPlatform()`, `GSecondsPerCycle = 1.0 / SDL_GetPerformanceFrequency()`, `GPageSize=4096`, `GProcessorCount = SDL_GetCPUCount()` — no rdtsc, no 1-second calibration sleep** | cbdb054 | H:645-677: `GTimestamp=1; a=appCycles(); appSleep(1); b=appCycles(); GSecondsPerCycle = 1.0/(b-a);` + `ExitCodeMap = new TMap<pid_t,int>;` + `sigaction(SIGCHLD, …)` | ADAPT | take the template's body; re-add `ExitCodeMap` + the `SIGCHLD` handler under the same guard as #10 (native Linux only). `GSecondsPerCycle` is **`FLOAT`** in 433 (`Inc/Core.h:437`, `Src/Core.cpp:143`): `1.0/1e9` narrows to a float with ~6e-8 relative error — a constant scale error, harmless for `appSeconds()` deltas |
| 21 | T:840-850 `appUnixPath`, T:856-890 `appGetLocalIP` with a NULL-safe `hostent` (Epic dereferenced NULL when the host name does not resolve — common in containers/wasm) | cbdb054, 301a9ca | H:700-730 Epic's | AC | take |
| 22 | **T:897-925 out-of-line `CORE_API DOUBLE appSeconds()` = `SDL_GetPerformanceCounter() * GSecondsPerCycle` and `CORE_API DWORD appCycles()` = `SDL_GetPerformanceCounter()` (MSVC: `QueryPerformanceCounter`; DC branch)** | cbdb054 — replaces the rdtsc inlines (§4.2 #14) | none (HP2's are the `ASMLINUX` inlines in `UnGnuG.h`) | ADAPT (433 idiom) | take with the 433 return type: `CORE_API FTime appSeconds() { return FTime( (DOUBLE)SDL_GetPerformanceCounter() * GSecondsPerCycle ); }` (`FTime(double)` scales by 2^32 into a `long long`; 1e6 s of uptime = 4.3e15, far inside range). Drop the DC branch |
| 23 | (removed by template) `stricmp`/`strnicmp`/`strupr` function definitions | cbdb054 — replaced by `UnGnuG.h` macros (§4.2 #16) | H:736-770 | ADAPT | delete (the cc probe's `redefinition of 'char* strupr(char*)'`) |
| 24 | (removed by template) `__Context::StaticInit` / `HandleSignal` / nine `sigaction`s / `longjmp` | cbdb054 — setjmp guard gone (§4.8 #1) | H:772-893 | ADAPT | delete. If a native crash hook is wanted, a plain `sigaction(SIGSEGV)` that logs and `_exit`s — never `longjmp` |

**HP2 433 functions the Unix core must ADD (declared in HP2 headers, defined today only in `UnVcWin32.cpp`):**

| Function | Declared | Callers (GCC-built) | Proposed Unix body |
|---|---|---|---|
| `CORE_API const TCHAR* appUserDir()` | `Inc/UnFile.h:235` | `Inc/FOutputDeviceFile.h:42`, `Src/UnMisc.cpp:1915,1935,2002,2005`, `Engine/Src/UnCamera.cpp:665,899`, `Engine/Src/UnReplay.cpp:40` (+ `Engine/Inc/UnEngineWin.h`, not built) | `return TEXT("");` — keeps inis/log in the cwd (`System/`), identical to the template's UT99 layout and the web's `/System` MEMFS; `appInit` then takes its `else` branch (fix its `"..\\Save"` literal, §5.10). Alternative (`$HOME/.hp2/` natively) only after the web path works; must end with `/` if non-empty (callers concatenate) |
| `CORE_API FTime appProcessSeconds()` | `Inc/UnFile.h:263` | `Engine/Src/UnGame.cpp:843,1465` (load-time log line), `Render/Src/UnRender.cpp:703` (`appProcessSeconds().GetFloat()` stat) | CPU time via `clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &ts )` → `FTime( ts.tv_sec + ts.tv_nsec*1e-9 )` (HP2's Win32 body, `UnVcWin32.cpp:501-512`, sums `GetProcessTimes` user+kernel and falls back to `appSeconds()`), or simply `return appSeconds();` on `__EMSCRIPTEN__`. ⚠ **Do not call `clock()`**: Core's `Inc/UnFile.h:182` `#define clock(Timer) {Timer -= appCycles();}` macro-expands `clock()` into a syntax error (write `(clock)()` if the C function is ever needed) |
| `void* appCreateProc(…, UBOOL)`, `UBOOL appGetProcReturnCode(void*, INT*)` | `Inc/UnFile.h:82-83` | see #10 | see #10 |
| `FFileStream::Read/Create(×2)/Destroy(INT)` | `Inc/Core.h:392-395` | `Src/UnMisc.cpp:1754-1770` (`DestroyStream`) | stubs returning 0 (§8.1) |
| `appMsgf`, `appGetLastError`, `appDebugMessagef` | `Inc/UnFile.h` | none outside `UnVcWin32.cpp` (grep) | nothing; add stubs only if a later link asks |

## 6. HP2-only Core files and what to do with each

Source: `merge3.sh` `HP2-ONLY-OR-DROPPED` rows + `ls Source/Core/{Inc,Src}`; users from `grep -rln <name> Source/`
(2026-09-26). "Built" = listed in `Source/Core/CMakeLists.txt`.

| File(s) | What it is | Users in HP2 `Source/` | Built? | Disposition / ACTION |
|---|---|---|---|---|
| `Inc/AudioMutex.h`, `Src/AudioMutex.cpp` | Win32 `CRITICAL_SECTION` wrapper (`AudioMutex`, `AudioMutexGrabber`, global `audioFileMutex`); the header re-declares `WORD`/`DWORD` (`typedef unsigned long DWORD` — **conflicts** with the template's `uint32_t` `DWORD`), `LIST_ENTRY`, `RTL_CRITICAL_SECTION` "if windows headers are not included"; the `.cpp` `#include "windows.h"` | **none** (only its own two files) | no | **Leave unbuilt, do not port.** cc probe: gcc `fatal error: windows.h: No such file`. If NOpenALDrv ever needs a lock, use `SDL_mutex`/`std::mutex` in the driver, not this |
| `Inc/UnCoreNative.h`, `Src/UnCoreNative.cpp` | the 433 (Loki/PS2) static-link native tables: `MAP_NATIVE(cls,name)` → `{"int"#cls#name, &cls::name}`, `DECLARE_NATIVE_TYPE` (MSVC-only `(##type::*##type##Native)` token pasting — not valid for GCC's preprocessor), `IMPLEMENT_NATIVE_HANDLER`, `GNativeLookupFuncs[32]`, `FindNative()` | `Inc/Core.h:496-498` (`#if __STATIC_LINK`), `Src/UnObj.cpp:4026-4027` (`#if __STATIC_LINK … FindNative`), `UCC/Src/UCC.cpp:121-148` (fills `GNativeLookupFuncs[]` by hand), Engine twin `Engine/Src/UnEngineNative.cpp` | **yes** (`UnCoreNative.cpp` is in the HP2 CMake list, §3.4) — compiles to an empty TU (`#if __STATIC_LINK` at H:11) | **Superseded by the template's registry** (`IMPLEMENT_FUNCTION` → `STATIC_EXPORT(int<Class><func>)`, same name scheme as `MAP_NATIVE`'s `"int"#cls#name`, same return value — the address of a `Native` member-pointer variable). ACTION: remove `UnCoreNative.cpp` from `Core/CMakeLists.txt` (and `UnEngineNative.cpp` from Engine's), delete the `Core.h` include; keep the files as reference or delete them. The template's `UCC.cpp` replaces HP2's (launcher report) |
| `Inc/UnStat.h` | `STATS` profiling: `FClock` RAII (`clock(Stat)`/`unclock(Stat)` = `appCycles()`), `#define Clock(stat)`, `struct CORE_API FStats GStat` (mesh/particle/actor/memory counters) | `Inc/FMallocAnsi.h`, `Inc/FMallocWindows.h`, `Src/Core.cpp` (`GStat`), `Render/Inc/Render.h`, `Engine/Src/UnParticleFX.cpp`, `Editor/Src/UnEdCam.cpp`, `Editor/Src/EdHook.cpp` | header | **keep** (Engine/Render depend on `GStat`'s layout). Portable once `appCycles()` is out-of-line (§5.12 #22). Web cost: `FMallocAnsi` clocks every `Malloc/Realloc/Free` (§4.5) — measure |
| `Inc/FFileManagerArc.h` | Tim Sweeney's archive-interceptor file manager (post-v400 Epic code) | `Editor/Src/UMasterCommandlet.cpp:10` only | header | leave; matters only if `UMasterCommandlet.cpp` is in the Editor build list (template-editor report decides; the template's ucc does not need it) |
| `Inc/FFileManagerPSX2.h`, `Inc/FFileManagerPSX2CD.h` | PS2 EE file managers | `UCC/Src/UCC.cpp:79-80` (inside a PS2 branch) | no | leave unbuilt (CLAUDE.md "Do NOT": no PSX2 deltas) |
| `Src/UnPSX2.cpp`, `Src/VuCoreMath.dsm`, `Src/VuCoreMath.vsm` | "PSX2 port of UnVcWin32.cpp" (`eekernel.h`) + VU0 microcode for the vector transforms | none on our targets | no | leave unbuilt |
| `Inc/UnLinker.h` + `Src/UnLinker.cpp` | the 433 split of `ULinker`/`ULinkerLoad`/`ULinkerSave` out of `UnObj.cpp` (Epic v400 kept it in `Src/UnLinker.h` included by `UnObj.cpp`) | Core (+ `Engine`, `Editor` via `Core.h`) | yes | **keep**; receives the template's four `Src/UnLinker.h` hunks by hand (§5.9) |
| `Inc/UnMsg.h` | `WM_USER+923…` UnrealEd/Window message ids | `Editor/Src/EdHook.cpp`, `UnrealEd/Inc/ViewportFrame.h` | header | reference only; `EdHook.cpp` is replaced by the template's `NullEdHook.cpp` (editor report) |
| `Inc/afxres.h` | MFC resource header copy | `.rc` files of FrontEnd, WinDrv, Game, Window, Gesture*, DebuggerLaunch, Tools | header | reference only (a MinGW `windres` step for `Game/Src/Res/LaunchRes.rc` would need it — only if the Windows target gets HP2's icon/resources; the template's MinGW build does not compile `.rc`) |
| `Src/Core.dsp`, `Src/makefile` | MSVC6 project / Loki makefile | — | no | reference only (the makefile's file list is what HP2's `Core/CMakeLists.txt` mirrors) |
| `Src/UnVcWin32.cpp` (not HP2-only, but HP2-modified: 558 lines vs Epic) | the MSVC platform layer + HP2's `FFileStream` reader thread, `appUserDir`, `appProcessSeconds`, `coda` init | — | only under MSVC (never in our presets) | leave; it is the **reference** for every 433 function the Unix core must re-implement (§5.12 "must ADD" table) |

## 7. Build-433 idioms that change the template's approach

Each item: the 433 fact (with file:line), what it changes in the template's recipe, and the ACTION.

### 7.1 `DECLARE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage )` — four arguments

- HP2 `Inc/UnObjBas.h:285/299` (`DECLARE_BASE_CLASS`, two variants), `:326` (`DECLARE_CLASS`), `:336`
  (`DECLARE_ABSTRACT_CLASS`) all take `TPackage`; every HP2 class uses it (`DECLARE_CLASS(USystem,USubsystem,CLASS_Config,Core)`,
  `Inc/UnCorObj.h:132`). The argument is consumed **only** by the `__STATIC_LINK` `DECLARE_BASE_CLASS` (for
  `GetPrivateStaticClass##TClass`); in the non-`__STATIC_LINK` build it is decorative.
- The template drivers already copied into HP2 (commit `e47a3c7`, "unadapted") use 3 arguments — verified:
  `NSDLDrv/Inc/NSDLDrv.h:26,109`, `NOpenALDrv` (`DECLARE_CLASS(UNOpenALAudioSubsystem, UAudioSubsystem, CLASS_Config)`),
  `NullDrv` (`DECLARE_CLASS(UNullRenderDevice,URenderDevice,CLASS_Config)`), `IpDrv` (`UUpdateServerCommandlet`).
- **Decision needed (one line, once):** (a) edit every copied driver's call sites to add the package (PLAN §4
  Phase B item 1 already says "4-arg `DECLARE_CLASS(…,NullDrv)`" — the chosen route), or (b) make the Core macros
  variadic (`DECLARE_CLASS( TClass, TSuperClass, TStaticFlags, ... )`) so both spellings compile. (a) keeps HP2's tree
  uniform and is what PLAN says; (b) keeps the drivers byte-closer to the template (cheaper future re-syncs). This
  report recommends **(a)** for consistency with PLAN, and notes that (b) is a 4-line change if re-syncing the drivers
  from the template later proves painful. Either way `a680288`'s "no parentheses" fix goes into the 4-arg macro (§4.9).

### 7.2 Two static-link schemes: HP2's `__STATIC_LINK` vs the template's `UNREAL_STATIC`

| Aspect | HP2 433 `__STATIC_LINK` (Loki/PS2) | Template `UNREAL_STATIC` (ut99dc) |
|---|---|---|
| Class registration | `GetPrivateStaticClass##TClass` functions called from `AUTO_INITIALIZE_REGISTRANTS` (`Inc/UnObjBas.h:1079-1110`, invoked `Src/UnObj.cpp:1332-1333`) | `autoclass<Class>` global + `STATIC_EXPORT(TClass, autoclass##TClass)` (§3.1); classes are constructed by their static `PrivateStaticClass` objects exactly as in a DLL build |
| Native functions | hand-maintained `MAP_NATIVE` tables per class (`UnCoreNative.cpp`, `Engine/Src/UnEngineNative.cpp`) looked up via `GNativeLookupFuncs[32]` filled in `UCC/Src/UCC.cpp:121-148` | `IMPLEMENT_FUNCTION` registers `int<Class><func>` automatically (`Inc/UnScript.h` T:28-31) — no table to keep in sync |
| Package handle | `appGetDllHandle` returns an uninitialised `INT*` (`Src/UnUnix.cpp:213-216`) | the address of the registered `GLoaded<Pkg>` byte |
| Export lookup | `UPackage::GetDllExport` → `FindNative()` (`Src/UnObj.cpp:4026-4027`) | `appGetDllExport` → `appGetStaticExport()` |
| Status | never defined by any CMake file; the PS2/Loki build scripts are gone | proven on native/web/MinGW for UT99 |

**Decided (PLAN §2 line 89): `UNREAL_STATIC`.** ACTION: leave `__STATIC_LINK` undefined and delete its branches
(Core: `Inc/Core.h:496-498`, `Inc/UnObjBas.h:284 #if !__STATIC_LINK` (keep its body H:285-297) / `:298-325 #else` branch (delete), `:386-420`,
`:1079-1110`, `Src/UnObj.cpp:1332-1333, 1587, 4002, 4016-4027`, `Src/UnUnix.cpp:213-216`; drop `UnCoreNative.cpp` from
CMake). **Registry-specific risk the 433 tree adds:** name collisions between packages (`int<Class><func>`, `<Class>`,
`GLoaded<Pkg>`) — after the first link, `nm -C <Game> | grep GStaticExport | sort | uniq -d` must print nothing.

### 7.3 The 433 script VM: `EX_DebugInfo`, renumbered opcodes, `FFrame::Parent`

`Inc/UnStack.h` renumbers the expression tokens (`EX_LineNumber 0x15`, `EX_DynArrayCount 0x37`, `EX_DebugInfo 0x38`,
`EX_GlobalFunction 0x39`, conversions `0x3a…0x46`, `EX_StringToName 0x5a`) and the shipped `.u` bytecode is compiled
against those numbers; `P_FINISH` steps over an `EX_DebugInfo` (`Inc/UnScript.h:70-71`), `SerializeExpr` handles it
via `HANDLE_OPTIONAL_DEBUG_INFO` (`Src/UnClass.cpp:1400-1415`), `UObject::execDebugInfo` + `UDebugger` (`Inc/Core.h:501-505`).
The template's opcode-adjacent hunks are **alignment-only** (DC `memcpy` reads) and do not interact. ACTION: never
take an Epic/template value for an opcode or native index; resolve every UnStack.h/UnScript.h/UnCorSc.cpp conflict
toward HP2's numbering. Dynamic arrays grow on write in 433 (`execDynArrayElement`, §5.8 #2).

### 7.4 `FTime` — 32.32 fixed-point time instead of `DOUBLE`

`Inc/Core.h:24-53`: `class FTime { long long v; }` with `FTime(float)`, `FTime(double)` (scale by `FIXTIME` =
`4294967296.f`), `GetFloat()`, `FTime - FTime → float`, `FTime + float → FTime`. `appSeconds()`, `appSecondsSlow()`,
`appProcessSeconds()` return it (`Inc/UnGnuG.h:193-273`, `Inc/UnFile.h:262-263`); `GSecondsPerCycle` is `FLOAT`
(`Inc/Core.h:437`, `Src/Core.cpp:143`); `GTempTime` replaces `GTempDouble`; `operator<<(FArchive&, FTime&)` serialises
it as a double (`Src/UnMisc.cpp`). Engine uses it on the net/camera/game paths (`UnNetDrv.h`, `UnConn.h`, `UnChan.h`,
`UnCamera.h`, `UnGame.cpp:843`), Render (`UnRender.cpp:703`), ALAudio.

Consequences for the template's recipe:
1. `UnGnuG.h`: declare `CORE_API FTime appSeconds();`, `CORE_API FTime appSecondsSlow();`, `extern CORE_API FLOAT GSecondsPerCycle;` (§4.2).
2. `UnUnix.cpp`: `appSeconds()` returns `FTime( (DOUBLE)SDL_GetPerformanceCounter() * GSecondsPerCycle )` (§5.12 #22).
   **Precision recommendation (new):** on native Linux `SDL_GetPerformanceCounter()` is `CLOCK_MONOTONIC` nanoseconds
   **since boot**; `FTime::GetFloat()` of an absolute value then loses precision on long-uptime machines (a float has
   24 mantissa bits: at 10^6 s uptime its step is 0.0625 s). HP2 code does call `appSeconds().GetFloat()` (e.g. the
   commented debug in `UnVcWin32.cpp:677`; grep Engine for live ones). ACTION: subtract a base counter captured in
   `appPlatformInit` so `appSeconds()` starts near 0 — harmless for deltas, protects every `GetFloat()` caller. (On the
   web the SDL counter is `performance.now()`-based and already small.)
3. The drivers copied from the template (`NSDLDrv`, `NOpenALDrv`, `XOpenGLDrv`, `UTGLR`, `IpDrv`, `WebRTCDrv`) use
   `DOUBLE Now = appSeconds();` — each becomes `FTime`, and differences are already `float` (drivers report).
4. Latent 433 quirks, leave alone unless a caller appears: `FTime::operator*(float)`/`operator/(float)` build through
   the `float` ctor and so rescale by `FIXTIME` a second time; `operator==`/`GetFloat()` are non-`const`; an `int`
   literal (`FTime t = 0;`) is ambiguous between the `float`/`double`/private `long long` ctors under GCC/clang.

### 7.5 `IMPLEMENT_PACKAGE` / `THIS_PACKAGE` / `UPACKAGE_NAME`

HP2 `Inc/UnObjBas.h:425-434` has Epic's two `IMPLEMENT_PACKAGE` variants writing `GPackage`; 17 packages call it
(`Core.cpp:208`, `Engine.cpp:20`, `Render.cpp:14`, `UnFractal.cpp:116` (Fire), `SoftDrv.cpp:12`, `Editor.cpp:25`, `ALAudio.cpp:15`,
+ the copied template drivers). ACTION: the template's single definition (`THIS_PACKAGE` = `GPackage##UPACKAGE_NAME`
under `UNREAL_STATIC`, §4.1 #3, §4.9 #4); every package CMake already passes `UPACKAGE_NAME=${PROJECT_NAME}`
(HP2 skeleton). No 433 variant of `IMPLEMENT_PACKAGE` exists — nothing extra to adapt.

### 7.6 `TArray` hierarchy inverted (`TRefArray<T>` ← `TArray<T>` ← `FArray`)

Covered in §4.17: dependent base → the template's `this->` hunks are insufficient; add `using TRefArray<T>::Data;
using TRefArray<T>::ArrayNum;` and `using TMapBase<TK,TI>::Pairs;`. Also `FArray` has no out-of-line members any more
(`UnMisc.cpp` conflict #1 resolves to "absent", §5.10 #2), and `TTransArray<T>::Insert` calls `FArray::Insert` on a
class that does not derive from `FArray` (fix to `TArray<T>::Insert( Index, Count )`).

### 7.7 Entry-pointer `FName`, `NAME_SIZE 128`, `HardcodedNames[]`

`Inc/UnName.h` (THEIRS-UNCHANGED): `FName` stores an `FNameEntry*`, not an index. No template hunk depends on the
representation; `AllocateNameEntry` is found by ADL in HP2 (§5.5 #2). Side effect: HP2's `reinterpret_cast<const TCHAR*
const&>(InClass->GetFName())` memtag hack in `StaticAllocateObject` (§5.11) relied on that representation — replace it.

### 7.8 Save-game infrastructure in Core: `appUserDir`, save slots, persistent-actor cache

`Inc/UnFile.h:235-245`, `Src/UnMisc.cpp:1826-1853, 1996-2140`: `appUserDir()`, `appSaveSlotDir()`, `appSetSaveSlotDir()`,
`Save/LoadPersistentActorCache()`, `USystem::SaveSlotPath`/`MapPath`/`LicenseeMode` (`Inc/UnCorObj.h:130-160`). The
template has none of this. ACTION: the Unix `appUserDir()` decides where inis, log, saves live (§5.12, recommendation
`""` → everything relative to `System/`, like the template); fix the backslash `"..\\Save"` literal; keep six `Paths=`
lines in any web `Default.ini` (the `Paths[5]` coupling); the web persistence layer (ufrontend FetchFS/IDB) must persist
`../Save/Slot<N>/` **and** `../Save/Slot<N>/cache/*` (the `_pa` persistent-actor files) — coordinate with the
web-layer report.

### 7.9 `FFileStream` / `GFileStream` — ALAudio's streaming singleton lives in Core

`Inc/Core.h:344-411` + `Src/UnMisc.cpp:1604-1810` (portable half, `Init`ed in `appInit` H:1990-1997 from
`[ALAudio.ALAudioSubsystem] Channels`) + `Src/UnVcWin32.cpp:665-940` (reader thread, `Read`, `Create`×2, `Destroy(INT)`).
Only `ALAudio` uses it. The template has no counterpart (NOpenALDrv streams on its own). ACTION: §8.1 (Unix definitions
of the four Win32-only members; neutralise the `Destroy()` busy-wait).

### 7.10 `STATS 1` + `FMallocAnsi` clocks every allocation; `DEFAULT_ALIGNMENT 16`; `FMemStack::Init(Size, Tag)`

`Inc/UnBuild.h:56` `STATS 1`; `Inc/FMallocAnsi.h` wraps `Malloc/Realloc/Free` in `Clock(GStat.MemTime)` (§4.5 —
web perf hazard); `Inc/UnGnuG.h:83` `DEFAULT_ALIGNMENT 16` (PS2) → use 8 (§4.2 #5); `FMemStack::Init` takes a tag
(`GMem.Init( 65536, TEXT("GMem") )`, §4.16) — template drivers do not call it.

### 7.11 HP2 retail was a `UNICODE` build; the port is ANSI

Evidence: `wcscmp` on `TCHAR*` (`Src/UnClass.cpp:1439`), `SHGetFolderPathW` (`Src/UnVcWin32.cpp:1057`). The
template's GCC build is ANSI (`TCHAR`=`char`, `Inc/UnGnuG.h` `TCHAR_TO_ANSI(str) str`). Package files store names as
ANSI in v61-era formats either way, so content is unaffected; but every HP2-only `wcs*`/`L"…"`/`_wcs*` call in
Engine/Editor/Render is a compile error under the port — grep them per package (`grep -rn "wcs\|L\"" Source/<Pkg>/Src`).

### 7.12 `PACKAGE_FILE_VERSION` / `PACKAGE_MIN_VERSION` are runtime globals + licensee version

`Src/Core.cpp:14-15` (`=79` / `=60`), `Inc/UnObjVer.h:38-42`, read by `Src/UnLinker.cpp`. The template's
`a57d578` version-mismatch hunk is GUID-based and unaffected. ACTION: none; do not replace with the template's
compile-time constants.

## 8. HP2 Core portability problems the template's hunks do not cover

The template was cut against Epic v400; these are HP2-433 lines that stop a GCC/clang/MinGW build of Core, or
break it at run time on Linux/the web, **regardless of how the merge is resolved**. Evidence column: "cc" = the
syntax-only probe of the merged tree (gcc -m32 / emsdk clang wasm32, §5 intro); "read" = code reading.

### 8.1 `FFileStream` has no Unix implementation (link blocker)

`Src/UnMisc.cpp:1754-1770` `FFileStream::DestroyStream` calls `Read( INT, INT )` and `Destroy( INT )`; those two and
both `Create(…)` overloads are defined only in `Src/UnVcWin32.cpp:706-940` (vorbisfile `ov_*`, the EA `coda` codec).
With `UNREAL_STATIC` + `--whole-archive` every binary links `UnMisc.o` → **undefined references** to
`FFileStream::Read/Create/Destroy(int)`. Only ALAudio (`ALAudio/Src/ALAudioSubsystem.cpp:364-670`) uses `GFileStream`,
and the port replaces ALAudio with NOpenALDrv. ACTION (Core-only, minimal):
```cpp
// Src/UnUnix.cpp — HP2 433: the Win32 reader lives in UnVcWin32.cpp; no streaming thread on Unix/web.
UBOOL FFileStream::Read( INT StreamId, INT Bytes )                            { return 0; }
UBOOL FFileStream::Create( INT StreamId, const TCHAR* Filename )             { return 0; }
UBOOL FFileStream::Create( INT StreamId, TLazyArray<BYTE>* RawData, INT N )  { return 0; }
UBOOL FFileStream::Destroy( INT StreamId )                                    { return 1; }
```
and in `Src/UnMisc.cpp:1629` change `#if !__UNIX__` → `#if _MSC_VER` (otherwise `FFileStream::Destroy()` spins forever
at exit, §5.10). If a later decision keeps ALAudio's Ogg streaming alive on Linux, move `UnVcWin32.cpp:665-940` into a
portable `Src/UnFileStream.cpp` with an `SDL_Thread` reader instead — not needed for the template's driver set.

### 8.2 433 platform functions missing from the Unix core (link blockers)

`appUserDir()`, `appProcessSeconds()`, and HP2's 3-argument `appCreateProc` + `appGetProcReturnCode` — full table with
callers and proposed bodies in §5.12 ("must ADD"). Decision recommended here: **`appUserDir()` returns `""` on every
GCC target** (inis/log/`Save` relative to `System/`, exactly the template's UT99 layout and what the web MEMFS/IDB
expects), plus the `"..\\Save"` → `TEXT("..") PATH_SEPARATOR TEXT("Save")` fix in `appInit` (§5.10).

### 8.3 Compile blockers at HP2-only lines (MSVC6 / UNICODE / PS2 idioms)

| File:line (HP2) | Problem | Evidence | Fix |
|---|---|---|---|
| `Inc/UnGnuG.h:10-33` | `#error Unsupported platform` unless `__LINUX_X86__`/`__PSX2_EE__` (no CMake defines either) | read | take the template's file (§4.2) |
| `Inc/UnGnuG.h:152` | `typedef unsigned int size_t;` clashes with glibc/musl/MinGW | read | template's file |
| `Inc/UnGnuG.h:129` | `typedef unsigned int OWORD __attribute__ ((mode (TI)));` — gcc `-m32`: "unable to emulate 'TI'" | read | drop (no user) |
| `Inc/UnGnuG.h:201-241` | `appRound(FLOAT f)`/`appFloor(FLOAT f)` bodies use `F` | read | template's `floorf` bodies |
| `Inc/UnGnuG.h:193` + `:330` | `GTimestamp` declared with C++ and then C linkage | read | template's file |
| `Inc/UnGnuG.h:19,31` | `"(" __VERSION__ ")"` spelled without spaces → C++11 user-defined-literal error | read | template's `COMPILER` |
| `Inc/UnGnuG.h:36-59` | `__Context` (setjmp + 9 `sigaction`s) needs `<signal.h>`/`<setjmp.h>`, `<sys/wait.h>` (absent on MinGW) | cc (62 errors in UnUnix.cpp) | delete with the template's UnFile.h guard macros (§4.8 #1) |
| `Inc/FFileManagerGeneric.h:129` | `SQWORD FFileManagerGeneric::GetGlobalTime(…)` qualified inside the class | read | drop the qualifier (§4.3) |
| `Inc/UnCoreNet.h:39` | extra qualification in-class | read | template hunk (§4.12) |
| `Inc/UnTemplate.h` (whole) | dependent-base member access (`ArrayNum`, `Data`, `Pairs`), missing `typename`; `TTransArray::Insert` → `FArray::Insert` | syn probe (28 errors → 0 with the fix) | §4.17 |
| `Inc/UnObjBas.h:989-1002` | `ConstructObject<T>` before `UClass` is complete; HP2's second overload calls the first | read | move both to UnClass.h (§4.9 #6) |
| `Inc/UnMem.h` | default args on both friend declaration and definition | read | template hunk (§4.16) |
| `Src/UnAnsi.cpp:282-290` | `appSprintf` falls off the end on non-MSVC (UB) | read | `return Result;` (§5.2 #3) |
| `Src/UnAnsi.cpp:316-334` | `strstr`/`strchr` return `const char*` into `TCHAR*` | read | `const_cast` (§5.2 #4,#6) |
| `Src/UnMath.cpp:23/33` | `for( DWORD i…)` then `for( i…)` | cc gcc 1 / wasm 7 | hoist `DWORD i;` (§5.6) |
| `Src/UnMisc.cpp:2047-2054` | `p` used after its `for` scope | cc | hoist (§5.10) |
| `Src/UnMisc.cpp:2083/2097, 2119/2134` | `j` reused after its `for` scope | cc (10 errors) | hoist (§5.10) |
| `Src/UnMisc.cpp:1839` | `FString + TCHAR[32]` ambiguous (clang) | cc wasm | `(const TCHAR*)itoa` (§5.10) |
| `Src/UnMisc.cpp:2274-2276` | `appFromUnicode` `Count` after its `for` | read | template hunk (§5.10 #10) |
| `Src/UnClass.cpp:1439` | `wcscmp` on `TCHAR*` (retail was UNICODE) | cc gcc/wasm | `appStrcmp` (§5.7) |
| `Src/UnObj.cpp:3313-3314` | `reinterpret_cast<const TCHAR* const&>( <prvalue FName> )` | cc gcc/wasm | `TEXT("StaticAllocateObject")` (§5.11) |
| `Src/UnObj.cpp:1426` | `i` reused after its `for` (only `DO_GUARD_SLOW`, i.e. `_DEBUG`) | read | hoist when building Debug |
| `Src/UnUnix.cpp:213-216` | uninitialised `FalseHandle` returned (`__STATIC_LINK`) | read | template's `appGetDllHandle` (§5.12 #6) |
| `Src/UnUnix.cpp:303-310` | `#ifdef ASMLINUX asm("int $03")` — true with `ASMLINUX 0` (template `UnGnuG.h:209-210`), does not assemble for wasm | read | template's `appDebugBreak` |
| `Src/UnUnix.cpp:424-429` | `(DWORD)appSeconds()` — `FTime` has no integer conversion | read | compiled out by `DEFINED_appCycles 1`; delete |
| `Src/UnUnix.cpp:736-770` | `stricmp/strnicmp/strupr` definitions vs the template's macros | cc (`redefinition of 'char* strupr(char*)'`) | delete (§5.12 #23) |
| `Src/AudioMutex.cpp:1` | `#include "windows.h"` | cc | not built (§6) |
| `Src/UnCoreNative.cpp` / `Inc/UnCoreNative.h` | MSVC-only `(##type::*##type##Native)` token pasting | read | not compiled without `__STATIC_LINK`; drop from CMake (§6) |

### 8.4 Run-time hazards on Linux / the web (compile fine, misbehave)

| Where (HP2) | Hazard | Fix |
|---|---|---|
| `Src/UnMisc.cpp:2030` | `debugf("%s", GSys->SaveSlotPath)` passes an `FString` through `...` → **clang traps at run time**; runs every boot | `*GSys->SaveSlotPath`; enumerate every other site with one `-Wnon-pod-varargs` web build (the template's CMake silences it) |
| `Src/UnMisc.cpp:1629` | `FFileStream::Destroy()` busy-wait live without `__UNIX__` → hang at exit | `#if _MSC_VER` (§8.1) |
| `Src/UnMisc.cpp:1955-1963` | `default.ini` (lower-case) → empty `Language` on case-sensitive FS | `Default.ini` |
| `Src/UnMisc.cpp:2011` | `SavePath = TEXT("..\\Save")` → a directory literally named `..\Save` | `PATH_SEPARATOR` |
| `Src/UnMisc.cpp:1912-1950` + `Inc/FFileManagerGeneric.h:129` | `GetGlobalTime(Default.ini) > GetGlobalTime(Game.ini)` regenerates the user's inis whenever the (re-preloaded) `Default.ini` is newer — every web page load | `GetGlobalTime` → 0 on `__EMSCRIPTEN__` (§4.3) |
| `Src/UnMisc.cpp:2004-2006` | `GSys->Paths[5]` indexed unconditionally when `appUserDir()` is non-empty; `GConfig->SetString("Core.System","paths", …)` rewrites the last-added `Paths` pair | keep six `Paths=` lines (save path last) in every shipped/web `Default.ini`, or keep `appUserDir()==""` (branch not taken) |
| `Src/UnMisc.cpp:2050-2054` | `MapPath` needs `System` in `appBaseDir()` | launcher convention (cwd = `…/System/`) |
| `Src/UnUnix.cpp:621` | `appStrncpy( Result, getlogin(), … )` — `getlogin()` is NULL in containers/wasm | template's NULL-safe `appUserName` (§5.12 #19) |
| `Src/UnUnix.cpp` `appGetLocalIP` | Epic derefs a NULL `hostent` | template's body (§5.12 #21) |
| `Src/UnGUID.cpp:102-103` | `*(unsigned32*)nodeid[0] = ip32;` wild write to address 0..255 on every `appCreateGuid()` (each package save — HP2 **saves games**) | template hunk (§5.3) |
| `Src/UnAnsi.cpp:282-290` | `appSprintf` return value garbage / UB | template hunk (§5.2) |
| `Src/UnMisc.cpp:2289-2291` | `appUnwindf` latches `GIsCriticalError` → gagged log + dangling object table after the first recovered load error | template hunk (§5.10 #11) |
| `Src/UnObj.cpp:3448-3547` | recursive GC tagger → `Maximum call stack size exceeded` on deep HP2 levels at `-O0` web | iterative rewrite (§5.11 #11) |
| `Src/UnObj.cpp:2137-2160` + `GIsStrict` | `-STRICT` turns every load warning into a fatal `appErrorf` | never pass `-STRICT` on the web |
| `Inc/Core.h:55-59` `SUPPORTS_PRAGMA_PACK` | keyed on `__LINUX__` (never defined with the template's `UnGnuG.h`) → the `#pragma pack(push,8)` around third-party codec headers (`UnVcWin32.cpp:654`, `ALAudio/Src/ALAudioSubsystem.cpp:15`) is skipped; under `-fpack-struct=4` `OggVorbis_File` would mismatch the separately-built libvorbis | `#if _MSC_VER || __ICC || __GNUC__` (§4.1) — relevant only if ALAudio/vorbis code is compiled |
| `Inc/UnGnuG.h:83` `DEFAULT_ALIGNMENT 16` | PS2 value; harmless but not retail | 8 (§4.2 #5) |
| `Inc/FMallocAnsi.h` | two `appCycles()` (JS `performance.now()` on the web) per allocation | measure; gate behind `!__EMSCRIPTEN__` if it shows (§4.5) |
| `Src/UnUnix.cpp` `appSeconds()` | absolute ns-since-boot → `FTime::GetFloat()` imprecise at long uptimes | subtract a start counter (§7.4) |
| `Src/UnUnix.cpp:387-398` `appGetTime` | `utime()` with an uninitialised buffer **sets** file times (Epic bug, also in the template) | no caller today; use `stat()` if one appears |

### 8.5 Cross-package items discovered from Core (owned by other reports — listed so they are not lost)

- `Engine/Src/UnAudio.cpp:584-611`: `hProcess` is declared only under `#ifdef _WINDOWS` but used after it → Engine compile
  error off Windows (editor XA-encoding path) — template-engine / hp2-core-engine-specifics report.
- Engine/Editor HP2-only `wcs*` / `L"…"` calls (UNICODE retail, §7.11) — per-package reports.
- `FString` through `...` in Engine/Editor/drivers (§8.4 first row) — every package.
- `DOUBLE Now = appSeconds();` in the copied template drivers → `FTime` (§7.4) — template-drivers report.
- `Default.ini` `Paths=../save/*.usa` (lower-case) vs `SavePath=../Save` — web inis (web-layer report); the Core
  case-insensitive lookup (§5.10 #6) papers over it for *reads* only.

## 9. Merge strategy per file + ordered checklist

### 9.1 Strategy legend and the command

- **3WAY** — `git merge-file -p --diff3 -L HP2-433 -L epic-v400 -L engine-ut99 <HP2> <epic-LF> <template>`
  (exactly what `tools/dev/merge3.sh <out> Core` runs; normalise the Epic base to LF first), review the result
  against HP2 with `diff -u`, resolve conflicts as stated, copy over the HP2 file.
- **TAKE-T** — copy the template's file verbatim.
- **T+433** — copy the template's file, then re-apply the listed HP2-433 items.
- **KEEP+HAND** — keep HP2's file; apply the listed template hunks by hand.
- **KEEP** — nothing from the template.

### 9.2 Per-file table

| File | Dry run | Strategy | What to resolve / re-apply | Section |
|---|---|---|---|---|
| `Inc/UnGnuG.h` | CONFLICTS=12 | **T+433** | re-add `FTime appSeconds()/appSecondsSlow()`, `FLOAT GSecondsPerCycle`, `DEFINED_appCycles/appSeconds`; `DEFAULT_ALIGNMENT` stays 8; drop PSX2/`OWORD`/`GCC_MOVE_ALIGN`/`__Context` | §4.2 |
| `Inc/Core.h` | CLEAN | 3WAY | + `SUPPORTS_PRAGMA_PACK` on `__GNUC__`; delete `#if __STATIC_LINK #include "UnCoreNative.h"`; optionally drop `appFindFiles` | §4.1 |
| `Inc/UnFile.h` | CLEAN | 3WAY | keep HP2's `FTime` declarations and 433 `app*` API; the template's inline `appMalloc` needs `GetMallocError` (Core.cpp) | §4.8 |
| `Inc/UnUnix.h` | OURS-UNCHANGED | TAKE-T | — | §4.18 |
| `Inc/FOutputDeviceAnsiError.h` | OURS-UNCHANGED | TAKE-T | — (DC-only hunks, dead) | §4.6 |
| `Inc/FOutputDeviceFile.h` | CONFLICTS=1 | 3WAY | trivial (blank line vs `#include <stdio.h>`); keep HP2's `appUserDir()` log path | §4.7 |
| `Inc/FFileManagerGeneric.h` | CLEAN | 3WAY | + drop the in-class `FFileManagerGeneric::` qualifier; `GetGlobalTime` → 0 on `__EMSCRIPTEN__` | §4.3 |
| `Inc/FFileManagerLinux.h` | CLEAN | 3WAY | — | §4.4 |
| `Inc/FMallocAnsi.h` | CLEAN | 3WAY | (later) perf gate on `Clock(GStat.MemTime)` | §4.5 |
| `Inc/UnCache.h`, `Inc/UnCoreNet.h` | CLEAN | 3WAY | — | §4.12-4.13 |
| `Inc/UnClass.h` | CLEAN | 3WAY | + receive HP2's second `ConstructObject` overload from UnObjBas.h | §4.14 |
| `Inc/UnMem.h` | CLEAN | 3WAY | + delete the `__PSX2_EE__` `Align=16` 3-liner | §4.16 |
| `Inc/UnObjBas.h` | CONFLICTS=3 | 3WAY + manual | 4-arg `DECLARE_CLASS` with `new(X)TClass` (no parens); template's single `IMPLEMENT_CLASS`/`IMPLEMENT_PACKAGE`; delete `__STATIC_LINK` branches + `AUTO_INITIALIZE_REGISTRANTS`; move `ConstructObject` ×2 to UnClass.h | §4.9 |
| `Inc/UnScript.h` | CONFLICTS=6 | 3WAY + manual | template's `IMPLEMENT_FUNCTION` (+`STATIC_EXPORT`) and unconditional `P_GET_SKIP_OFFSET` memcpy; keep HP2's `P_FINISH` `EX_DebugInfo` step and `FFrame::Parent`; delete PSX2 guards | §4.10 |
| `Inc/UnStack.h` | CONFLICTS=1 | 3WAY | HP2's ctor + template's `virtual ~FFrame() {}`; **HP2 opcode numbers** | §4.11 |
| `Inc/UnTemplate.h` | CONFLICTS=9 | 3WAY + manual | template's `this->`/`typename`/`Clamp<INT>`/**`FString::Mid` DWORD fix**; add the 3 `using` declarations; `TTransArray::Insert` → `TArray<T>::Insert` (resolved file exists: `<scratchpad>/tcore-private/syn/UnTemplate.fixed.h`) | §4.17 |
| `Inc/UnMath.h` | CONFLICTS=5 | **KEEP+HAND** | `UBOOL` returns (optional); delete PS2 `FVector` + ASM/ASMLINUX/PSX2 transform branches (hygiene); add `static_assert(sizeof(FVector)==12)` etc. | §4.15 |
| `Src/UnUnix.cpp` | CONFLICTS=5 (62 gcc errors merged) | **T+433** | 3-arg `appCreateProc`/`appGetProcReturnCode` (+ web/Win32 bodies), `FTime appSeconds()/appSecondsSlow()` (+ start-counter base), `appUserDir()`=`""`, `appProcessSeconds()`, `FFileStream` stubs | §5.12, §8.1-8.2 |
| `Src/Core.cpp` | CLEAN | 3WAY | — (`GetMallocError`) | §5.1 |
| `Src/UnAnsi.cpp` | CONFLICTS=1 | 3WAY | `appStrchr` → template's body (drop PSX2); keep `appAcos`/`appFrand(From,To)`/`appRandRange` | §5.2 |
| `Src/UnGUID.cpp` | CLEAN | 3WAY | — (**wild-write fix**) | §5.3 |
| `Src/UnProp.cpp` | CLEAN | 3WAY | + delete the `__PSX2_EE__` struct-offset block (H:1416-1423) | §5.4 |
| `Src/UnName.cpp` | CONFLICTS=1 | 3WAY | keep HP2's hardcoded-name block + duplicate check; forward-declare `AllocateNameEntry` before its first use | §5.5 |
| `Src/UnMath.cpp` | CONFLICTS=2 | KEEP+HAND | nothing from the template; hoist `DWORD i;` in `SetupTable` | §5.6 |
| `Src/UnClass.cpp` | CONFLICTS=3 | 3WAY + manual | layout guard (ENativeConstructor ctor + after H:394), native-bool BitMask restore; delete PSX2 blocks; `wcscmp`→`appStrcmp` | §5.7 |
| `Src/UnCorSc.cpp` | CONFLICTS=4 | 3WAY | `Clamp<INT>` in `execArrayElement` only; everything else HP2's | §5.8 |
| `Src/UnLinker.cpp` (HP2) ← template `Src/UnLinker.h` | no pairing | KEEP+HAND | 4 hunks: OpenFailed names file ×2, `VerifyImport` NULL bail-outs ×2 (`__EMSCRIPTEN__`) | §5.9 |
| `Src/UnMisc.cpp` | CONFLICTS=2 | 3WAY + manual | 9 of 11 template hunks merge clean; `appApplyIniOverrides()` **after** the gk block; HP2 blockers `p`/`j`/`itoa`/`SaveSlotPath` varargs/`default.ini`/`"..\\Save"`/`FFileStream::Destroy` | §5.10 |
| `Src/UnObj.cpp` | CONFLICTS=4 | 3WAY + manual | web NULL-return sites ×6; iterative `FArchiveTagUsed` **with `RefObj`** + two `DrainWorklist()` calls; `ClassName` memtag cast; skip debug prints and DC bits; keep `check(Removed…)` | §5.11 |
| `Inc/UnStaticExports.h`, `Src/UnStaticExports.cpp`, `Src/UnMinGW.cpp` | NO-BASE | already present (byte-identical) | wire the registrants/lookups (UnGnuG.h/UnObjBas.h/UnScript.h/UnUnix.cpp) | §3.1-3.2 |
| `Src/UnFilePool.cpp` | template-only | never copy | — | §3.3 |
| `CMakeLists.txt` | template-only | KEEP (HP2 skeleton) | drop `UnCoreNative.cpp`; keep `-Og` on `UnCorSc.cpp`/`UnProp.cpp` | §3.4 |
| `Inc/AudioMutex.h`, `Src/AudioMutex.cpp`, `Inc/FFileManagerPSX2*.h`, `Src/UnPSX2.cpp`, `Src/VuCoreMath.*`, `Inc/UnMsg.h`, `Inc/afxres.h`, `Inc/FFileManagerArc.h` | HP2-only | KEEP, unbuilt | — | §6 |
| `Inc/UnCoreNative.h`, `Src/UnCoreNative.cpp` | HP2-only | remove from build | superseded by the registry | §6, §7.2 |
| `Inc/UnStat.h`, `Inc/UnLinker.h` | HP2-only | KEEP | — | §6 |
| 33 `THEIRS-UNCHANGED` files (`FMallocWindows.h`, `UnVcWin32.cpp`, `UnBits.cpp`, `FCodec.h`, `UnName.h`, …) | — | KEEP | only the §8 items that live in them | §1.1 |

### 9.3 Ordered checklist (each box = one reviewable commit "Core: …", PLAN §1.6)

- [ ] **1. Platform header.** `Inc/UnGnuG.h` = template + 433 patch (§4.2); `Inc/UnUnix.h` = template. Verify:
      `grep -n "__PSX2_EE__\|OWORD\|__Context\|size_t;" Source/Core/Inc/UnGnuG.h` → nothing.
- [ ] **2. Core.h / UnFile.h / file managers / output devices.** 3WAY Core.h, UnFile.h, FFileManagerGeneric.h,
      FFileManagerLinux.h, FMallocAnsi.h, FOutputDeviceFile.h, FOutputDeviceAnsiError.h, UnCache.h, UnCoreNet.h, UnMem.h;
      apply the HP2-only fixes (`SUPPORTS_PRAGMA_PACK`, `GetGlobalTime` qualifier + web 0, PSX2 `Align`).
- [ ] **3. Object model.** UnObjBas.h (4-arg + a680288 + template `IMPLEMENT_*`; delete `__STATIC_LINK`), UnClass.h
      (+ `ConstructObject` ×2), UnScript.h, UnStack.h. Decide §7.1 (call-site edits vs variadic).
- [ ] **4. Templates.** UnTemplate.h = `syn/UnTemplate.fixed.h` + `TTransArray::Insert` fix. Verify with the header
      syntax check: `g++ -m32 -std=gnu++17 -fsyntax-only -DUNREAL_STATIC -DPLATFORM_LINUX -DPLATFORM_SDL -DCORE_API= -I Source/Core/Inc <t.cpp including Core.h>` → 0 errors (same for
      `clang++ --target=wasm32-unknown-emscripten -fpack-struct=4`).
- [ ] **5. UnMath.h** hygiene (delete PS2/ASM branches) + `static_assert`s.
- [ ] **6. Platform layer.** `Src/UnUnix.cpp` = template + 433 additions (§5.12 table, §8.1 `FFileStream` stubs,
      §8.2 `appUserDir`/`appProcessSeconds`/`appCreateProc` family, `FTime` timing with start counter).
- [ ] **7. Small Src files.** Core.cpp, UnGUID.cpp, UnProp.cpp (3WAY clean), UnAnsi.cpp, UnName.cpp, UnMath.cpp.
- [ ] **8. UnClass.cpp + UnCorSc.cpp** (layout guard, BitMask restore, `wcscmp`, `Clamp<INT>`, PSX2 deletions).
- [ ] **9. UnMisc.cpp** (template hunks + every HP2 blocker/hazard in §5.10).
- [ ] **10. UnObj.cpp + UnLinker.cpp** (web NULL sites, iterative GC with `RefObj`, memtag cast, linker hunks).
- [ ] **11. CMake.** Drop `UnCoreNative.cpp`; confirm `AudioMutex.cpp`/`UnPSX2.cpp`/`UnVcWin32.cpp` stay out.
- [ ] **12. Build gates (Phase A acceptance for Core).** `cmake --preset native && cmake --build --preset native --target Core`
      compiles with zero errors; then `wasm` and `mingw` presets for `Core`. Then, at the first link of `Game`/`UCC`:
      no undefined `FFileStream::*`, `appUserDir`, `appProcessSeconds`, `appCreateProc`, `appGetProcReturnCode`;
      `nm -C <bin> | grep GStaticExport | sort | uniq -d` empty.
- [ ] **13. Run gates.** Native `ucc` boot: log shows `Detected: Linux`, `CPU Timer Freq=`, no `[layout] native …`
      warnings for Core/Engine classes; `obj garbage` / a level change runs `CollectGarbage` without a crash. Web boot
      (`-O0` debug build): no `[appThrowf]` before the menu, no `Maximum call stack size exceeded` on the deepest HP2
      level, second page load keeps `User.ini`/`Game.ini` edits.

## 10. Open questions / how to verify

### 10.1 Corrections to `docs/porting/PLAN.md` §3 guidance (found while checking it against the code)

| PLAN says | Finding | Suggested PLAN edit |
|---|---|---|
| "Add the template's layout guard (`GLayoutStructs` in `UStruct::UStruct(EStaticConstructor…)` …)" | The template records the size in the **`ENativeConstructor`** ctor (T `Src/UnClass.cpp:288-302`). HP2's `EStaticConstructor` twin (H:304-318) is reached only through the `__STATIC_LINK` `IMPLEMENT_CLASS` (`Inc/UnObjBas.h:394`), which is deleted — a guard placed there would record nothing | "…in `UStruct::UStruct(ENativeConstructor…)`" |
| `UnObj.cpp`: "HP2's `FPushMemTag`/`RefObj` extras can go unless something calls `GetReferencing()`" | `IsReferenced(…, IgnoreReference=0)` calls it (`Src/UnObj.cpp:3663`) and `Editor/Src/EdHook.cpp:190` passes 0; `FPushMemTag` is a no-op unless `FMallocDebug` is used | keep `RefObj` (cheap, §5.11 sketch); `FPushMemTag` may stay |
| `UnGnuG.h`: "re-add `GCC_ALIGN`/`GCC_MOVE_ALIGN`, `OWORD`" | `GCC_ALIGN` is already in the template; `GCC_MOVE_ALIGN` and `OWORD` have no user in `Source/` and `OWORD`'s `mode(TI)` does not compile with `-m32` | drop both |
| `UnName.cpp`: "add the template's declaration if it uses it" | HP2's only call is found by ADL (argument `FNameEntry*`); declaration optional | as is, note "optional" |

### 10.2 Open questions

| # | Question | Why it matters | How to verify |
|---|---|---|---|
| Q1 | Does any Engine/Render/Editor code rely on `DEFAULT_ALIGNMENT == 16`? | §4.2 #5 recommends 8 | `grep -rn DEFAULT_ALIGNMENT Source/{Engine,Render,Fire,Editor,SoftDrv}` (2026-09-25: only `Render/Src/UnLight.cpp:2098,2165`, alignment requests); then a lightmapped level renders identically at 8 |
| Q2 | Is `TTransArray<T>::Insert` ever instantiated? | ill-formed `FArray::Insert` call (§4.17) | first Engine+Editor compile; fix regardless |
| Q3 | Which other `FString`-through-`...` sites are on a boot/gameplay path? | clang traps (§8.4) | one web build with `-Wnon-pod-varargs`; list per package |
| Q4 | `appUserDir()` = `""` vs a per-user directory natively? | where inis/log/saves live | decide in the launcher report; Core needs *a* definition. The web requires `System/`-relative (MEMFS/IDB) |
| Q5 | Is `UMasterCommandlet.cpp` (only `FFileManagerArc.h` user) built? | editor build list | template-editor report |
| Q6 | Are there more HP2 `wcs*`/`L"…"` (UNICODE-retail) calls in Engine/Editor? | ANSI port (§7.11) | `grep -rn "wcs[a-z]*(\|L\"" Source/{Engine,Render,Editor,Fire}/Src` |
| Q7 | Does the Linux performance counter start near 0 on the target machines? | `FTime::GetFloat()` precision (§7.4) | log `appSeconds().GetFloat()` at boot; if > 1e4, apply the start-counter base (recommended anyway) |
| Q8 | Is the O(n) `appGetStaticExport` walk a boot-time problem with HP2's native count? | every native bind is a linear `strcmp` scan | time `UObject::StaticInit`→menu natively; UT99 lives with it. A hash map is an optimisation, not a port task |
| Q9 | Does `-fpack-struct=4` + HP2's `PropertiesAlign` agree with every shipped class? | silent member-offset bugs | the layout guard (§5.7) must print **zero** `[layout] native` lines on native (natural layout) and web/MinGW (`-fpack-struct=4`); it is necessary-not-sufficient (tail padding) — for classes with trailing bitfields/bytes also compare per-property offsets (`UProperty::Offset` vs `offsetof`) in a one-off debug build |
| Q10 | Do HP2's `.u` files actually contain `EX_DebugInfo` tokens? | keeps `P_FINISH`'s extra step honest | disassemble a function from `Assets/System/HGame.u` (e.g. with the ufrontend package tooling) or count 0x38 after `EX_EndFunctionParms` in `SerializeExpr` with a debug log |

### 10.3 How the §5-§9 content was derived (re-checkable)

- `diff --strip-trailing-cr -w <epic>/Core/Src/<f> <template>/Core/Src/<f>` for every hunk; HP2 sites located with
  `grep -n` + `sed -n` in `/home/ion/hp2-ufrontend/HarryPotter2/Source/Core` (HEAD at the time of writing: `c73e7e7`,
  Core sources unchanged since `e47a3c7`).
- The "cc probe" numbers come from the previous run's syntax-only compile of the resolved dry-run tree
  (`<scratchpad>/tcore-private/cc/probe.sh`, logs `cc/{gcc,wasm}_*.log`); every error cited was re-mapped to the HP2
  line and re-read in the HP2 source for this report.
- Template rationale: commit bodies (`git -C /home/ion/engines/engine-ut99 show -s <sha>`), quoted comments in the
  template source.
- Nothing under `/home/ion/engines`, `/home/ion/epic-sources`, `/home/ion/ufrontend` was written; no build was run in
  any repo; only this report was written in the HP2 repo.

## Status: complete

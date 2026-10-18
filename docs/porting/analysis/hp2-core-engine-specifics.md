Record — kept as history; the current state is in README.md, BUILD.md and the change inventory.

# HP2 build-433 Core+Engine idioms that are hazards for a GCC / Emscripten / MinGW port

Key: `hp2-core-engine-specifics` · analyst report (re-run) · 2026-09-26

Scope: `Source/Core` and `Source/Engine` of `/home/ion/hp2-ufrontend/HarryPotter2` (HP2 PC, UE1 build 433)
read against the template `/home/ion/engines/engine-ut99` (ut99dc's GCC/Emscripten/MinGW port of UT99 v400)
and the common ancestor `/home/ion/epic-sources/ut99-v400`.

Conventions: unprefixed `Path:line` = HP2 tree (`/home/ion/hp2-ufrontend/HarryPotter2/…`); `ut99:` = template;
`epic:` = v400 reference. Every finding ends in an **ACTION**. Unverified points say so and give the command
to verify. Nothing was built or run (brief forbids cmake/make); "will not compile" is reasoning over quoted code.

(Section bodies are appended as each is researched; a section that still reads "_pending_" was not reached.)

## Contents
- 0. Executive summary
- (a) Platform switch
- (b) `__PSX2_EE__` / VU0 blocks and the FVector layout
- (c) asm inventory
- (d) MSVC-isms and the UNICODE/TCHAR question
- (e) Threading
- (f) Native-function registry and IMPLEMENT_* macros
- (g) Stats and FTime
- (h) FFileManager family and case-insensitive lookup
- (i) Config / ini names
- (j) GLazyLoad and Suppress=
- (k) Package versions
- (l) Everything else that will bite
- Prioritized checklist
- Appendix

## 0. Executive summary

HP2's Core/Engine is v400 plus a Legend/EA/Loki layer that was built two ways only: MSVC 6 UNICODE for Win32, and an ee-gcc
`__STATIC_LINK` build for PS2. Neither of those is the configuration the template runs (GCC/clang, ANSI TCHAR, `UNREAL_STATIC` registry).
The hazards, ranked by how early they stop the port:

| # | Hazard | Where | Stops | Action (section) |
|---|---|---|---|---|
| 1 | HP2's `UnGnuG.h` is the Loki/PS2 header: `#error` without `__LINUX_X86__`, `appRound/appFloor` return undeclared `F`, `jmp_buf` assignment in `__Context`, multi-line asm strings, `OWORD mode(TI)`, `typedef unsigned int size_t`, no `DOUBLE` | `Core/Inc/UnGnuG.h` | every TU | template header + 4 HP2 re-adds (§a.2) |
| 2 | `DLL_EXPORT {…}` spellings in `IMPLEMENT_CLASS/PACKAGE/FUNCTION` need HP2's `DLL_EXPORT extern "C"`; one `GPackage` per package collides in a static binary; no `STATIC_EXPORT` | `UnObjBas.h`, `UnScript.h`, `Core.h:473`, `UnFile.h:206-217` | compile + link + native bind | template macro trio + `THIS_PACKAGE`, keep 4-arg `DECLARE_CLASS`, keep `__STATIC_LINK` off (§f) |
| 3 | 433 rebased `TArray` on template `TRefArray<T>` → ~65 unqualified dependent-base uses | `UnTemplate.h:206-681` | every TU | `using` decls + template's `this->` (§l.2) |
| 4 | UNICODE-only call `wcscmp` on ANSI TCHAR; `__declspec` in a built header; for-scope reuse (1 HP2-only + v400-inherited list; `UnMisc.cpp:2047` `p`) | `UnClass.cpp:1439`, `Palette.h:18`, App. A.1 | compile | §d |
| 5 | Unix core lacks `appSeconds/appSecondsSlow/appProcessSeconds` (FTime), `appUserDir`, `IsOSVer2kOrXP`, and `FFileStream::Read/Create/Destroy` (called by `UnMisc.cpp` itself) | `UnUnix.cpp`, `UnVcWin32.cpp` only | link | "HP2 433 additions" block + `UnFileStream.cpp` with a synchronous pump (§a.3, §e.2, §g, §l.1) |
| 6 | `appInit` opens `default.ini` (lower-case) and writes an uninitialised `DTemp` as `Language` | `UnMisc.cpp:1955-1964` | boot: all text `<?int?>` | §i.2 |
| 7 | `debugf("%s", GSys->SaveSlotPath)` — FString by value through varargs: garbage on gcc, **trap on em++** | `UnMisc.cpp:2030` | web boot | `*` (§i.3) |
| 8 | Content is mixed-case (`Startup.unr` vs `startup.unr`, `potions`/`Potions.utx`, `hp_fx`/`HP_FX.utx`, `adv1Willow.unr`…) and the Unix file layer is exact-case | `appFindPackageFile`, `FFileManagerLinux.h` | map loads | template `appFindFileNoCase` (§h.3) |
| 9 | `FFileStream::Destroy` waits for a thread that does not exist on Unix unless `__UNIX__` is defined (the template never defines it) | `UnMisc.cpp:1629` | exit/restart hang | `#if _MSC_VER` (§e.2) |
| 10 | HP2 made `GetGlobalTime` real (`stat` mtime) and uses it to invalidate persistent-actor caches and regenerate inis | `FFileManagerGeneric.h:129`, `UnLevel.cpp:328,492`, `UnMisc.cpp:1919,1938` | web: state/settings lost each session | override → 0 (§h.2) |
| 11 | `SetString("Core.System","paths",…)` overwrites the **last** `Paths=` line; `SavePath="..\\Save"` | `UnMisc.cpp:2011,2064` | saves / search path | keep `Paths=../Save/*.usa` last; `PATH_SEPARATOR` (§i.3) |
| 12 | `catch(char*)` misses `const TCHAR*` throws → stale `GObjLoaders` | `UnLinker.cpp:298-306`, `UnGame.cpp:961-965` | error recovery | `catch(const TCHAR*)` (§k.3) |
| 13 | GCC-only semantic bugs behind `#if _MSC_VER`: viewport teardown skipped (leaks the render device), screenshot row off-by-one; value-initialising `InternalConstructor` | `UnCamera.cpp:370-398,534-543`, `UnObjBas.h:333` | web resize/relaunch, legacy loads | §d.5, §d.7 |

Answers to the brief's direct questions: **(b)** 32 `__PSX2_EE__` lines in 15 files, 18 PS2 asm blocks (16 VU0 in `UnMath.h`); outside PSX2 `FVector`
is the v400 3-float, 12-byte class (the `W` member exists only in the PS2 arm). **(c)** every Core/Engine asm site has a C fallback except
`appSeconds` on Unix. **(d)** HP2 retail is UNICODE (`/D UNICODE /D _UNICODE`, UTF-16 strings in retail `Core.dll`); under `__GNUG__` without
those defines `TCHAR = char`; `.u`/`.unr` strings are self-describing (±length) and every shipped text file is 8-bit, so ANSI is correct
(Latin-1 SKUs fine, CJK SKUs out of scope) — same as the template. **(e)** `AudioMutex` has zero users and never shipped; the only engine
thread is the Win32 file-streaming thread. **(g)** `FTime` is a 32.32 `long long` fixed-point class; keep it. **(h)** the launcher instantiates
`FFileManagerLinux`; it is exact-case. **(i)** `<appUserDir()><InPackage>.ini` → `Game.ini` + `User.ini`, seeded from `Default.ini`/`DefUser.ini`.
**(k)** `PACKAGE_FILE_VERSION` is a runtime `INT` = 79 (`Core.cpp:13`), `PACKAGE_MIN_VERSION` = 60; shipped packages are v79 (132), v76 (75,
incl. the boot map `Entry.unr`), v69 (1), v68 (2), v61 (2), licensee 0 everywhere.

## (a) The platform switch — `Core.h`, `UnGnuG.h`, `UnUnix.h`, `UnVcWin32.h`, `UnBuild.h`

### a.0 Repo state this report was written against
`git log` of the HP2 repo: `754be39` import → `200241e` LF → `e47a3c7` CMake skeleton + template drivers
copied unadapted + `Core/Src/UnStaticExports.cpp`, `Core/Src/UnMinGW.cpp`, `Core/Inc/UnStaticExports.h`.
Every Core/Engine file quoted below is still the **2002 original** (only line endings changed) except those
three. `Core/Src/UnGUID.cpp` is HP2's own (in the original drop, listed in `Core/Src/makefile:50`).

### a.1 How `Core.h` selects a platform (HP2 vs template)

`Source/Core/Inc/Core.h`:
```c
22  #include "UnBuild.h"
26  class FTime { #if __GNUG__ #define TIMETYP long long #else #define TIMETYP __int64 ... };   // HP2-only, BEFORE the compiler header
55  #if _MSC_VER || __ICC || __LINUX__
56  	#define SUPPORTS_PRAGMA_PACK 1
62  #if _MSC_VER
63  	#include "UnVcWin32.h"
64  #elif __GNUG__
65  	#include <string.h>
66  	#include "UnGnuG.h"
72  #if !ASM && !__GNUG__
73  	#define __asm ERROR_ASM_NOT_ALLOWED
77  #if __UNIX__
78  	#include "UnUnix.h"
79  	#include <signal.h>
473 extern "C" DLL_EXPORT TCHAR GPackage[];
496 #if __STATIC_LINK
497 #include "UnCoreNative.h"
```
Template (`ut99:Source/Core/Inc/Core.h`, diffed against epic): deleted lines 72-80 (no `__asm` poison, no
`__UNIX__`→`UnUnix.h` include — `UnUnix.h` is included only by `UnUnix.cpp`/`UnGUID.cpp`), added a
`PLATFORM_WIN32` branch that typedefs `TCHARU` when MinGW's `<tchar.h>` already set `_TCHAR_DEFINED`,
replaced `GPackage` by `THIS_PACKAGE` (`ut99:Core.h:348-355`, §f), and includes `UnStaticExports.h`.

Diff HP2 `Core.h` vs epic v400 (everything 433 added, all of it portable C++ except where noted):
| HP2 line | Addition | Port impact |
|---|---|---|
| 24-53 | `FTime` fixed-point 32.32 clock class | keep; §g |
| 55-59 | `SUPPORTS_PRAGMA_PACK` | only used by `UnVcWin32.cpp:654` and `ALAudio/Src/ALAudioSubsystem.cpp:15`; harmless |
| 224-226 | `FMalloc::MemSize/SetTag/GetTag` virtuals | portable; `FPushMemTag` (`UnFile.h:505-516`) uses them |
| 340 | `FFileManager::Init(UBOOL Startup)` virtual | portable; the template's `FFileManagerLinux` must gain it if we take the template's header (§h) |
| 344-410 | `EFileStreamType`, `FStream`, `FFileStream` (streaming for Ogg/XA) | half the implementation lives in `UnVcWin32.cpp` only — §e.2 |
| 437-438 | `FLOAT GSecondsPerCycle` (v400: `DOUBLE`), `FTime GTempTime` (v400: `DOUBLE GTempDouble`) | template's `UnGnuG.h:245` declares `DOUBLE GSecondsPerCycle` → conflicting declaration; §g |
| 464-470 | `GCurrentViewport`, `GEncryptLoad`, `GCurrentEncrypt`, `GDebugger`, `GFileStream`, `GAudioMaxRadiusMultiplier`, `GAudioDefaultRadius` | portable globals (defined in `Core.cpp`) |
| 473 | `extern "C" DLL_EXPORT TCHAR GPackage[];` | **with HP2's `UnGnuG.h` (`DLL_EXPORT`=`extern "C"`) this expands to `extern "C" extern "C" char GPackage[]` — legal but only because of the brace-less linkage nesting; with the template's `UnGnuG.h` (`DLL_EXPORT` empty) it is the v400 spelling. Either way one global per package = duplicate symbol in a static binary → §f** |
| 492 | `#include "UnCId.h"` (v400: `UnCid.h`) | HP2's file is `UnCId.h`, matches — fine on a case-sensitive FS |
| 496-498 | `#if __STATIC_LINK #include "UnCoreNative.h"` | keep `__STATIC_LINK` undefined (§f) |
| 501-506 | `class UDebugger` (abstract, `DebugInfo(...)`) | portable; `GDebugger` stays NULL (no debugger DLL) |

**ACTION (a.1):** keep HP2's `Core.h` body; apply the template's three edits (drop lines 72-80, add the
`PLATFORM_WIN32` `TCHARU` typedef, `GPackage`→`THIS_PACKAGE` + `#include "UnStaticExports.h"`). Nothing in
Core/Engine needs `UnUnix.h` via `Core.h` (checked: `stricmp`/`strupr` come from the template `UnGnuG.h`
`#define`s; `appGetLocalIP`/`appGetGUID` are only called from `UnUnix.cpp`/`UnMisc.cpp`… verify with the first
compile — if `UnObj.cpp`'s `appCreateGuid` path needs `appGetGUID`, include `UnUnix.h` there under `PLATFORM_POSIX`).

### a.2 What HP2's `UnGnuG.h` assumes — it is the Loki/PS2 file and has never compiled for x86 Linux as shipped

`Source/Core/Inc/UnGnuG.h` (338 lines) = epic v400's file + PS2 branches + 433 time types. Line by line:

| Line | Code | Problem on gcc-15 `-m32` / emcc (wasm32 clang 23) / i686-w64-mingw32-g++ 13 |
|---|---|---|
| 10-33 | `#ifdef __LINUX_X86__ … #elif __PSX2_EE__ … #else #error Unsupported platform.` | **Nobody defines `__LINUX_X86__`** (it came from Loki's `makefile-header`, not in the drop; `Source/CMakeLists.txt` defines `PLATFORM_POSIX`/`PLATFORM_X86`/`PLATFORM_32BIT`…). First compile error of every TU. |
| 15-18 | `#undef ASM` `#undef ASM3DNOW` `#undef ASMKNI` `#define ASMLINUX 1` | Turns on the AT&T-asm paths in `UnGnuG.h:245-306`, `UnMath.h:1756,1880,1949,2024`, `Engine/Inc/UnRender.h:192`, `Render/Src/UnLight.cpp:905`, `UnRender.cpp:1036` — x86-only, wasm cannot take them (§c). |
| 19, 30 | `#define COMPILER "Compiled with GNU g++ ("__VERSION__")"` | Verified in the scratchpad: gcc-15 → `warning: invalid suffix on literal; C++11 requires a space [-Wliteral-suffix]` (accepts); **em++ → `error: no matching literal operator for call to 'operator""__VERSION__'`** at every use. No HP2 Core/Engine file uses `COMPILER` today, so it is latent. |
| 36-59 | `#include <sys/wait.h>` + `class __Context { static jmp_buf Env; … 9 static struct sigaction …; jmp_buf Last; }` | Consumed by the GCC `guard()` in `UnFile.h:115-123`: every `guard` block constructs a `__Context`, `try{ if(setjmp(__Context::Env)) { throw 1; } else {`. setjmp per guarded function, a `throw` out of a signal handler longjmp — undefined behaviour in modern C++, no meaning on wasm (no signals; `-fignore-exceptions`). `jmp_buf` copy-assignment in `__Context(){ Last = Env; }` (42, also in epic) and `~__Context(){ Env = Last; }` (43, HP2 un-commented it) — verified with gcc-15: **`error: invalid array assignment`** ×2 (g++ 2.9x accepted array assignment as an extension; that is how Loki built it). |
| 79-80 | `#define HANDLE DWORD` / `#define HINSTANCE DWORD` | DWORD-sized handles; template uses `uintptr_t` and skips when `_WINDOWS_` is defined (`ut99:UnGnuG.h:73-76`). On MinGW these macros would clobber `<windows.h>`. |
| 83 | `DEFAULT_ALIGNMENT = 16` (v400: 8) | HP2 changed it (PS2 VU alignment). Users: `UnMem.h:67-117` (`FMemStack` new), `UnCache.h:83,108` (`GCache` default alignment), `UnMath.h:657` `#define FVECTOR_ALIGNMENT DEFAULT_ALIGNMENT`, `Render/Src/UnLight.cpp:2098`. Allocation alignment only, no serialised layout. The template's 8 is fine; 16 costs a little GMem. Either is safe. |
| 85-88 | `GCC_PACK`, `GCC_ALIGN(n)` (active), `GCC_MOVE_ALIGN(n)` (empty) | Checked: `GCC_MOVE_ALIGN` has **zero users** in the whole tree; `GCC_ALIGN` has exactly one, `UnMath.h:649` `} GCC_ALIGN(16);` which closes the **PSX2** `FVector` (inside `#if __PSX2_EE__` 192-652). Once the PSX2 blocks go, neither is needed; the template header defines `GCC_ALIGN` anyway. |
| 96-98 | `#define DLL_EXPORT extern "C"` | Template: `#define DLL_EXPORT` (empty). HP2's `IMPLEMENT_PACKAGE`/`IMPLEMENT_CLASS`/`IMPLEMENT_FUNCTION` (`UnObjBas.h:385,431`, `UnScript.h`) write `DLL_EXPORT` where the template writes `extern "C" {…}` explicitly → must change together (§f). |
| 107-121 | `GET_VARARGS` with `vsprintf` (no length) | same as template (buffer overrun risk is v400's too). |
| 127 | `typedef unsigned long long QWORD` | fine. |
| 128-130 | `typedef unsigned int OWORD __attribute__ ((mode (TI)));` | Verified: gcc `-m32` → **`error: unable to emulate 'TI'`**; i686-w64-mingw32-g++ → same error; em++ accepts. `OWORD` has **no user** in Core/Engine/Render (grep matches only `LOWORD`). Delete. |
| 145-147 | `UBOOL`=`signed int`, `SIZE_T`=`unsigned int` | template: `int32_t`, `SIZE_T`=`size_t`. |
| 152 | `typedef unsigned int size_t;` | Verified: **em++ → `typedef redefinition with different types ('unsigned long' vs 'unsigned int')`** (wasm32 `size_t` is `unsigned long`); gcc -m32 accepts (same type). Delete. |
| (missing) | v400's `typedef double DOUBLE;` was **removed** by HP2 | `UnFile.h:390-399` declare `DOUBLE appExp(DOUBLE)`, `appAcos`, … → undefined type → hard error. The template header has it (`ut99:UnGnuG.h:179`). |
| 155-157 | `#ifdef __CHAR_UNSIGNED__ #error` | fine (wasm32/i386/mingw: char signed); template uses `static_assert((char)-1 < 0)`. |
| 160-168 | `#if __UNIX__` → `"\n"`, `"/"`, `".so"` else `"\r\n"`, `"\\"`, `".dll"` | on MinGW the template deliberately keeps `/` and `\n`. |
| 175-176 | `IMPLEMENT_PACKAGE_PLATFORM(pkgname) BYTE GLoaded##pkgname;` | v400 had `extern "C" {HINSTANCE hInstance;}` too; HP2 removed it. `hInstance` is referenced only by `Engine/Inc/UnEngineWin.h:89,1028` (Win32 launcher header, not compiled) → no link issue. Template: `STATIC_EXPORT(GLoaded##pkgname…)` under `UNREAL_STATIC` (§f). |
| 191 | `#define appAlloca(size) alloca((size+7)&~7)` | needs `<alloca.h>` (glibc/musl) or `<malloc.h>` (MinGW); HP2's header includes neither. Template includes both appropriately. |
| 193-195 | `extern CORE_API UBOOL GTimestamp; extern CORE_API FLOAT GSecondsPerCycle; CORE_API FTime appSecondsSlow();` | **433-specific**; keep the FLOAT/FTime spelling (§g). |
| 201-218 | `inline INT appRound( FLOAT f ) { … #else return (INT)(F); #endif }` | **Parameter is `f`, body uses `F`** → `'F' was not declared in this scope` — hard error in every TU. Even fixed, `(INT)f` truncates (template's comment `ut99:UnGnuG.h:254-264`: this bug caused the software-renderer streaks). |
| 223-240 | `appFloor(FLOAT f)` → `return (INT)F;` | same undeclared `F`; and v400's `(INT)(F-0.5)` was replaced by a truncation — wrong for negatives. Take the template's `floorf`. |
| 204-213, 226-235 | `#if __PSX2_EE__ __asm__ __volatile__(" cvt.w.s %1,%1 … ")` — **multi-line string literals** | removed from GCC in 3.3; inside a skipped `#if` group GCC/clang only warn (`missing terminating " character`), but they must go (the plan: no PSX2 deltas). |
| 245-256 | `#if ASMLINUX inline DWORD appCycles(){ if(GTimestamp){ asm("rdtsc"…); return r; } }` | x86-only; **no return on the `else` path** (UB; `-Wreturn-type`). |
| 261-273 | `#if ASMLINUX inline FTime appSeconds(){ … rdtsc … else return appSecondsSlow(); }` | x86-only; and **when `ASMLINUX` is off, no `appSeconds` is defined anywhere on Unix** (HP2's `UnUnix.cpp` has none; `grep -c appSeconds Source/Core/Src/UnUnix.cpp` = 0 definitions) → link error. |
| 278-306 | `#if ASMLINUX appMemcpy` with a **multi-line `asm volatile("…")` string** and clobbering `%ebx` by push/pop | active on `__LINUX_X86__` → hard lexer error on any modern GCC; x86-only. |
| 311-315 | `appMemzero` → `memset` | fine. |
| 322-331 | `extern "C" { extern HINSTANCE hInstance; … GIsMMX … GTimestamp; }` | template drops `GTimestamp` from this block (it's declared at 244). HP2 `Core.cpp:190` defines them `#if __INTEL__` — **template header defines `__INTEL__` only `#ifndef PLATFORM_BIG_ENDIAN`**, so they stay defined on x86 and wasm (OK). |
| (missing) | `PATH_MAX`, `<stdint.h>`, `<stdarg.h>`, `<stdio.h>`, `<ctype.h>`, `<dirent.h>`, `<limits.h>` | template includes them (`ut99:UnGnuG.h:20-46`); HP2's `UnUnix.cpp`/`UnMisc.cpp` rely on transitive includes that musl does not give. |
| (missing) | `strupr`/`stricmp`/`strnicmp` | template defines them inline/as macros (`ut99:UnGnuG.h:321-337`); HP2 declares them in `UnUnix.h:17-19` and defines them in `UnUnix.cpp`. |
| 92-93 | `DISABLE_OPTIMIZATION`/`ENABLE_OPTIMIZATION` are empty in HP2 | Users: `Engine/Src/UnPhysic.cpp:1421/1513`, `UnAudio.cpp:22` (no matching ENABLE), `UnScript.cpp:1235/1291`, all spelled **`#pragma DISABLE_OPTIMIZATION`** — GCC does not macro-expand an unknown pragma, so both the HP2 (empty) and template (`_Pragma("GCC push_options")…`) definitions end up as an ignored unknown pragma (`-Wunknown-pragmas`). The template has the identical sites (`ut99:UnPhysic.cpp:1129/1218`, `UnAudio.cpp:16`, `UnScript.cpp:827/881`). Harmless; same as template. |
| (missing) | `GCC_HIDDEN`, `GCC_USED`, `STATIC_EXPORT` plumbing, `UNREAL_STATIC` `DLL_IMPORT` override | template-only; needed for the static registry (§f). |

**Verdict:** HP2's `UnGnuG.h` cannot be salvaged by `#define __LINUX_X86__`; it has at least four hard
errors on every compiler (undeclared `F`, `jmp_buf` assignment, multi-line asm string, missing `DOUBLE`), two
more on gcc-m32/MinGW (`mode(TI)`), and one more on wasm (`size_t`).

**ACTION (a.2):** replace `Source/Core/Inc/UnGnuG.h` with `ut99:Source/Core/Inc/UnGnuG.h` verbatim, then re-add
exactly these HP2 items:
1. (Optional) `#define GCC_MOVE_ALIGN(n)` (empty) next to `GCC_ALIGN` — PLAN.md lists it, but it has no user (checked), so skip it unless a later merge brings one.
2. `extern CORE_API FLOAT GSecondsPerCycle;` and `CORE_API FTime appSecondsSlow();` / `CORE_API FTime appSeconds();`
   in place of the template's `DOUBLE` versions (§g) — `FTime` is already defined by `Core.h:26` before this
   header is included, so it is visible.
3. `#define __UNIX__ 1` and `#define __LINUX__ 1` under `#ifdef PLATFORM_POSIX` (NOT on MinGW). Reasons:
   `Core.h:55` (`SUPPORTS_PRAGMA_PACK`), `UnMisc.cpp:1629` (`#if !__UNIX__` wait loop, §e.2),
   HP2 `UnUnix.cpp`'s own `#if` sites if any of it is kept. Alternatively rekey those three sites onto
   `PLATFORM_POSIX` and define nothing — the template never defines `__UNIX__`; pick one and document it.
4. `#undef ASMKNI` / `#define ASMKNI 0` beside the template's `ASM`/`ASMLINUX`/`ASM3DNOW` block — HP2's
   `UnBuild.h:70-71` defaults `ASMKNI 1` and the template header forgets it (only `UnVcWin32.h` consumes it
   today, so this is hygiene, not a blocker).
5. Do **not** re-add `__Context`, `OWORD`, `size_t`, `COMPILER`-with-`__VERSION__`, PSX2 branches.
Then replace `UnFile.h:108-124` (the `#if _MSC_VER … #else __Context … setjmp` guard pair) with the template's
single `try{ … }catch(TCHAR*Err){throw Err;}catch(...){appUnwindf(…); throw;}` form (`ut99:UnFile.h:101-109`).

### a.3 `UnUnix.h` / `UnUnix.cpp` — HP2's Unix core is the Loki 436-era file

`UnUnix.h` is **byte-identical to epic v400's** (`diff` clean); the template's adds `<cstring>` and
`appWinSockInit` for MinGW. `UnUnix.cpp` (897 lines) = epic v400 (757) + the later Loki/UT436 changes
("Obfuscated by Daniel Vogel"): `appCreateProc` returns `void*` via `fork(); _exit(system(...))` (322-336),
`appGetProcReturnCode` + `SIGCHLD` handler + `TMap<pid_t,int>* ExitCodeMap` (316-357, 658-676), the nine-signal
`__Context::StaticInit` (767-885, `SA_ONESHOT`), clipboard stubbed (568-581), `appSleep` via `appRound`.
Things that matter for us:

| `UnUnix.cpp` | Code | Hazard |
|---|---|---|
| 213-217 | `#if __STATIC_LINK INT* FalseHandle; return (void*) FalseHandle; #endif` in `appGetDllHandle` | returns an **uninitialised pointer** as the "handle" in the Loki/PS2 static build. Irrelevant once the template's registry lookup is used (§f). |
| 306-308 | `#ifdef ASMLINUX asm("int $03"); #endif` in `appDebugBreak` | **`#ifdef`, not `#if`** — the template header `#define ASMLINUX 0`, so this x86 `int3` is compiled in → em++ error (`invalid instruction` / unknown asm for wasm). |
| 316, 351-357, 658-676 | `SIGCHLD` handler writing a `TMap` from signal context | not async-signal-safe; pointless on wasm; `appCreateProc` is called by `Engine/Src/UnAudio.cpp` and `AStatLog.cpp` — check (§l). |
| 403-420 | `FTime appSecondsSlow()` via `gettimeofday` | fine, keep semantics. |
| 660-669 | `GSecondsPerCycle = 1.0 / CyclesPerSecond` measured with rdtsc/`appSeconds` busy-wait | template measures with `SDL_GetPerformanceFrequency()` (`ut99:UnUnix.cpp:808`). |
| (absent) | `appSeconds`, `appUserDir`, `appProcessSeconds`, `IsOSVer2kOrXP`, `FFileStream::{Read,Create(×2),Destroy(INT)}`, `appMsgf`, `appGetLastError`, `appDebugMessagef` | defined only in `UnVcWin32.cpp` (430, 1048, 501, 1211, 706-935, 378, 387, 368). Callers in built packages: `appUserDir` ← `UnMisc.cpp:1915,1935,2002,2005`, `FOutputDeviceFile.h:42`, `UnCamera.cpp:665,899`, `UnReplay.cpp:40`; `appProcessSeconds` ← `UnGame.cpp:843,1465`, `Render/Src/UnRender.cpp:703`; `IsOSVer2kOrXP` ← `Engine/Src/UnScript.cpp:2841-2849` (native 327 `AActor::execIsOSVer2kOrXP`, script `Engine/Classes/Actor.uc:1664`, no `.uc` caller in the drop); `appSeconds` ← ~20 files. `appMsgf`/`appGetLastError`/`appDebugMessagef` have no caller outside `UnFile.h`. |

**ACTION (a.3):** take the template's `UnUnix.cpp` (as `PLAN.md` says) and add, in a clearly marked
"HP2 433 additions" block: `FTime appSeconds()` (SDL counter × `GSecondsPerCycle`, return `FTime(double)`),
`FTime appSecondsSlow()`, `FTime appProcessSeconds()` (`clock()/CLOCKS_PER_SEC` or `getrusage`; on wasm the
same as `appSeconds`), `const TCHAR* appUserDir()` returning `TEXT("")` (keeps saves under `../Save`, §i),
`CORE_API bool IsOSVer2kOrXP(){ return true; }` (the script uses it to pick OS-specific behaviour; `true` = the
"modern Windows" path — verify what HGame does with it: `strings Assets/System/HGame.u | grep -i OSVer`),
`appCreateProc`/`appGetProcReturnCode` with HP2's `void*` signature (the template's are `void`-returning —
`UnFile.h:82-83`), and the `FFileStream` back end (§e.2). Keep `appGetDllHandle` = the template's registry lookup.

### a.4 `UnVcWin32.h` — MSVC only; not what MinGW compiles
`Core.h:62` sends `_MSC_VER` to `UnVcWin32.h` and every GCC (including i686-w64-mingw32) to `UnGnuG.h`, same
as the template. HP2's `UnVcWin32.h` differs from v400 only by `#define inline __forceinline` (52), extra
`#pragma warning` disables, `ANSI_TO_TCHAR` alloca size ×`sizeof(TCHAR)` (176, a UNICODE-build bug fix),
`#define GCC_ALIGN(n)` (181), unconditional `rdtsc` in `appCycles` (243), and the `FLOAT GSecondsPerCycle` /
`FTime appSeconds()` switch (262-278). Nothing here is compiled by our three toolchains.
**ACTION (a.4):** none for Linux/web. For MinGW, the template's rule holds: `PLATFORM_WIN32` inside `UnGnuG.h`,
`UnMinGW.cpp` for real Win32 calls; never include `UnVcWin32.h`.

### a.5 `UnBuild.h`
HP2 vs v400: the `_DEBUG` `#error` is commented out (6-24), `DO_GUARD_SLOW` = 1 under `_DEBUG` (42-47),
`ASM3DNOW` default 0 (v400: 1), new `HPDEMO 0` (80-83). Defaults that bite: `ASM 1` and `ASMKNI 1` unless
overridden. The HP2 CMake passes `-DASM=0` **only in the `if(EMSCRIPTEN)` block** (`Source/CMakeLists.txt:71`);
native relies on `UnGnuG.h`'s `#undef ASM`/`#define ASM 0` — correct with the template header.
`STATS 1` stays (§g). `HPDEMO` gates one block, `Engine/Src/UnGame.cpp:913` (demo build: refuses maps other than `DEMO_PC/ENTRY/STARTUP.UNR`) — leave it 0.
**ACTION (a.5):** keep HP2's `UnBuild.h` (its commented-out `_DEBUG` error is friendlier for `-DCMAKE_BUILD_TYPE=Debug`
on GCC, which does not define `_DEBUG` anyway). Add `-DASM=0` to the native/MinGW branches too, for symmetry,
or rely on the header — but not both inconsistently.

### a.6 What a modern x86-32 GCC / wasm32 clang / MinGW actually need (summary table)
| Need | gcc-15 -m32 (native) | em++ (wasm32) | i686-w64-mingw32 13 | Where satisfied |
|---|---|---|---|---|
| no x86 asm | optional (x86 asm would work) | **mandatory** | optional | template `UnGnuG.h` `ASM 0`/`ASMLINUX 0`; §c inventory |
| `size_t`/`SIZE_T` | `unsigned int` | **`unsigned long`** | `unsigned int` | template `SIZE_T = size_t`; delete HP2's `typedef unsigned int size_t` |
| 64-bit alignment | `double`/`long long` 4-aligned in structs (i386 SysV) | **8-aligned** | **8-aligned** (MS ABI) | `-fpack-struct=4` on web + MinGW (HP2 CMake lines 79, 170) — `FTime` (8-byte `long long`) now appears in reflected structs? (§g: check) |
| bitfield layout | GCC | GCC | MS (`-mms-bitfields`) | template's `ABrush::bColoredPad` lesson; layout guard |
| `wchar_t` | 4 bytes | 4 bytes | **2 bytes** | irrelevant if TCHAR stays `char` (§d.6) |
| signals / setjmp in guard | works but UB-ish | **no signals**, exceptions ignored | SEH, not POSIX signals | drop `__Context` (a.2) |
| `alloca` header | `<alloca.h>` | `<alloca.h>` | `<malloc.h>` | template includes |
| `PATH_MAX` | from `<limits.h>` | **not transitively** | `MAX_PATH`/`PATH_MAX` 260 | template `#ifndef PATH_MAX 4096` |
| threads | pthreads | `-pthread` + SharedArrayBuffer (template compiles with `-pthread`) | win32/pthreads | §e |

## (b) `__PSX2_EE__` / VU0 blocks and the FVector layout

### b.1 Count
`grep -rc __PSX2_EE__` over `Source/**/*.{h,cpp}`: **32 lines in 15 files** (plus the never-built
`Core/Src/UnPSX2.cpp`, `Core/Inc/FFileManagerPSX2.h`, `FFileManagerPSX2CD.h`, and the VU0 microcode
`Core/Src/VuCoreMath.dsm/.vsm`). By package: **Core 22**, **Engine 2**, Render 7, UCC 1.
Inline asm that only exists for PS2: **18 blocks** — 16 VU0 (`lqc2/vmul/vadd/sqc2`, all in `UnMath.h`) + 2 MIPS
FPU `cvt.w.s` (`UnGnuG.h:204-213,226-235`). Every one is a **multi-line string literal** (`asm volatile ("⏎`),
which modern GCC/clang cannot lex; inside a *skipped* `#if` they only produce `missing terminating " character`
warnings, so leaving them in place is not a compile error — deleting them is still the plan (no PSX2 deltas).

| File:line | What the PSX2 branch does | Non-PSX2 path (what we compile) | ACTION |
|---|---|---|---|
| `Core/Inc/UnGnuG.h:20-30` | PS2 platform defines, `ASMPSX2` | — | file replaced (§a.2) |
| `Core/Inc/UnGnuG.h:204-213, 226-235` | `appRound`/`appFloor` via `cvt.w.s` asm | `(INT)(F)` — broken (§a.2) | template's `floorf` versions |
| `Core/Inc/UnMath.h:192-652` | **the 16-byte `FVector {X,Y,Z,W}`**, `FVECTOR_ALIGNMENT 16`, 12 VU0 asm operators (lines 212, 228, 247, 262, 277, 293, 308, 340, 354, 368, 383, 397), `} GCC_ALIGN(16);` (649) | `#else` at 653: `#define FVECTOR_ALIGNMENT DEFAULT_ALIGNMENT` + the 3-float `FVector` (658-925) | delete 192-656 and the `#endif` at 925 |
| `Core/Inc/UnMath.h:1953-2007` | `TransformPointBy` via VU0 (asm at 1956, 1968) | `#else` 2008-2010: plain C `Temp | Coords.XAxis …` | delete the `#elif __PSX2_EE__` arm |
| `Core/Inc/UnMath.h:2028-2076` | `TransformVectorBy` via VU0 (asm at 2031, 2041) | `#else` 2077-2078: plain C | delete the arm |
| `Core/Inc/UnMem.h:35-37` | `Align = Max(Align,16)` in `FMemStack::PushBytes` | none needed | delete |
| `Core/Inc/UnScript.h:64-68` | `P_GET_SKIP_OFFSET` reads `_WORD` via `appMemcpy` | `var=*(_WORD*)Stack.Code` (unaligned load) | keep the non-PSX2 read — x86 and **wasm both allow unaligned loads** (wasm alignment is a hint). The template's Dreamcast `appMemcpy` variant is not needed. |
| `Core/Inc/UnScript.h:128,139,150,161,174` | `FFrame::ReadInt/ReadObject/ReadFloat/ReadName/ReadWord` via `appMemcpy` | `*(INT*)Code` etc. | same — keep plain reads; `-fno-strict-aliasing` is already on (`CMakeLists.txt:213`) |
| `Core/Src/UnAnsi.cpp:75` | `appGetGMTRef` body emptied on PS2 | `gmtime/mktime` — portable | delete the empty arm |
| `Core/Src/UnAnsi.cpp:327-334` | `appStrchr` via `FString::Printf` | `#elif UNICODE wcschr #else strchr` | delete; see §d.6 on the UNICODE arms |
| `Core/Src/UnClass.cpp:383-391, 408-416` | `PropertiesAlign = 16` for Vector/Plane/Coords struct members | nothing (`PropertiesAlign` stays 4) | delete — **must** stay deleted: a 16-alignment would shift every reflected offset vs `Core.u`/`Engine.u` |
| `Core/Src/UnClass.cpp:421-…` | `PropertiesSize += sizeof(FLOAT)` for `Vector`, Plane→32 | nothing | delete |
| `Core/Src/UnProp.cpp:1416-1424` | `UStructProperty::Link` aligns Vector/Plane/Coords to 16 | `Align(…, ElementSize==2?2:ElementSize>=4?4:1)` | delete |
| `Core/Src/UnObj.cpp:3531-3540` | `FName` unaligned copy workaround | `Name.ClearFlags(RF_Unreachable)` | delete |
| `Core/Src/UnCorSc.cpp:427-432` | `UObject::execBoolVariable` reads the inline `UBoolProperty*` via memcpy | `*(UBoolProperty**)Stack.Code` | keep plain read |
| `Core/Src/UnGUID.cpp:174-175` | empty `get_system_time` on PS2 | `gettimeofday` | delete |
| `Engine/Src/UnAudio.cpp:794-…` (`USound` WAV reduce/downsample helper) | RIFF chunk walk via `appMemcpy` into an aligned `FRiffChunkOld` | direct pointer walk | keep plain (x86/wasm unaligned OK); `mmioFOURCC` is defined portably in `Engine/Inc/UnAudio.h:247` |
| `Engine/Src/ULodMesh.cpp:233-237` | `appRound(Frame-0.5f)` "PSX2 appFloor bug workaround" | `appFloor(Frame)` | keep `appFloor` — **correct only after the template's `floorf` appFloor lands** (HP2's GCC `appFloor` truncates) |
| `Render/Src/UnRender.cpp:68-76` | `check(sizeof(FVector)==16)`, `FCoords==64` | `check(sizeof(FVector)==12)` … | keep the non-PSX2 checks — they are a free layout assertion |
| `Render/Src/UnRender.cpp:249, 605` | stats with `BigFont`, `CurY-40` | `SmallFont` | delete |
| `Render/Src/UnRender.cpp:871-…` | a different `Pipe()` | the normal `Pipe()` | delete (Render owner) |
| `Render/Src/UnRender.cpp:1644-…` | VU0 transform asm | C | delete (Render owner) |
| `Render/Src/UnLight.cpp:546, 582` | `#if !__PSX2_EE__` specular — **negated** guard | the specular code is **inside** the `!PSX2` arm | when deleting, keep the *body* (do not delete the whole `#if !…` block) |
| `UCC/Src/UCC.cpp:78` | `FFileManagerPSX2` | `#elif __LINUX_X86__ FFileManagerLinux` — **never true with the template header** → falls to `FFileManagerAnsi` | UCC is replaced by the template's (PLAN B.5), which keys on `__LINUX__`/`PLATFORM_POSIX` |

### b.2 FVector layout — confirmed 3 floats (v400 layout) outside PSX2
`UnMath.h:658-663`:
```c
#define FVECTOR_ALIGNMENT DEFAULT_ALIGNMENT
class CORE_API FVector
{
public:
	// Variables.
	FLOAT X,Y,Z;
```
Diffing this `#else` class against epic v400's `FVector`: identical data members; HP2 only adds non-virtual
methods (`explicit FVector(FLOAT)`, `DivSize`, `MaxVal`, `FastNormalize`, `SizeApprox`, `DivSizeApprox`,
`Min/Max/SetMin/SetMax/Interp` friends) and `1.0`→`1.f` literal tweaks. `FPlane : public FVector { FLOAT W; }`
(`UnMath.h:982-986`) = 16 bytes. HP2's own assertion `Render/Src/UnRender.cpp:73` `check(sizeof(FVector)==12)`
confirms it at run time. `FVECTOR_ALIGNMENT` is `DEFAULT_ALIGNMENT` (16 in HP2, §a.2) and is only an allocation
hint — nothing serialised depends on it.

The **W member exists only in the PSX2 FVector**; the script struct `Object.Vector` in `Core.u` is 12 bytes and
the PSX2 build papered over the mismatch with the `UnClass.cpp:421`/`UnProp.cpp:1416` hacks. With the PSX2
blocks deleted, C++ and script agree again.

**ACTION (b):** in the Core conflict-resolution commit, delete all 22 Core `__PSX2_EE__` arms (taking the
non-PSX2 arm), the 2 Engine arms (keep `appFloor` in `ULodMesh.cpp`), and hand the 7 Render ones to the Render
owner with the `UnLight.cpp` negated-guard warning. Verify with `grep -rn __PSX2_EE__ Source/{Core,Engine,Render}` → 0.
Keep `UnPSX2.cpp`, `FFileManagerPSX2*.h`, `VuCoreMath.*` on disk, unbuilt (fidelity), as PLAN.md says.

## (c) asm inventory — `__asm` / `asm(` / `#if ASM` / `ASMKNI` / `ASM3DNOW` / `ASMLINUX` in Core + Engine

Method: `grep -rn '#\s*if\s*ASM\b|ASM3DNOW|ASMKNI|ASMLINUX|__asm\b|_asm\b|__asm__|asm volatile|\basm\s*('` over
`Source/{Core,Engine}/{Inc,Src}` minus `UnVcWin32.cpp`/`UnPSX2.cpp` (never compiled by GCC), then each site read.
With the template `UnGnuG.h` (`ASM 0`, `ASMLINUX 0`, `ASM3DNOW 0`) and ASMKNI forced 0 (§a.2 item 4):

| # | Site | Guard | What | C fallback? | With template header |
|---|---|---|---|---|---|
| 1 | `Core/Inc/UnGnuG.h:204-213, 226-235` | `__PSX2_EE__` | MIPS `cvt.w.s` round/floor | `(INT)(F)` — **broken** (undeclared `F`, truncation) | file replaced; `floorf` versions |
| 2 | `Core/Inc/UnGnuG.h:245-256` | `ASMLINUX` | `appCycles` = `rdtsc` (no return on `!GTimestamp`) | `UnUnix.cpp:424` `#if !DEFINED_appCycles` | gone; template `UnUnix.cpp:912` SDL counter |
| 3 | `Core/Inc/UnGnuG.h:261-273` | `ASMLINUX` | `appSeconds` = `rdtsc × GSecondsPerCycle` | **none on Unix** (`UnFile.h:256-257` only declares `FTime appSeconds()`) | template defines `DOUBLE appSeconds()` out of line → HP2 must return `FTime` (§g) |
| 4 | `Core/Inc/UnGnuG.h:278-306` | `ASMLINUX` | `appMemcpy` `rep movsl/movsb` (multi-line string) | `UnAnsi.cpp:232` / `UnFile.h:444` generic | template: inline `memcpy` |
| 5 | `Core/Src/UnUnix.cpp:306-308` | **`#ifdef ASMLINUX`** | `asm("int $03")` in `appDebugBreak` | no-op otherwise | **still compiled** if HP2's file were kept (template defines `ASMLINUX 0`) — moot, file replaced by template's (`ut99:UnUnix.cpp:387`) |
| 6 | `Core/Inc/UnMath.h:1543-1585` | `#if 0` | MSVC `POWER_ASM`, `SqrtApprox` `__asm{}` | `#else` 1586-1611: `SqrtApprox`, `Power`, `DivSqrtApprox`, `DivApprox` in C with `(DWORD&)F` type-punning | always C (needs `-fno-strict-aliasing`, already on) |
| 7 | `Core/Inc/UnMath.h:1679-1755` / `1756-1820` | `ASM` / `ASMLINUX` | `ASMTransformPoint` (MSVC Abrash / AT&T) | yes — `TransformPointBy` `#else` 2008-2010 | C |
| 8 | `Core/Inc/UnMath.h:1822-1878` / `1880-1937` | `ASM` / `ASMLINUX` | `ASMTransformVector` | yes — `TransformVectorBy` `#else` 2077-2078 | C |
| 9 | `Core/Inc/UnMath.h:1945-2011`, `2020-2079` | `ASM`/`ASMLINUX`/`__PSX2_EE__`/else | callers of 7/8 + VU0 (§b) | yes | C. Note the `ASMLINUX` arm of `TransformPointBy` returns a **`static FVector Temp`** (1950) — not thread/reentrancy-safe; irrelevant once off |
| 10 | `Core/Inc/UnMath.h:212-397` | `__PSX2_EE__` | 12 VU0 FVector operators | yes (3-float class) | §b |
| 11 | `Core/Inc/UnVcWin32.h:207-345` | `ASM`, `ASM3DNOW` | MSVC `fistp`, `rdtsc`, `rep movsd`, `femms` | yes (`#else`) | MSVC only, never included by GCC |
| 12 | `Core/Inc/UnVcWn32SSE.h:58-270` | (macros) | Katmai SSE `_asm _emit` byte macros | — | **included by nothing** (grep) — dead file |
| 13 | `Engine/Inc/UnRender.h:129-191` / `192-252` | `ASM` / `ASMLINUX` | `FOutVector::ComputeOutcode` clip-outcode asm (2 multi-line `asm volatile` at 194, 229) | yes — `#else` 253-262 | C |
| 14 | `Engine/Src/UnMesh.cpp:124-486` (asm at 151, 282) and `512-517` | `ASM3DNOW` | `AMD3DGetFrame` K6 path, `#pragma pack(8)`…`#pragma pack()` around it | yes — `GetFrame` falls through to C | C (3DNow off) |
| 15 | `Engine/Src/Amd3d.h` (included by `UnMesh.cpp:12`, `Render/Inc/Render.h:21`) | (macros) | 3DNow `_asm _emit` macros | — | **identical to the template's**, which compiles it (macro bodies never expanded) |
| 16 | `Core/Inc/Core.h:72-73` | `!ASM && !__GNUG__` | poisons `__asm` | — | never true on GCC; template deleted it |

Not found in Core/Engine (checked): SSE/MMX intrinsics (`xmmintrin`, `_mm_*`, `__m64/__m128`), `_rotl/_rotr`,
`_control87/_controlfp`, `_finite`, `rdtsc` outside #2/#3. `_isnan` appears once (`UnAnsi.cpp:140-146`) under
`#if _MSC_VER` with an `isnan` `#else` — same as template.
`ASMKNI`: defined in `UnBuild.h:70-71`, consumed by nothing in Core/Engine/Render (only `UnVcWin32.h`, which
does not even test it) — dead switch.

**Every asm site in Core+Engine has a C fallback except `appSeconds` (#3).** Render has more (`UnLight.cpp:905`,
`UnRender.cpp:1036,1148,1627` — `ASMLINUX`/`ASM3DNOW`, all with C paths per the Render report); SoftDrv's
`DrawSurf.cpp:37` has an **unpaired `#pragma pack(8)`** (SoftDrv owner: with `-fpack-struct=4` it re-widens
the packing of every struct declared after line 37 in that TU; harmless only if none is shared with other TUs).

**ACTION (c):** with the template header nothing asm-related remains active in Core/Engine. Delete the
`ASMLINUX` arms in `UnMath.h` (7, 8, 9) and `UnRender.h` (13) in the same commit as the PSX2 arms — they
contain multi-line string literals that only survive because the group is skipped, and nobody will ever
turn `ASMLINUX` back on. Keep the MSVC `#if ASM` arms (MSVC build fidelity). Provide `FTime appSeconds()`
in `UnUnix.cpp` (§g). Grep gate for the web build: `grep -rnE 'asm *(\(|volatile|__volatile__)' Source/{Core,Engine}` must
only hit MSVC-guarded or deleted code.

## (d) MSVC-isms GCC/clang reject, and the UNICODE / TCHAR question

### d.1 Token pasting in `UnCoreNative.h`
`Source/Core/Inc/UnCoreNative.h:12-19`:
```cpp
#define MAP_NATIVE(cls,name) \
	{"int"#cls#name, &cls::name},

#define DECLARE_NATIVE_TYPE(pkg,type) \
	typedef void (##type::*##type##Native)( FFrame& TheStack, RESULT_DECL ); \
	struct type##NativeInfo \
	{ TCHAR* Name; type##Native Pointer; }; \
	extern void* Find##pkg##type##Native( char* NativeName );
```
`(##type` pastes `(` with `UObject` → GCC/clang: `pasting "(" and "UObject" does not give a valid preprocessing
token` (MSVC 6 silently dropped the stray `##`). Standard spelling: `typedef void (type::*type##Native)( … );`.
`MAP_NATIVE`'s `"int"#cls#name` → `"int" "UObject" "execFoo"` (adjacent literal concatenation) is fine; note its
`TCHAR* Name` initialised from a narrow literal only works because TCHAR=char (§d.6).
**Reach:** `UnCoreNative.h` is included only from `Core.h:496-498` under `#if __STATIC_LINK`; `UnEngineNative.h`
only from `UnEngineNative.cpp:11-12` under the same guard (and the `UnCoreNative.cpp`/`UnEngineNative.cpp` bodies are
`#if __STATIC_LINK` too — with it off both compile to empty TUs).
**ACTION (d.1):** keep `__STATIC_LINK` undefined (§f) so the macro is never expanded; fix the paste anyway (one
line) so a future grep-and-enable does not trip on it.

### d.2 `__declspec`, `__forceinline`, `__int64`, `_alloca`, `#pragma pack`, `#pragma warning`
Method: `grep -rnE` over `Source/{Core,Engine}/{Inc,Src}` excluding the never-GCC files (`UnVcWin32.*`, `UnPSX2.cpp`,
`UnVcWn32SSE.h`, `UnEngineWin.h`, `winstuff.cpp`, `UnDDraw.h`).

| Idiom | Sites in GCC-compiled Core/Engine | Effect | ACTION |
|---|---|---|---|
| `__declspec` | **`Engine/Inc/Palette.h:18`** `__declspec( dllexport ) int HeckbertQuantize(void *srcdata, …)`; included by `Engine/Src/palette.cpp:21` (in `Engine/CMakeLists.txt`) and `UnTex.cpp:14` (under `#ifdef WIN32`) | Linux gcc / emcc: `__declspec` is not a keyword → **hard error**. MinGW accepts it. | wrap in `#if _MSC_VER`, or `#define __declspec(x)` in the template `UnGnuG.h` for non-Win32 (the template has no such define — grep clean) |
| `__forceinline` | none (only `UnVcWin32.h:52`) | — | none |
| `__int64` | `Core/Inc/Core.h:31` (the `!__GNUG__` arm of `TIMETYP` — fine); `Core/Inc/FMallocWindows.h:27` (header not included by any GCC TU — the launcher uses `FMallocAnsi`) | — | none |
| `_alloca` | `Core/Inc/FMallocDebug.h:238` (`FMallocDebug` not used by the template launcher) | — | none; or `#define _alloca alloca` in `UnGnuG.h` if FMallocDebug is ever wanted |
| `_vsnprintf`, `_isnan` | `UnAnsi.cpp:300` / `143` — both inside `#if _MSC_VER` | — | none |
| `#pragma pack(push,1)` | `Engine/Src/UnTex.cpp:265-276, 280-297` (BMP *import* headers, no `GCC_PACK`) | GCC honours `#pragma pack(push,1)` (it is supported since GCC 4 for all targets incl. wasm), so the structs are 14/40 bytes as intended. (The old worry "GCC ignores it" does not hold for modern GCC/clang.) | none |
| `#pragma pack(push,1)` + `GCC_PACK(1)` per member | `Engine/Src/UnCamera.cpp:483-508, 825-850, 915-940` (screenshot BMP headers) | double-packed; correct | none |
| `#pragma pack(push,4)` | `Engine/Inc/EngineClasses.h:7 / 3954` (inside `#if _MSC_VER`) | GCC arm relies on flags: native i386 SysV ABI aligns `double`/`long long` to 4 in structs anyway; web + MinGW get `-fpack-struct=4` (`Source/CMakeLists.txt:79,170`) — same as the template | none; keep the layout guard (VERIFY_CLASS_SIZE) the template uses |
| `#pragma pack( 8 )` / `#pragma pack()` | `Engine/Src/UnMesh.cpp:126/485` inside `#if ASM3DNOW` | skipped | none |
| `#pragma warning(...)` | `palette.cpp:23`, `UnMesh.cpp:127/484` | `-Wunknown-pragmas` noise | none |
| `#pragma DISABLE_OPTIMIZATION` | `UnPhysic.cpp:1421/1513`, `UnAudio.cpp:22`, `UnScript.cpp:1235/1291` | ignored unknown pragma (same as template, §a.2 table) | none |
| `__cdecl`/`__stdcall` | only `UnGnuG.h:104 #define __cdecl` | goes with the header | none |

### d.3 `for` scoping and non-POD varargs

**`for` scoping.** A heuristic scan (a for-init declaration whose name is used after the loop at the same block depth
with no intervening declaration; `forscope.py` + `forscope3.py` filter in the session scratchpad) over GCC-compiled
Core+Engine gives **66 candidate sites in 33 files** (full list: Appendix A.6). **Every one of the 33 files also exists in the
template**, i.e. these are v400-inherited MSVC-6 `for`-scope reuses that ut99dc already fixed in its copy (example
`Core/Src/UnMath.cpp:23` `for( DWORD i=0; …)` then `:33` `for( i=0; …)`; template `ut99:Core/Src/UnMath.cpp` declares `i`
before the loops) — **except `Core/Src/UnLinker.cpp:413→418`** (UnLinker.cpp is the 433 split of UnObj.cpp; not in
the template). The HP2-only files named in the brief (`UnSkeletalMesh.cpp`, `UnParticle*.cpp`, `UnGesture.cpp`,
`UnReplay.cpp`, `UnCoreNative.cpp`, `UnEngineNative.cpp`) have **zero** hits — that code was written against the PS2
ee-gcc (ISO scoping), which also explains the double-brace idiom `{for( INT i=0,…; …) {…}}` in `UnMisc.cpp` and
`UnLinker.cpp`. Modern GCC (≥9, `-ffor-scope` removed) and clang make every real hit a hard
`'i' was not declared in this scope`. The scan has false positives (e.g. `Engine/Inc/UnActor.h:61→62` is a nested
use inside the loop statement) — the compiler is the arbiter.
**ACTION (d.3a):** fix during the 3-way merge by taking the template's resolution for the 32 shared files; fix
`UnLinker.cpp:413-418` by hand (hoist `INT j` or re-declare).

**Non-POD through `...`.** `debugf/appErrorf/Logf/Printf/appSprintf/StatusUpdatef` with an `FString` argument
(missing `*`). A scanner over the same files (`nonpod.py`) found 4 candidates, all **false positives** on reading
(`UnObj.cpp:613,615` pass a `TCHAR Value[4096]`; `AStatLog.cpp:410,429` pass a `const TCHAR* FileName`). Behaviour if one
slips through: gcc-15 accepts silently (verified in scratchpad `va.cpp`: rc 0, no diagnostic) and passes the object by invisible
reference, so `%s` prints garbage (`va2.cpp`); a better scan with FString member names (`nonpod2.py`) then found **one real site:
`Core/Src/UnMisc.cpp:2030` `debugf( TEXT("Save Slot Path: %s"), GSys->SaveSlotPath );`** (§i.3 — it runs in every `appInit`);
**clang/em++ diagnoses `cannot pass object of non-trivial type … call will abort at runtime` and emits a trap** — HP2's
`Source/CMakeLists.txt:81` already passes `-Wno-error=non-pod-varargs -Wno-non-pod-varargs` in the wasm block (as the
template), which *silences* the diagnostic but not the trap.
**ACTION (d.3b):** in the wasm block change `-Wno-non-pod-varargs` to keep the warning visible (`-Wno-error=non-pod-varargs`
only) and treat every hit as a must-fix (`*Str`). This matters more for the HP2 packages outside Core/Engine (HGame,
HPParticle, ALAudio, Render) which were not scanned here.

### d.4 `TCHAR_CALL_OS`
`Core/Inc/UnGnuG.h:183` (and the template's `:234`) map it to the ANSI call; users outside Windows-only files: only
`Editor/Src/TTFontImport.cpp` (Editor, TTF import — not built for the game). **No action.**

### d.5 `#if _MSC_VER` sites in GCC-compiled Core/Engine — what the `#else` does (semantic hazards)

| File:line | MSVC arm | non-MSVC arm | Verdict / ACTION |
|---|---|---|---|
| `Core/Inc/Core.h:55,62` | select `UnVcWin32.h` | `UnGnuG.h` | fine |
| `Core/Inc/UnFile.h:108-124` | `guard()` = `try{` | `__Context` + `setjmp` | take the template's (§a.2) |
| `Core/Inc/UnObjBas.h:349,369,425` | `IMPLEMENT_CLASS` / `IMPLEMENT_PACKAGE` | `DLL_EXPORT {…}` spelling | replace with template text (§f) |
| `Core/Inc/UnScript.h:23-31` | `IMPLEMENT_FUNCTION` | `extern "C" DLL_EXPORT {…}` | add `STATIC_EXPORT` (§f) |
| `Core/Src/UnAnsi.cpp:42-68` | `appTimestamp` via `_strdate` | none (Unix core defines its own) | fine |
| `Core/Src/UnAnsi.cpp:142-146` | `_isnan` | `isnan` | fine |
| `Core/Src/UnAnsi.cpp:282-290` | `return GET_VARARGS(Dest,1024,Fmt);` | `int Result; GET_VARARGS_RESULT(Dest,1024,Fmt,Result);` — **no `return Result;`** (falls off a non-void function: UB, `-Wreturn-type`) | add `return Result;` — the template already did (`ut99:Core/Src/UnAnsi.cpp:390-392`), so the 3-way merge brings it |
| `Core/Src/UnLinker.cpp` `catch( TCHAR* Error )` vs `catch( char* Error )` ("PSX2 gcc doesn't like catch(TCHAR*)") | | | identical under ANSI; but see §l `const` catch |
| `Core/Src/UnObj.cpp:1833-1837` | `DUMPNATIVES` free-slot listing | disabled | fine |
| `Engine/Inc/UnLevel.h` `enum ELevelTick` | defined inline | provided by `Engine/Inc/UnEngineGnuG.h` via `Engine.h` `#if __GNUG__` | fine (MinGW/emcc are `__GNUG__`) |
| `Engine/Inc/UnLevel.h` / `UnLevel.cpp` `WelcomePlayer(…, TCHAR* Optional=TEXT(""))` | TCHAR | `char* Optional=""` | identical under ANSI; string literal → `char*` needs `-Wno-write-strings` (present, `CMakeLists.txt:214`) |
| **`Engine/Src/UnCamera.cpp:370-398`** (`UViewport::Destroy`) | deletes `Input`, `Console`, `Canvas`, **`RenDev->Exit(); delete RenDev;`** | **all of it skipped** (PS2 hack) → leaks the render device and never calls `Exit()` | **ACTION: remove the `#if _MSC_VER`/`#endif` pair** — the web/SDL client recreates viewports (fullscreen toggles, `SetRes`), a leaked GL context / never-`Exit()`ed XOpenGL device per recreate is fatal |
| **`Engine/Src/UnCamera.cpp:534-543`** (screenshot BMP writer) | `for( INT i=SizeY-1; i>=0; i-- )` | `for( INT i=SizeY; i>0; i-- )` → reads **one row past** `Buf` and skips row 0 | **ACTION: use the MSVC loop unconditionally** |
| `Engine/Src/AStatLog.cpp:338-380` | `FString` log line | hand-rolled UTF-16 emulation ("Emulate UNICODE for Linux") | works; harmless |
| `Engine/Src/UnGame.cpp` `catch( TCHAR* CatchError )` / `catch( char* CatchError )` | | | §l |
| `Engine/Inc/EngineClasses.h:6-7, 3953-3954` | `#pragma pack(push,4)` | nothing | flags (§d.2) |

Also `#ifdef WIN32` (not `_MSC_VER`) in `Engine/Src/UnTex.cpp:12-16` (includes `UnDDraw.h`, `Palette.h`, `S3tc.h`) and the
`UTexture::Compress/Decompress` body (calls `S3TCencode/S3TCgetEncodeSize/S3TCdecode/HeckbertQuantize`; the `#else` stubs
return 0). `Source/CMakeLists.txt:162` defines `WIN32` for MinGW → the MinGW build would pull `s3tc.lib` (MSVC-only,
closed). **ACTION (d.5):** change both `#ifdef WIN32` in `UnTex.cpp` to `#if _MSC_VER` (the template did the same for
v400's copy). No caller of `Compress/Decompress` outside `UnTex.cpp` (verify: `grep -rn 'Compress(' Source/{Engine,Render,HGame}`).

### d.6 The UNICODE question — HP2 shipped a UTF-16 build; the port is ANSI, and that is correct

**How HP2 was built.** `Source/Core/Src/Core.dsp:46` (Release):
`# ADD CPP /nologo /Zp4 /MD … /D CORE_API=__declspec(dllexport) /D "_WINDOWS" /D "NDEBUG" /D "UNICODE" /D "_UNICODE" /D "WIN32" …`;
same in `Engine/Src/Engine.dsp:46,74`, `Launch/Src/Launch.dsp:46,72`. So the retail `Core.dll` had `TCHAR = UNICHAR`
(2-byte), `TEXT(s) = L##s`. (Also `/Zp4` = 4-byte struct packing — the reason for `-fpack-struct=4` on targets whose
ABI would otherwise 8-align `double`/`QWORD`.)

**What TCHAR is under `__GNUG__`.** `Core/Inc/Core.h:93-120` (identical to epic v400):
```cpp
#ifdef _UNICODE
	#ifndef _TCHAR_DEFINED
		typedef UNICHAR  TCHAR;      // UnGnuG.h:140  typedef unsigned short UNICHAR
	…	#define TEXT(s) L##s
#else
	#ifndef _TCHAR_DEFINED
		typedef ANSICHAR  TCHAR;     // char
	…	#define TEXT(s) s
	inline TCHAR    FromAnsi   ( ANSICHAR In ) { return In;                              }
	inline TCHAR    FromUnicode( UNICHAR In  ) { return (_WORD)In<0x100 ? In : MAXSBYTE; }
	inline ANSICHAR ToAnsi     ( TCHAR In    ) { return (_WORD)In<0x100 ? In : MAXSBYTE; }
	inline UNICHAR  ToUnicode  ( TCHAR In    ) { return (BYTE)In;                        }
#endif
```
HP2's CMake defines neither `UNICODE` nor `_UNICODE` (`grep -n UNICODE Source/CMakeLists.txt` → nothing) → **TCHAR = `char`**,
exactly as the template (whose CMake also has no UNICODE define; `ut99:UnGnuG.h:171-174` `typedef char ANSICHAR; typedef uint16_t UNICHAR;`).
Two spellings matter: `_UNICODE` selects the typedef/TEXT, `UNICODE` selects code paths (`UnAnsi.cpp:48,251,297,309,318,330,339,348,357,366,375,384,398,407,416`,
`UnFile.h:212,350`). **Define neither.** (Defining them on GCC would be a disaster anyway: `L"…"` is 4-byte `wchar_t` on Linux and wasm,
2-byte on MinGW, while `UNICHAR` is 2 bytes.)

**What the content assumes — nothing that breaks ANSI:**
- **Package strings.** `Core/Src/UnMisc.cpp:133-163`:
  ```cpp
  INT SaveNum = appIsPureAnsi(*A) ? A.Num() : -A.Num();
  Ar << AR_INDEX(SaveNum);
  if( Ar.IsLoading() ) { …
      if( SaveNum>=0 ) for(…) {ANSICHAR ACh; Ar << *(BYTE*)&ACh; A(i)=FromAnsi(ACh);}
      else             for(…) {UNICHAR UCh; Ar << UCh; A(i)=FromUnicode(UCh);}
  ```
  The on-disk format is self-describing (positive length = 8-bit, negative = UTF-16), identical to v400 and the template.
  An ANSI build loads UTF-16 strings by mapping code points ≥0x100 to `MAXSBYTE` (0x7F) — Latin-1 survives, CJK does not.
  Names (`FNameEntry`) go through the same `FString` path for `Ar.Ver()>=64` (HP2 packages are v79, §k).
- **Text files.** Every shipped `.int`/`.ini`/localised file is 8-bit with no BOM (checked: `head -c2 | xxd` over
  `Assets/System/*.int` (25), `SAPFont.{jap,kor,sim,tha,tra}`, all 207 `Assets/SystemLocalized/*.{det,est,frt,itt,int}` →
  every one starts with `[`, 0x5B). `hpdialog.int`/`SAPFont.jap` contain bytes ≥0x80 (`file`: "Non-ISO extended-ASCII") —
  cp1252 / DBCS bytes. In the retail UNICODE build `appLoadFileToString` widened each byte with `FromAnsi` ((BYTE)In) → the
  glyph index equalled the byte; in the ANSI build the byte is kept → **same glyph lookup for single-byte languages**.
- **Asian SKUs are out of scope.** The `SAPFont.jap/kor/sim/tha/tra` files and multi-byte dialogue only render correctly in a
  UTF-16 build with the matching fonts; the port (like the template) is single-byte. Document it; do not chase it.
- **FTime serialisation** (`UnMisc.cpp:164-174`, HP2-only) writes an FTime as a pseudo-double built by copying float
  mantissa/exponent bitfields into a double union without re-biasing the exponent — self-consistent (load inverts save),
  lossy (float precision), and endian/bitfield-layout dependent (`QWORD` bitfields; `-mms-bitfields` on MinGW packs
  them into the same 64-bit unit, so it matches). Keep as-is; only matters for savegames written by one build and read by another.

**The one hard ANSI break in Core/Engine.** `Core/Src/UnClass.cpp:1428-1441` (inside `UStruct::SerializeExpr`, runs for
every `Expr==130` during script load):
```cpp
TCHAR badTeleport1[256]=TEXT("Function Engine.Teleporter.SpecialHandling");
TCHAR badTeleport2[256]=TEXT("Class Engine.Teleporter");
const TCHAR* nameComp;
…
nameComp=this->GetFullName();
if((wcscmp(nameComp,badTeleport1)!=0)&&(wcscmp(nameComp,badTeleport2)!=0))
{ HANDLE_OPTIONAL_DEBUG_INFO(); }
```
`wcscmp(const char*, char*)` → **hard error**. **ACTION (d.6):** `appStrcmp`. No other `wcs*`/`L"…"`/`wchar_t` exists in
GCC-compiled Core/Engine outside the `#if UNICODE` arms of `UnAnsi.cpp` (grepped `\bL"|wchar_t|\bwcs|_wcs|swprintf|_tcs|WCHAR`).
Note what that block protects: `HANDLE_OPTIONAL_DEBUG_INFO` (`UnClass.cpp:1400-1415`) peeks for the 433 `EX_DebugInfo` (0x38)
opcode after a native call and only serialises it when its version INT == 100 — HP2's `.u` files carry debugger opcodes
(`UnScript.h:71` `P_FINISH` also skips one). That is 433 bytecode, handled by HP2's own `UnClass.cpp`/`UnCorSc.cpp` — **never
take the template's `SerializeExpr`/`P_FINISH` during the merge.**

### d.7 Summary of GCC/clang hard errors found in Core+Engine (outside `UnGnuG.h`, which is replaced)

| Site | Text | Why | ACTION |
|---|---|---|---|
| `Core/Src/UnClass.cpp:1439` | `wcscmp(nameComp,badTeleport1)` | wide call on `char*` | `appStrcmp` |
| `Engine/Inc/Palette.h:18` | `__declspec( dllexport )` | not a keyword on Linux/wasm | `#if _MSC_VER` |
| `Core/Inc/UnObjBas.h:385`, `:431-433`, `Core/Inc/UnScript.h:28-30`, `Core/Inc/Core.h:473` | `DLL_EXPORT {…}` with the template's empty `DLL_EXPORT` | bare `{…}` at namespace scope | template macro text (§f) |
| `Core/Src/UnLinker.cpp:413-418` (+ 32 v400-inherited files) | `for`-scope reuse | ISO scoping | §d.3 |
| `Core/Src/UnUnix.cpp` (HP2's) `appCycles` `return (DWORD)appSeconds();` | no `FTime→DWORD` conversion | type error | moot — take the template's `UnUnix.cpp` (§a.3) |
| `Core/Inc/UnMath.h`, `Engine/Inc/UnRender.h` | multi-line asm string literals in skipped groups | warnings only while skipped | delete (§b, §c) |
| `Core/Inc/UnCoreNative.h:16` | `(##type` | invalid paste | only under `__STATIC_LINK`; fix anyway |

Semantic (compiles, misbehaves):

| Site | Problem | ACTION |
|---|---|---|
| `Core/Inc/UnObjBas.h:333` `InternalConstructor` → `new( (EInternal*)X )TClass();` | value-initialisation **zero-fills** classes without a user-provided default ctor *after* `StaticAllocateObject` wrote the UObject header (Class/Outer/Name) → objects with `Class==NULL` (template comment `ut99:UnObjBas.h:297-310`, found on v61 map loads) | `new( (EInternal*)X )TClass;` (no parens) — port with the macro trio |
| `Core/Src/UnAnsi.cpp:282-290` `appSprintf` | no `return Result` on GCC | add it |
| `Engine/Src/UnCamera.cpp:370-398` | viewport destroy skipped on GCC | drop the `#if` |
| `Engine/Src/UnCamera.cpp:534-543` | BMP writer off-by-one-row | MSVC loop |

## (e) Threading — `AudioMutex`, the file-streaming thread, the audio mixer

### e.1 `AudioMutex` — dead code, and toxic to include
`Core/Inc/AudioMutex.h:1-52` hand-copies `LIST_ENTRY`, `RTL_CRITICAL_SECTION_DEBUG`, `RTL_CRITICAL_SECTION` out of `<winnt.h>`
under `#ifndef _WINDOWS_` ("Trying to include the windows headers caused lots of errors"), and to do so **re-typedefs**
`typedef unsigned short WORD; typedef unsigned long DWORD; typedef long LONG;` (11-13) — `DWORD` as `unsigned long`
conflicts with the GCC headers' `DWORD` (`unsigned int` in HP2's `UnGnuG.h:126`, `uint32_t` in the template) → error in
any TU that includes both. `Core/Src/AudioMutex.cpp:1-3` includes `"windows.h"` and `"core.h"` (lower-case: fails on a
case-sensitive FS), wraps `Initialize/Enter/Leave/DeleteCriticalSection`, and defines the global `AudioMutex audioFileMutex;` (38).

**Users: none.** `grep -rn 'AudioMutex\|audioFileMutex' Source --include=*.cpp --include=*.h` returns only those two files
(Engine, ALAudio, HGame, Render and the copied template drivers are clean). `AudioMutex.cpp` is **not in `Core.dsp`** either
(`grep -rn AudioMutex Source --include=*.dsp` → nothing) — it never shipped. `Core/CMakeLists.txt` correctly omits it.
**ACTION (e.1):** nothing to port. Keep both files on disk unbuilt; never add `#include "AudioMutex.h"` anywhere. If a
future NOpenALDrv needs a lock, use `SDL_mutex`/`std::mutex` locally. (PLAN.md line 179-180 can be closed as "no caller".)

### e.2 The file-streaming thread and `FFileStream` — the real threading surface
**Interface** (HP2-only, "TG ALPHA"): `Core/Inc/Core.h:344-409` — `enum EFileStreamType { ST_Regular, ST_Ogg, ST_OggLooping,
ST_XA, ST_XALooping }`, `struct FStream { Data, Handle, TDD, FileSeek, ChunkSize, ChunksRequested, ChunksRemaining, Locked,
Used, EndOfFile, NumSamples, Type }`, singleton `class CORE_API FFileStream` (`CreateStream`×2, `RequestChunks`,
`DecrementChunkCount`, `ChunksRemaining`, `IsStreamAlive`, `DestroyStream`, and "Only use the below functions in the thread's
main loop": `Read`, `Create`×2, `Destroy(INT)`, `Enter`, `Leave`). Global `GFileStream` (`Core.h:468`, defined `Core.cpp:172`).

**Portable half** — `Core/Src/UnMisc.cpp:1605-1810`: `Init` (`new FStream[N]`, zeroed), `Destroy`, the spin "lock"
```cpp
void FFileStream::Enter( INT StreamId ) // has to be reentrant
{
	while (Streams[StreamId].Locked)
		appSleep(0); //!! appSleep has to be reentrant!
	Streams[StreamId].Locked = 1;
}
void FFileStream::Leave( INT StreamId ) { Streams[StreamId].Locked = 0; }
```
(a non-atomic test-then-set on a plain `INT` — a data race even on Windows), and `CreateStream`, `DestroyStream`,
`RequestChunks`, `DecrementChunkCount`, `ChunksRemaining`, `IsStreamAlive`. Created in `appInit` (`UnMisc.cpp:1993-1997`):
```cpp
bSuccess = GConfig->GetInt( TEXT("ALAudio.ALAudioSubsystem"), TEXT("Channels"), iMaxChannels );
GFileStream = FFileStream::Init( bSuccess ? iMaxChannels : 32 );
```
(note the hard-coded ini section `ALAudio.ALAudioSubsystem` — when the port's audio device is `NOpenALDrv.NOpenALAudioSubsystem`
the key is simply absent and 32 streams are allocated; harmless).

**Win32 half** — `Core/Src/UnVcWin32.cpp:651-935` only: `FileStreamingThread` (667-703) loops `Sleep(1)` over all streams,
`Enter(i)`, and if `Used && ChunksRequested` does `Read(i, ChunkSize); ChunksRequested--; ChunksRemaining++;`; started by
`CreateThread(…FileStreamingThread…)` at `UnVcWin32.cpp:1489` inside `appPlatformInit` (right after `CODASetNew/CODASetDelete`).
`FFileStream::Read` (706-830): `ST_Regular` → `ReadFile`; `ST_Ogg/ST_OggLooping` → `ov_read(…)`, loop via `ov_time_seek(…,0.0)`
or zero-fill + `EndOfFile=1`; `ST_XA*` → EA `coda` XA block decoder. `Create(StreamId, Filename)` (831-887) opens the file + `ov_open`;
`Create(StreamId, TLazyArray<BYTE>*, NumSamples)` (889-922) sets up an XA decoder on in-package data; `Destroy(INT)` (924-935).

**Consumer**: only `ALAudio/Src/ALAudioSubsystem.cpp` (`CreateStream` 364/470, `RequestChunks` 432/540/1506/1528,
`IsStreamAlive` 1468, `ChunksRemaining`/`DecrementChunkCount` 1485-1489, `DestroyStream` 389/488/652/670). Music:
`PlayMusic` builds `FString Filename = TEXT("..\\music\\"); … Filename += TEXT(".ogg");` (`ALAudioSubsystem.cpp:1136-1140`) —
backslashes and lower-case `music` vs the disk's `Assets/Music/` (§h).

Hazards and **ACTIONS (e.2)**:
1. **Link — confirmed hard error.** `UnMisc.cpp`'s portable half itself calls the Win32 half: `CreateStream` →
   `Create( iStreamIndex, RawData, NumSamples )` (1684) / `Create( iStreamIndex, Filename )` (1733) and
   `Read( iStreamIndex, ChunkSize * InitialChunks )` (1698, 1747); `DestroyStream` → `Read(…)` (1764) and `Destroy(StreamId)` (1767).
   `UnMisc.o` is always linked (and `--whole-archive` in the static build), so the Unix/web link fails with undefined
   `FFileStream::Create(int,…)`×2, `Read(int,int)`, `Destroy(int)`. **Write `Core/Src/UnFileStream.cpp`** for GCC targets: `ST_Regular` → `GFileManager->CreateFileReader`;
   `ST_Ogg*` → libvorbis `ov_open_callbacks` over an `FArchive` (the template's `cmake/` already builds vorbis for NOpenALDrv;
   Core would need it too — alternative: register a function-pointer hook that NOpenALDrv fills, keeping Core vorbis-free);
   `ST_XA*` → `return 0` + `debugf(NAME_DevSound,…)` until PLAN §7 decides the codec.
2. **No thread on Unix/web.** Without the Win32 thread nobody services `ChunksRequested`; streamed sounds and music stay
   silent while `IsStreamAlive` reports alive. Choose: (a) **synchronous pump** — `FFileStream::Pump()` doing exactly the
   thread body once, called from the audio subsystem's `Update()` (single-threaded, identical on native and web, makes
   the `Enter/Leave` spin-lock trivially safe); (b) an `SDL_CreateThread` worker (native only; on web a blocking
   `Sleep(1)` worker against the main-thread-proxied FS is a bad idea). **Recommend (a).**
3. **Exit hang.** `FFileStream::Destroy()` (`UnMisc.cpp:1623-1641`):
   ```cpp
   Destroyed = 1;
   debugf(TEXT("Waiting for file streaming thread to finish..."));
   #if !__UNIX__
   //!!TODO: vogel: implement
   	while (Destroyed == 1)
   		appSleep(1);
   #endif
   ```
   The template's `UnGnuG.h` does **not** define `__UNIX__` → the wait loop is compiled in and, with no thread to set
   `Destroyed=2`, **`appExit` spins forever** (on web: the tab hangs on quit/restart). Fix by the §a.2 item 3 decision
   (define `__UNIX__` on POSIX **and** make MinGW take the same path) — simplest robust edit: `#if _MSC_VER` instead of `#if !__UNIX__`.
4. `appSleep(0)` → `usleep(0)` (`UnUnix.cpp:463`) is a no-op; fine once single-threaded.

### e.3 Is the audio mixer on a thread?
Not in the engine. `ALAudio` calls OpenAL from the game thread (`UALAudioSubsystem::Update`); Creative's OpenAL 1.0 DLL
mixed on its own thread, as openal-soft does (on web openal-soft's Emscripten backend mixes from a WebAudio
ScriptProcessor/AudioWorklet callback — the template's NOpenALDrv pattern). Engine-created threads in the whole drop:
the streaming thread above, `Engine/Inc/UnEngineWin.h:116` (Win32 launcher header — not built), and in the *copied template*
`IpDrv/Src/IpDrvPrivate.h:124-132` (DNS resolve thread: `CreateThread` on Win32, `pthread_create` detached on POSIX —
the template already copes with it on web; the IpDrv owner decides). HP2 Core/Engine/Render/HGame create no other thread.
**ACTION (e.3):** NOpenALDrv inherits no threading obligation beyond the stream pump in e.2; the web build must not rely on
`-pthread` for audio.

## (f) Native-function registry — `UnCoreNative`/`UnEngineNative` vs `GNatives`; the IMPLEMENT_* macros; what the static registry must do

### f.1 What is unchanged from v400 (and the template)
- **Numbered natives** — `Core/Inc/UnScript.h:16-18`:
  `typedef void (UObject::*Native)( FFrame& TheStack, RESULT_DECL ); extern CORE_API Native GNatives[]; BYTE CORE_API GRegisterNative( INT iNative, const Native& Func );`
  and `Core/Src/UnCorSc.cpp:19` `CORE_API void (UObject::*GNatives[EX_Max])( FFrame &Stack, RESULT_DECL );` — same as v400/template.
  Registration at static-init time through `IMPLEMENT_FUNCTION` — counted: **Core 242** (`UnCorSc.cpp` 241 + `UnObj.cpp:3975`
  `UCommandlet::execMain`), **Engine 28** (`UnCanvas` 9, `UnPawn` 7, `UnScrTex` 5, `UnCon` 3, `UnScript` 3, `UnEngine` 1).
  (Most Engine natives are bound *by name*, below.)
- **Unnumbered natives by name** — `Core/Src/UnClass.cpp:1789-1796` (`UFunction::Bind`):
  ```cpp
  TCHAR Proc[256];
  appSprintf( Proc, TEXT("int%sexec%s"), GetOwnerClass()->GetNameCPP(), GetName() );
  UPackage* ClassPackage = GetOwnerClass()->GetOuterUPackage();
  Native* Ptr = (Native*)ClassPackage->GetDllExport( Proc, 1 );
  ```
- **Native classes** — `UnClass.cpp:1050-1066` (`UClass::Bind`): `appSprintf( ProcName, TEXT("autoclass%s"), GetNameCPP() ); … UClass** ClassPtr = (UClass**)ClassPackage->GetDllExport( ProcName, 0 );`
- **Package handle** — `Core/Src/UnObj.cpp:1575-1592` (`BindPackage`): `appSprintf( PathName, TEXT("%s%s"), appBaseDir(), Pkg->GetName() ); … Pkg->DllHandle = appGetDllHandle( PathName );`
  and `UPackage::GetDllExport` (`UnObj.cpp:4012-4034`):
  ```cpp
  #if !__STATIC_LINK
  if( !DllHandle ) { if( Checked && !ParseParam(appCmdLine(),TEXT("nobind")) ) appErrorf( LocalizeError("NotDll"), GetName(), ExportName ); Result = NULL; }
  else
  #endif
  {
  	#if __STATIC_LINK
  	Result = FindNative( (char*) ExportName );
  	#else
  	Result = appGetDllExport( DllHandle, ExportName );
  	#endif
  ```
So with `__STATIC_LINK` undefined the three symbol families a single static binary must answer are **`GLoaded<Pkg>`**
(the handle), **`autoclass<Class>`** and **`int<Class>exec<Func>`** — exactly what the template's registry provides
(`Core/Inc/UnStaticExports.h`, already copied into HP2 byte-identical to `ut99:`; `STATIC_EXPORT( sexp, ssym ) extern "C" { GCC_USED
FPackageExport GStaticExport##sexp( #sexp, (void*)&ssym ); }`, a linked list walked by `appGetStaticExport(Name)` in
`Core/Src/UnStaticExports.cpp`), wired from the template's `UnUnix.cpp` `appGetDllHandle`/`appGetDllExport`.

### f.2 What 433 added — the `__STATIC_LINK` / `GNativeLookupFuncs` scheme (Loki/PS2), and why we do not use it
- `Core/Inc/UnCoreNative.h` (46 lines, "Created by Brandon Reinhart" — the Loki UT-Linux/PS2 static-link author):
  `MAP_NATIVE` rows `{"int"#cls#name, &cls::name}`, `DECLARE_NATIVE_TYPE(pkg,type)` (typedef + `struct type##NativeInfo {TCHAR* Name; type##Native Pointer;}` +
  `extern void* Find##pkg##type##Native(char*)`), `IMPLEMENT_NATIVE_HANDLER(pkg,type)` (a linear `appStrcmp` over
  `G##pkg##type##Natives[]` returning `&…Pointer`, a pointer *to* the member pointer — matches what `UFunction::Bind` dereferences),
  `typedef void* (*NativeLookup)(char*); extern NativeLookup GNativeLookupFuncs[32];` and a header-`static` `FindNative()` that
  tries every slot. `DECLARE_NATIVE_TYPE(Core,UObject); DECLARE_NATIVE_TYPE(Core,UCommandlet);`
- Tables: `Core/Src/UnCoreNative.cpp:11-276` (`GCoreUObjectNatives[]` ≈250 rows, `GCoreUCommandletNatives[]` 1 row,
  `NativeLookup GNativeLookupFuncs[32];` at 272) and `Engine/Src/UnEngineNative.cpp:11-239` (AActor, APawn, APlayerPawn, ADecal,
  AStatLog, AStatLogFile, AZoneInfo, AWarpZoneInfo, ALevelInfo, AGameInfo, ANavigationPoint, UCanvas, UConsole, UScriptedTexture —
  the 14 `DECLARE_NATIVE_TYPE(Engine,…)` in `Engine/Inc/UnEngineNative.h:12-26`). **Both files are wholly inside `#if __STATIC_LINK`.**
- Slot filling is done by hand in the launcher: HP2's own `UCC/Src/UCC.cpp:121-158` (still HP2's file on disk; only its
  `CMakeLists.txt` came from the template) sets `GNativeLookupFuncs[0..15] = &FindCoreUObjectNative, …` and comments out IpDrv/UWeb.
- Classes are pre-registered by the macro lists `AUTO_INITIALIZE_REGISTRANTS` (`Core/Inc/UnObjBas.h:1079-…`, invoked from
  `UObject::StaticInit`, `UnObj.cpp:1331-1333` under `#if __STATIC_LINK`) and `AUTO_INITIALIZE_REGISTRANTS_ENGINE`
  (`Engine/Inc/UnEngineNative.h:28-127`, invoked from `UCC.cpp:204`).
- Under `__STATIC_LINK` the class macros change shape (`UnObjBas.h:298-323`, `386-422`): `PrivateStaticClass` becomes a
  `UClass*` created lazily by `GetPrivateStaticClass##TClass( TEXT(#TPackage) )` with the 433-only `EC_StaticConstructor`
  (`UnObjBas.h:49`) and linked by `InitializePrivateStaticClass##TClass()` — **this is why every HP2 `DECLARE_CLASS` carries a 4th
  `TPackage` argument** (e.g. `DECLARE_CLASS(UCheckSumCommandlet,UCommandlet,CLASS_Transient,Engine)`, `AStatLog.cpp:402`).
- Weaknesses: hand-maintained tables (a new native = edit two tables + the launcher), no `autoclass` path (classes must be in the
  `AUTO_INITIALIZE_*` lists), the `(##type` paste error (§d.1), `TCHAR* Name` initialised from narrow literals (ANSI-only).
**Decision (PLAN §2, confirmed here):** `__STATIC_LINK` stays **undefined** on every target; HP2 uses the template's `UNREAL_STATIC`
registry. Nothing in the 433 content depends on the `__STATIC_LINK` code path.

### f.3 The macros, quoted — HP2 vs template
HP2 `Core/Inc/UnObjBas.h`, the arms GCC compiles today (non-MSVC, non-static):
```cpp
284 #if !__STATIC_LINK
285 #define DECLARE_BASE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
      …  private: static UClass PrivateStaticClass; public: … static UClass* StaticClass() { return &PrivateStaticClass; } …
326 #define DECLARE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
327 	DECLARE_BASE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
      …
332 	static void InternalConstructor( void* X ) \
333 		{ new( (EInternal*)X )TClass(); } \
336 #define DECLARE_ABSTRACT_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
368 #elif !__STATIC_LINK
369 	#define IMPLEMENT_CLASS(TClass) \
370 		UClass TClass::PrivateStaticClass \
371 		( EC_NativeConstructor, sizeof(TClass), TClass::StaticClassFlags, TClass::Super::StaticClass(), \
375 		  TClass::WithinClass::StaticClass(), FGuid(TClass::GUID1,…), TEXT(#TClass)+1, \
379 			GPackage, \
380 			StaticConfigName(), RF_Public | RF_Standalone | RF_Transient | RF_Native, \
382 			(void(*)(void*))TClass::InternalConstructor, (void(UObject::*)())&TClass::StaticConstructor ); \
385 		DLL_EXPORT {UClass* autoclass##TClass = TClass::StaticClass();}
430 #else
431 	#define IMPLEMENT_PACKAGE(pkg) \
432 	extern "C" DLL_EXPORT {TCHAR GPackage[] = TEXT(#pkg);} \
433 		IMPLEMENT_PACKAGE_PLATFORM(pkg)
```
HP2 `Core/Inc/UnScript.h:27-30`:
```cpp
	#define IMPLEMENT_FUNCTION(cls,num,func) \
		extern "C" DLL_EXPORT { Native int##cls##func = (Native)&cls::func; } \
		static BYTE cls##func##Temp = GRegisterNative( num, int##cls##func );
```
HP2 `Core/Inc/UnGnuG.h:96-98, 175-176`: `#define DLL_EXPORT extern "C"` and `IMPLEMENT_PACKAGE_PLATFORM(pkgname) BYTE GLoaded##pkgname;`.
Note that with HP2's `DLL_EXPORT` the `IMPLEMENT_PACKAGE` line expands to `extern "C" extern "C" {…}` and `IMPLEMENT_FUNCTION`'s to
`extern "C" extern "C" {…}` — legal nesting, which is the only reason it ever compiled.

Template (`ut99:Core/Inc/UnObjBas.h:334-357`, `ut99:Core/Inc/UnScript.h:28-31`, `ut99:Core/Inc/UnGnuG.h:93-126,220-227`, `ut99:Core/Inc/Core.h:348-355`):
```cpp
#define DLL_EXPORT                                           // empty
#define GCC_HIDDEN  __attribute__((visibility("hidden")))   // empty under UNREAL_STATIC
#define GCC_USED    __attribute__((used))
#ifdef UNREAL_STATIC
	#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
		extern "C" {BYTE GCC_USED DLL_EXPORT GLoaded##pkgname;} \
		STATIC_EXPORT( GLoaded##pkgname, GLoaded##pkgname )
#else …
#ifdef UNREAL_STATIC
#define THIS_PACKAGE_PASTE(A, B) A ## B
#define THIS_PACKAGE_EVAL(A, B) THIS_PACKAGE_PASTE(A, B)
#define THIS_PACKAGE THIS_PACKAGE_EVAL(GPackage, UPACKAGE_NAME)      // GPackageCore, GPackageEngine, …
#else
#define THIS_PACKAGE GPackage
#endif
extern "C" DLL_EXPORT char THIS_PACKAGE[];

	#define IMPLEMENT_CLASS(TClass) \
		UClass TClass::PrivateStaticClass ( … TEXT(#TClass)+1, THIS_PACKAGE, StaticConfigName(), … ); \
		DLL_EXPORT UClass* autoclass##TClass = TClass::StaticClass(); \
		STATIC_EXPORT( TClass, autoclass##TClass )
	#define IMPLEMENT_PACKAGE(pkg) \
	extern "C" { DLL_EXPORT GCC_HIDDEN TCHAR THIS_PACKAGE[] = TEXT(#pkg);} \
		IMPLEMENT_PACKAGE_PLATFORM(pkg)
	#define IMPLEMENT_FUNCTION(cls,num,func) \
		extern "C" DLL_EXPORT { Native int##cls##func = (Native)&cls::func; } \
		static BYTE cls##func##Temp = GRegisterNative( num, int##cls##func ); \
		STATIC_EXPORT( int##cls##func, int##cls##func )
```
and the template's `DECLARE_CLASS` is **3-argument** with the no-parens `new( (EInternal*)X )TClass;` (explained in its comment, `ut99:UnObjBas.h:297-310`).
`AUTOGENERATE_NAME(name)` = `extern ENGINE_API FName ENGINE_##name;` and `AUTOGENERATE_FUNCTION(cls,idx,name)` = empty
(`Engine/Inc/EngineClasses.h:15-16`) — the same in both trees; `EngineClasses.h` is the 433 script-generated header and must be
kept (it encodes HP2's reflected layouts; never take the template's).

### f.4 What the static registry must do for 433 — exact edit list (one commit with the `UnGnuG.h` swap)
1. `Core/Inc/Core.h:473`: replace `extern "C" DLL_EXPORT TCHAR GPackage[];` with the template's `THIS_PACKAGE` block + `extern "C" DLL_EXPORT char THIS_PACKAGE[];`,
   and add `#include "UnStaticExports.h"` (after `UnFile.h`, as the template). CMake already passes `UPACKAGE_NAME=${PROJECT_NAME}`
   (`Core/CMakeLists.txt`, `Engine/CMakeLists.txt` last line) and `-DUNREAL_STATIC` (`Source/CMakeLists.txt:92`).
2. `Core/Inc/UnFile.h:206-217`: the ten `Localize*( …, const TCHAR* Package=GPackage, … )` defaults → `THIS_PACKAGE`. (11 `GPackage`
   references in Core+Engine outside `UnObjBas.h`: these 10 + `Core.h:473`.) Without it: one `GPackage` symbol per package → duplicate
   definitions at link, or every `LocalizeError("…")` resolving against Core.
3. `Core/Inc/UnObjBas.h`: keep the **4-argument** `DECLARE_BASE_CLASS/DECLARE_CLASS/DECLARE_ABSTRACT_CLASS` (every HP2 header and the
   generated `EngineClasses.h` use them; the 4th argument becomes unused), delete the `__STATIC_LINK` arms (298-323, 386-422), take the
   template's `IMPLEMENT_CLASS` (THIS_PACKAGE + `STATIC_EXPORT`) and `IMPLEMENT_PACKAGE`, and the no-parens `InternalConstructor`.
   Optional convenience for the copied template drivers (which use 3 args — `NSDLDrv.h:26,109`, `IpDrv`, `WebRTCDrv`, NullDrv…):
   `#define DECLARE_CLASS( TClass, TSuperClass, TStaticFlags, ... )` accepts both arities (C++20 / GNU `__VA_ARGS__` may be empty);
   the other reports instead add the 4th argument to each driver (`template-drivers.md`) — pick one, not both.
4. `Core/Inc/UnScript.h:28-30`: append `STATIC_EXPORT( int##cls##func, int##cls##func )`.
5. `Core/Src/UnObj.cpp:4012-4034`: nothing — with `__STATIC_LINK` undefined it calls `appGetDllExport`, which the template's
   `UnUnix.cpp` routes to `appGetStaticExport`. `DllHandle` becomes the (non-NULL) address of `GLoaded<Pkg>`; the "Bound to Engine.so"
   log line prints in a static build (cosmetic).
6. `UnCoreNative.cpp`, `UnEngineNative.cpp` compile to empty TUs — leave them in the lists (fidelity) or drop them.
7. Link every package with `--whole-archive` (HP2's CMake mirrors the template) so no static initialiser is discarded — the 270
   `GRegisterNative` calls and every `FPackageExport` constructor depend on it.
8. 433 extra: `UnCorSc.cpp:361 IMPLEMENT_FUNCTION( UObject, EX_DebugInfo, execDebugInfo );` (opcode 0x38, `UnStack.h:140`) — covered by 7;
   keep HP2's `P_FINISH` (`UnScript.h:71`) and `HANDLE_OPTIONAL_DEBUG_INFO` (`UnClass.cpp:1400-1415`).
9. HP2's `UCC/Src/UCC.cpp` (still the original: `#if __STATIC_LINK` slot filling at 121-158, `appInit( TEXT("HP"), …)` at 200,
   `AUTO_INITIALIZE_REGISTRANTS_ENGINE` at 204) is replaced by the template's `UCC.cpp` per PLAN B.5; its `__STATIC_LINK` blocks are dead
   meanwhile. Keep the `"HP"` ini base name in mind for §i.
10. Script-only HP2 packages (`HGame`, `HPParticle`, `HProps`, `HPModels`, `HPSounds`, `UWindow`, `UnrealShare` — `.uc` only, no
    `native` declarations found by grep) need **no** registry entries; every HP2 native lives in Core/Engine (+Render/Fire/ALAudio drivers).
    Verify at boot: `UClass::Bind` errors "Can't bind to native class" name any gap.

## (g) `UnStat.h` / `UnStats.h` stats and `FTime`

### g.1 `FTime` — 433's 32.32 fixed-point clock (keep it; never turn it back into `DOUBLE`)
`Core/Inc/Core.h:24-53` (quoted in full — every operator matters to the drivers):
```cpp
#define FIXTIME 4294967296.f
class FTime
{
#if __GNUG__
#define TIMETYP long long
#else
#define TIMETYP __int64
#endif
public:
	        FTime      ()               {v=0;}
	        FTime      (float f)        {v=(TIMETYP)(f*FIXTIME);}
	        FTime      (double d)       {v=(TIMETYP)(d*FIXTIME);}
	float   GetFloat   ()               {return v/FIXTIME;}
	FTime   operator+  (float f) const  {return FTime(v+(TIMETYP)(f*FIXTIME));}
	float   operator-  (FTime t) const  {return (v-t.v)/FIXTIME;}
	FTime   operator*  (float f) const  {return FTime(v*f);}
	FTime   operator/  (float f) const  {return FTime(v/f);}
	FTime&  operator+= (float f)        {v=v+(TIMETYP)(f*FIXTIME); return *this;}
	FTime&  operator*= (float f)        {v=(TIMETYP)(v*f); return *this;}
	FTime&  operator/= (float f)        {v=(TIMETYP)(v/f); return *this;}
	int     operator== (FTime t)        {return v==t.v;}
	int     operator!= (FTime t)        {return v!=t.v;}
	int     operator>  (FTime t)        {return v>t.v;}
	FTime&  operator=  (const FTime& t) {v=t.v; return *this;}
private:
	FTime (TIMETYP i) {v=i;}
	TIMETYP v;
};
```
Facts that bite:
| # | Fact | Consequence | ACTION |
|---|---|---|---|
| 1 | `TIMETYP` = `long long` under `__GNUG__` | correct 64-bit on i386, wasm32, MinGW | none |
| 2 | Only `>`, `==`, `!=` exist, all **non-const**; no `<`, `>=`, `<=`, no conversion to float/double | template-driver code like `if( appSeconds() < Deadline )` or `DOUBLE t = appSeconds();` fails to compile; comparing a `const FTime` fails | drivers: rewrite (`B > A`, `.GetFloat()`, `A - B` gives float) — listed in `renderer-forks.md`/`template-drivers.md`; optionally add `const` + `operator<` to `FTime` (header-only, binary-compatible, harmless) |
| 3 | `operator*`/`operator/`: `v*f` is `float`, so `FTime(v*f)` picks the **float ctor** and scales by 2³² again (the private `TIMETYP` ctor is not chosen); `*=`/`/=` are correct | latent bug; `grep -rn 'appSeconds() *\*\|Time *\* ' Source/{Core,Engine,Render}` finds no user | leave; do not "fix" silently |
| 4 | `GetFloat()` returns `float` of absolute time | 24-bit mantissa: at T = 10⁴ s resolution ≈ 1 ms, at 10⁶ s ≈ 0.06 s. Absolute `appSeconds().GetFloat()` users: `UnConn.cpp:209` (`appRound(appSeconds().GetFloat()*1000)`), `UnPath.cpp:96,129`, `UnScrTex.cpp:199`, debug prints | make `appSeconds()` start near 0 at boot (rebase the counter in `appPlatformInit`) — Windows `rdtsc` since power-on had the same exposure; rebasing is strictly better |
| 5 | HP2 stores an FTime raw inside the reflected `UBitmap`: `Engine/Inc/UnTex.h:249` `INT __LastUpdateTime[2];` + `:261-262` `appMemcpy(&T,&__LastUpdateTime,sizeof(FTime))` | `sizeof(FTime)` must stay 8 and the member stays 4-aligned (INT[2]) → no packing issue on any target | keep FTime 8 bytes |
| 6 | Serialised by `UnMisc.cpp:164-174` (pseudo-double, §d.6) | only for savegame/demo use | keep |

C++-only (non-reflected) FTime members: `UnNetDrv.h:45`, `UnBunch.h:18`, `UnChan.h:112-113`, `UnCamera.h:222,238`
(`UViewport::LastUpdateTime/CurrentTime`), `UnConn.h:38,80-83,96,114,120`, `UnLevel.h:146` (`ULevel::TimeSeconds`), and the
`UTexture::Lock/Update/Get( FTime Time )` virtuals (`UnTex.h:258,340,350,365`) — the latter are why every render driver must take
`FTime` in `Lock` calls.

**ACTION (g.1):** `UnFile.h:256-263` already declares `CORE_API FTime appSeconds();`, `appSecondsSlow()`, `appProcessSeconds()`.
Implement them in the Unix core (they do not exist there, §a.3):
```cpp
static Uint64 GStartCounter;                     // set in appPlatformInit
CORE_API FTime appSeconds()        { return FTime( (double)(SDL_GetPerformanceCounter()-GStartCounter) * GSecondsPerCycle ); }
CORE_API FTime appSecondsSlow()    { return appSeconds(); }
CORE_API FTime appProcessSeconds() { return appSeconds(); }   // Win32 used GetProcessTimes; only logged (UnGame.cpp:843,1465, UnRender.cpp:703)
```
(`GSecondsPerCycle` is FLOAT today — multiply in double: `(double)… * (double)GSecondsPerCycle`, see g.2.)

### g.2 `GSecondsPerCycle` and `appCycles`
HP2: `Core.h:437 CORE_API extern FLOAT GSecondsPerCycle;`, `Core.cpp:143 CORE_API FLOAT GSecondsPerCycle=1.0f;`,
`UnGnuG.h:194` / `UnVcWin32.h:262` `extern CORE_API FLOAT GSecondsPerCycle;`. Template: `DOUBLE` everywhere
(`ut99:UnGnuG.h:245`, its `UnUnix.cpp` `GSecondsPerCycle = 1.0 / SDL_GetPerformanceFrequency()`).
Readers (≈90 in Engine/Render) multiply cycle deltas for ms stats (`UnLevTic.cpp:811-928`, `UnLevel.cpp:1341-1350`,
`Render/Src/UnRender.cpp:226-732`, `UnMeshRnLOD.h:755`) — any float type works.
**ACTION (g.2):** one type in all four declarations. `DOUBLE` is what the template validated (Linux SDL frequency is 10⁹ Hz;
`1e-9f` is fine as a float too, but the template's `UnUnix.cpp` is written against DOUBLE). If kept FLOAT, change the template
`UnGnuG.h` line instead. Either way `Core.h:437`, `Core.cpp:143`, `UnGnuG.h` must agree or the build fails with
"conflicting declaration". `appCycles()` (`UnFile.h:252` `DWORD`): the template's SDL-counter truncation (`ut99:UnUnix.cpp:912-920`) —
`clock()/unclock()` (`UnFile.h:182-183`) only use differences, so 32-bit wrap is harmless.

### g.3 `UnStat.h` (Core) and `UnStats.h` (Engine)
- **`Core/Inc/UnStat.h`** (169 lines, HP2's much-extended v400-style stats): `#if STATS` (`UnBuild.h:56 #define STATS 1`) →
  `class FClock` (RAII `clock()/unclock()`, 22-38) and `struct CORE_API FStats` (40-156) — mesh/particle/decal/skeletal counters and
  EARI blocks (`FName EARIActorNames[64]; FName EARIActorTags[64]; INT EARISubActors[64]; …`, 146-155); `extern CORE_API FStats GStat;`
  (157), defined `Core.cpp:177`. Included by `Core.cpp:7`, `FMallocAnsi.h:9`, `FMallocWindows.h:23`, `Render/Inc/Render.h:20`,
  `Engine/Src/UnParticleFX.cpp:21`, `Editor/Src/EdHook.cpp:38`, `UnEdCam.cpp:12`. Nothing asm; compiles as-is.
  - `GStat` is a global with `FName` members constructed before `FName::StaticInit` — fine because `FName()` is trivial.
  - `clock(Timer)`/`unclock(Timer)` are **function-like macros** that shadow libc `clock()` (`UnAnsi.cpp:13-14` `#undef`s them).
    Any new TU (e.g. `UnFileStream.cpp`, the template's `UnUnix.cpp` if it calls `clock()`) that includes `Core.h` then calls libc
    `clock()` must `#undef clock` — same trap as the template.
  - **HP2's `FMallocAnsi.h` wraps every `Malloc/Realloc/Free` in `Clock(GStat.MemTime)`** (lines 21, 31, 54; the template's does not) →
    two `appCycles()` = two `SDL_GetPerformanceCounter()` (web: `emscripten_get_now()` → `performance.now()`) per allocation. Correct but a
    measurable cost on wasm at load time. **ACTION:** keep for native; for the web consider `#if !__EMSCRIPTEN__` around the three
    `Clock(GStat.MemTime)` lines (measure first). Do **not** set `STATS 0` globally: `GStat.*` is referenced outside `STAT()` wrappers
    (Render, UnParticleFX), which would then fail to compile.
- **`Engine/Inc/UnStats.h`** (210 lines): a later-engine `class ENGINE_API FStats` (TArray-based, CSV) whose name **clashes** with Core's
  `struct FStats` — **nothing includes it** (`grep -rn UnStats.h Source` → only its own banner line). **ACTION:** leave it unbuilt and
  out of every include; do not "fix" it.

## (h) The FFileManager family and case-insensitive lookup

### h.1 Which manager runs where
| Header | Back end | Instantiated by | For us? |
|---|---|---|---|
| `Core/Inc/FFileManagerLinux.h` (406 lines) | `fopen/fread`, `opendir/readdir`, `chmod/mkdir/rmdir/chdir/getcwd` | the launcher: the template's `Game/Src/Launch.cpp` (`#include "FFileManagerLinux.h"`, `FFileManagerLinux FileManager;`) on every GCC target incl. MinGW; HP2's own `UCC/Src/UCC.cpp:78-90` picks it only `#elif __LINUX_X86__` (never true with the template header → falls to Ansi) | **yes — the one and only** |
| `Core/Inc/FFileManagerAnsi.h` | `fopen` + MSVC `_findfirst/_findnext`, `TCHAR_CALL_OS(_wfopen…)` | HP2 `UCC.cpp` fallback | no (MSVC-only APIs) |
| `Core/Inc/FFileManagerArc.h` | in-memory `.uxx` archive over another manager (`ARCHIVE_MAGIC 0x9fe3c5a3`) | `Editor/Src/UMasterCommandlet.cpp` only | no |
| `Core/Inc/FFileManagerWindows.h` | Win32 API | `Launch`, `UnrealEd`, `PackageTool`, `DebuggerLaunch`, `EdHook.cpp` | no |
| `Core/Inc/FFileManagerPSX2.h`, `FFileManagerPSX2CD.h` | `sceOpen`… | `UnPSX2.cpp` | no |
| `Core/Inc/FFileManagerGeneric.h` | base: `FileSize` via reader, `Move/Copy`, `MakeDirectory(Tree)`, and — **HP2 addition** — a real `GetGlobalTime` = `stat().st_mtime` (`:129-137`) | base of Linux/Ansi/Windows | yes |

`Core/Src/UnUnix.cpp` instantiates **none** (v400 convention: the launcher owns the manager and passes it to `appInit`,
`UnMisc.cpp` `GFileManager = InFileManager;`; `Core.cpp:42-71` holds an `FFileManagerError` placeholder until then).
433 adds a virtual `FFileManager::Init(UBOOL Startup) {}` (`Core.h:340`) — non-pure, so the template's manager needs nothing.

### h.2 HP2's `FFileManagerLinux.h` vs v400 vs the template
`diff --strip-trailing-cr -w epic HP2` — exactly two HP2 changes:
1. `FArchiveFileWriter` ctor (`FFileManagerLinux.h:139-142`) gains `ArIsSaving = ArIsPersistent = 1;` (v400's Linux writer left
   both 0 — a Loki-era bug fix; the Windows writer always set them). **The template does not have this fix** (`ut99:FFileManagerLinux.h:140`
   `{}`); it matters to anything that checks `Ar.IsSaving()` on a raw file writer (e.g. `appSaveStringToFile`-style archives and HP2's
   persistent-actor writer below uses raw `Serialize`, so it does not care).
2. **HP2 deleted v400's `GetGlobalTime(){ return 0; }` / `SetGlobalTime(){ return 0; }` overrides** (epic `:256-269`) and moved a real
   `stat()`-based `GetGlobalTime` into `FFileManagerGeneric.h:129-137`:
   ```cpp
   SQWORD FFileManagerGeneric::GetGlobalTime( const TCHAR* Filename )
   {	struct stat buffer;
   	if (stat( appToAnsi(Filename), &buffer ) != 0) return 0;
   	return buffer.st_mtime; }
   ```
   The template kept v400's `return 0` (`ut99:FFileManagerLinux.h:266-281`).
   Callers (Core/Engine): `UnMisc.cpp:1919` and `:1938` (appInit: regenerate `Game.ini`/`User.ini` when `Default.ini`/`DefUser.ini` is newer, §i)
   and the **HP2 persistent-actor save system** `Engine/Src/UnLevel.cpp:278-345` (save) / `437-500` (load):
   ```cpp
   MapFilename = GSys->MapPath * FinalMapName;
   PAFilename  = GSys->SaveSlotPath * TEXT("cache") * FinalMapName.Left(…) + TEXT("_pa.") + URL.DefaultSaveExt;
   …  FileTime = GFileManager->GetGlobalTime( *MapFilename );  pActAr->Serialize(&FileTime, sizeof(FileTime));      // save
   …  IF_TRUE_MSG_AND_BREAK(FileTime != GFileManager->GetGlobalTime(*MapFilename), TEXT("…FileTime that this PA file is for!") ) // load
   ```
   On the web every preloaded/fetched MEMFS/WasmFS file gets the session's time as mtime → every persistent-actor cache written in a
   previous session is **rejected** (game state of already-visited maps silently resets), and every user ini is regenerated from
   `Default.ini` each boot (user settings lost).
   Other template differences (the template is the better file): `#include <sys/stat.h>`, `<cerrno>`; `S_IRUSR|S_IWUSR` instead of the
   glibc-internal `__S_IREAD|__S_IWRITE` (HP2 `:234,254`); `mkdir(Path, __S_IREAD && __S_IWRITE && __S_IEXEC)` (HP2 `:263`, **logical AND →
   mode 1**, a v400 bug) → `S_IRUSR|S_IWUSR|S_IXUSR` and no mode on Win32; `getcwd` failure fallback.

**ACTION (h.2):** take the template's `FFileManagerLinux.h`, re-add HP2's `ArIsSaving = ArIsPersistent = 1;`, and keep the template's
`GetGlobalTime → 0` override **at least on `__EMSCRIPTEN__`** (natively, returning the real mtime is HP2 behaviour and harmless;
returning 0 everywhere is simpler and equally valid for a port whose content never changes — choose 0 everywhere unless a reason
appears). Delete or keep `FFileManagerGeneric`'s stat version — it is overridden either way.
Also note the PA file format writes raw `TCHAR` strings (`sizeof(TEXT("PA0"))`, `(appStrlen+1)*sizeof(TCHAR)`, `UnLevel.cpp:309-315`) —
**retail Windows `*_pa.*` caches (UTF-16) are not readable by the ANSI port** (they fail the "PA0"/map-name check and are ignored,
not misparsed — verify with a retail save if save import is ever offered).

### h.3 Case sensitivity — FFileManagerLinux is exact-case; HP2 content is mixed-case
`FFileManagerLinux::CreateFileReader` = `fopen`; `FindFiles` compares `d_name` with `appStrcmp`/`appStrncmp`/`appStrstr` — **no
case-insensitive matching anywhere** in HP2's (or v400's) Unix file layer. `appFindPackageFile` (`UnMisc.cpp`, v400 text) tries
`GFileManager->FileSize(Path+In+Ext)` per `Paths=` entry → exact case on Linux and on Emscripten MEMFS/WasmFS.

Mismatches found in the shipped content (package strings vs `ls Assets/*`):
| Referenced as | Where | On disk |
|---|---|---|
| `Startup.unr` | `Config/System/Default.ini:6 LocalMap=Startup.unr` | **`Assets/Maps/startup.unr`** |
| `startup.unr` | `HGame.u`: `ChangeLevel("startup.unr",false)` | `startup.unr` (matches) |
| `potions`, `rictusempra` (lower-case imports) | name table of `Assets/Maps/Ch1Rictusempra.unr` (`strings | grep -ix`) | `Assets/Textures/Potions.utx`, `Rictusempra.utx` |
| `hp_fx` | import in `Adv1Willow.unr` | `Assets/Textures/HP_FX.utx` |
| `adv1Willow.unr`, `ch6wizardcard.unr`, `Grounds_Hub.unr`, `grandstaircase_hub.unr`, `grounds_night.unr`, `quidditch.unr` | string table of `Assets/System/HGame.u` (both spellings appear) | `Adv1Willow.unr`, `Ch6WizardCard.unr`, `Grounds_hub.unr`, `Grandstaircase_hub.unr`, `Grounds_Night.unr`, `Quidditch.unr` |
| `Ch1Rictusempra.unr` | HGame.u | `Ch1Rictusempra.unr` (matches) |
| `..\music\<Song>.ogg` | `ALAudio/Src/ALAudioSubsystem.cpp:1136-1140` (raw `fopen`, not via `appFindPackageFile`) | `Assets/Music/*.ogg` |
| `../save/*.usa` | `Default.ini:50 Paths=../save/*.usa` | `Assets/Save` |
| `default.ini` | `UnMisc.cpp:1960` (`new FConfigCacheIni` path, §i.2) | `Default.ini` |
| `GSys->MapPath * FinalMapName` | `UnLevel.cpp:278,437` (PA cache; `FileSize`/`GetGlobalTime` of the map by the URL's spelling) | exact case of the URL — mismatches give -1/0 on both save and load, so the check still passes (consistent, harmless) |

**Template solution** (`ut99:Core/Src/UnMisc.cpp:1543-1700`): `static UBOOL appFindFileNoCase(Dir, In, Ext, Out)` lists `Dir` via
`GFileManager->FindFiles("*"+Ext)` and compares with `appStricmp` (logs `Case-insensitive search: %s -> %s`), called inside
`appFindPackageFile` **per `Paths` entry right after the exact attempt** (`ut99:UnMisc.cpp:1672`; the ordering comment explains
why a global case-insensitive pass after all exact passes picks the wrong file). HP2's `appFindPackageFile` is v400 text → the hunk applies.
On the web the glue is additionally lenient (ufrontend FetchFS/WasmFS manifests are keyed by lower-case path), but native needs the engine fix.

**ACTIONS (h.3):** (1) port `appFindFileNoCase` + the per-path retry into HP2's `appFindPackageFile`; (2) in the NOpenALDrv `PlayMusic`
port build `".." PATH_SEPARATOR "Music" PATH_SEPARATOR + Song + ".ogg"` and resolve it case-insensitively (or add `Paths=../Music/*.ogg`
and use `appFindPackageFile`); (3) `LocalMap=startup.unr` is resolved by `appFindPackageFile` once (1) lands — no ini change needed, but the
web ini may as well use the disk spelling; (4) `Default.ini` spelling at `UnMisc.cpp:1960` (§i.2); (5) `Paths=../Save/*.usa` (§i.3).

## (i) Config — `appInit`, ini names, `Default.ini`/`DefUser.ini`, `appUserDir`, save paths

### i.1 What ini name `appInit` derives
`Core/Src/UnMisc.cpp:1845` `appInit( const TCHAR* InPackage, const TCHAR* InCmdLine, FMalloc*, FOutputDevice*, FOutputDeviceError*,
FFeedbackContext*, FFileManager*, FConfigCache*(*ConfigFactory)(), UBOOL RequireConfig )` — v400 signature. Then (`:1912-1952`):
```cpp
TCHAR GIni[512]=TEXT("");
if( !Parse( appCmdLine(), TEXT("INI="), GIni, 256 ) )
	appSprintf( GIni, TEXT("%s%s.ini"), appUserDir(), InPackage );
bool bDefaultIniChanged = false;
if(   RequireConfig
   && (   GFileManager->FileSize(GIni) < 0
       || GFileManager->GetGlobalTime(TEXT("Default.ini"))  >  GFileManager->GetGlobalTime(GIni) //ft
      ) )
{	FString S;
	if( !appLoadFileToString( S, TEXT("Default.ini"), GFileManager ) )
		appErrorf( LocalizeError("MisingIni"), "Default.ini" );
	appSaveStringToFile( S, GIni );
	bDefaultIniChanged = true; }
TCHAR GUserIni[512]=TEXT("");
if( !Parse( appCmdLine(), TEXT("USERINI="), GUserIni, 256 ) )
	appSprintf( GUserIni, TEXT("%sUser.ini"), appUserDir() );
if( RequireConfig && ( FileSize(GUserIni) < 0 || GetGlobalTime("DefUser.ini") > GetGlobalTime(GUserIni) || bDefaultIniChanged ) )
{	… appLoadFileToString( S, TEXT("DefUser.ini"), GFileManager ) … appSaveStringToFile( S, GUserIni ); }
GConfig = ConfigFactory();
GConfig->Init( GIni, GUserIni, RequireConfig );
```
So the system ini is **`<appUserDir()><InPackage>.ini`** and the user ini **`<appUserDir()>User.ini`**, seeded from `Default.ini` /
`DefUser.ini` in the working directory (`System/`), overridable with `INI=` / `USERINI=`. Two 433 behaviours differ from v400:
(1) **ini regeneration by mtime** (`//ft` lines) — with the `GetGlobalTime → 0` override (§h.2) the comparison is `0 > 0`, i.e. only
"file missing" regenerates (template behaviour); (2) **`User.ini` is regenerated whenever `Game.ini` was** (`bDefaultIniChanged`).
`GIni`/`GUserIni` are stack arrays passed to `GConfig->Init`, which copies them (`FConfigCacheIni.h` `Init` stores `FString`s) — fine.

`InPackage`: the retail Win32 launcher does `appStrcpy( GPackage, appPackage() );` (`Launch/Src/Launch.cpp:83`) and
`appInit( GPackage, … )` (`:141`) — the exe's base name, so retail = **`Game.ini`** (`System/Game.exe`) + `Game.log`. HP2's UCC passes
`TEXT("HP")` (`UCC/Src/UCC.cpp:200` → `HP.ini`). The template launcher copied into HP2 still has
`appInit( TEXT("UnrealTournament"), … )` (`Game/Src/Launch.cpp:883`).
**ACTION (i.1):** `TEXT("Game")` in `Game/Src/Launch.cpp:883` (literal, not `appPackage()` — on Unix `appPackage()` returns `GModule`
from `argv[0]`, which can carry a path); UCC keeps `"HP"` or also uses `"Game"` (decide in the launcher/UCC report; retail
`ucc.exe` used `HP.ini`).
`appUserDir()` exists only in `UnVcWin32.cpp:1048-1070` (`SHGetFolderPathW(CSIDL_PERSONAL)` + `"\\Harry Potter and the Chamber of Secrets\\"`,
`CreateDirectory`) — retail kept `Game.ini`, `User.ini`, `Game.log` (`FOutputDeviceFile.h:42`), screenshots (`UnCamera.cpp:665,899`),
replays (`UnReplay.cpp:40`) and `Save\` in *My Documents*. **ACTION:** Unix/web `appUserDir()` returns `TEXT("")` — everything lands in
`System/` (and `../Save`), matching the template's run tree and the web MEMFS layout the frontend persists.

### i.2 The second config cache on `default.ini` — boot-breaker on a case-sensitive FS, plus a garbage read
`UnMisc.cpp:1955-1964`:
```cpp
	// gk 10/15/01
{
	TCHAR Temp[256];
	TCHAR DTemp[256];
	FConfigCache* DConfig=ConfigFactory();
	DConfig->Init(TEXT("default.ini"),GUserIni,RequireConfig);
	GConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), Temp, ARRAY_COUNT(Temp) );
	DConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), DTemp, ARRAY_COUNT(DTemp));
	GConfig->SetString( TEXT("Engine.Engine"), TEXT("Language"), DTemp );
}
```
Intent: re-impose `Default.ini`'s `Language` (set by the installer) on `Game.ini` every boot. On Linux/web `default.ini` does not exist
(the file is `Default.ini`) → `GetString` fails and returns without writing `DTemp` → **uninitialised stack bytes become `Language`**
→ `UObject::SetLanguage(garbage)` (`:1968-1969`) → every `Localize()` misses (and `Suppress=Localization`, `Default.ini:64`, hides the
warnings) → the whole UI shows `<?int?…?>` keys. Also `DConfig` leaks. (A missing file does not assert: `FConfigCacheIni::Init`, `FConfigCacheIni.h:242`, ignores
`RequireConfig` — checked, the only occurrence of the word in the header — so the failure is silent.)
**ACTION (i.2):** `TEXT("Default.ini")`; `DTemp[0]=0;` and `if( DConfig->GetString(…) && *DTemp ) GConfig->SetString(…);`; `delete DConfig;`.

### i.3 Save path, save slots, `Paths[5]` and the "last `Paths=`" coupling, `MapPath`
`UnMisc.cpp:1999-2067` (433 additions after `appPlatformInit()`), quoted:
```cpp
if( *appUserDir() )
{	GSys->SavePath = FString(appUserDir()) * TEXT("Save");
	GSys->Paths[5]=*(GSys->SavePath * TEXT("*.usa")); }            // hard-coded index 5
else
{	GSys->SavePath = TEXT("..\\Save"); }                            // literal backslash
GFileManager->MakeDirectory( *GSys->SavePath, 0 );
int iSlotNum;
if( Parse( appCmdLine(), TEXT("SAVESLOT="), iSlotNum ) ) appSetSaveSlotDir( iSlotNum );   // SavePath * "Slot" + N (1834-1841)
else GSys->SaveSlotPath = GSys->SavePath;
if( *GSys->SaveSlotPath )
{	debugf( TEXT("Save Slot Path: %s"), GSys->SaveSlotPath );      // ← FString BY VALUE through varargs (line 2030)
	GFileManager->MakeDirectory( *GSys->SaveSlotPath, 0 );
	GFileManager->MakeDirectory( *(GSys->SaveSlotPath * TEXT("cache")), 0 ); }
FString FileExt = TEXT(".unr");
for(int i=0; i<GSys->Paths.Num(); i++ )
{	if( GSys->Paths(i).Right( FileExt.Len() ) == FileExt )
	{	for( const TCHAR* p = *GSys->Paths(i) + GSys->Paths(i).Len() - 1; p >= *GSys->Paths(i); p-- )
		{	if( *p=='/' || *p=='\\' || *p==':' ) break; }
		GSys->MapPath = appBaseDir();
		GSys->MapPath = GSys->MapPath.Left( GSys->MapPath.InStr(TEXT("System")));
		GSys->MapPath += GSys->Paths(i).Left( p - *GSys->Paths(i)).Right(4);      // ← uses p after its for-scope
		break; } }
LoadPersistentActorCache();
GConfig->SetString( TEXT("Core.System"), TEXT("SavePath"), *GSys->SaveSlotPath );
GConfig->SetString( TEXT("Core.System"), TEXT("paths"), *(GSys->SaveSlotPath * TEXT("*.usa")) );
GConfig->Flush( 0 );
```
Hazards, in boot order:
| # | Hazard | Effect | ACTION |
|---|---|---|---|
| 1 | `SavePath = TEXT("..\\Save")` | on Linux/web `MakeDirectory` creates a directory literally named `..\Save` inside `System/`; saves go there | `TEXT("..") PATH_SEPARATOR TEXT("Save")` |
| 2 | `debugf( TEXT("Save Slot Path: %s"), GSys->SaveSlotPath )` (**line 2030**) | non-POD `FString` through `...`: gcc passes non-trivially-copyable objects by invisible reference, so `%s` prints the bytes of the `FString` object — **garbage in the log** (verified in the scratchpad: `va2.cpp`, gcc-15 `-m32`, output `[\016\300uc\001]` for "hello"; a string without a NUL nearby would read further); **clang/em++ emits a trap → the web build aborts inside `appInit`** (diagnostic hidden by `-Wno-non-pod-varargs`, §d.3) | `*GSys->SaveSlotPath` |
| 3 | `for( const TCHAR* p = …; …; p-- ) {…}` then `p` used after the loop | ISO for-scope: **hard compile error** on modern GCC/clang (missed by the heuristic scan in §d.3 because of the `const` qualifier) | hoist `const TCHAR* p;` before the `for` |
| 4 | `GSys->Paths[5]` (only when `appUserDir()` is non-empty) | assumes ≥6 `Paths=` entries and that #5 is the save path | unreached with `appUserDir()==""`; if ever enabled, look the entry up by `*.usa` instead |
| 5 | `SetString( "Core.System", "paths", SaveSlotPath*"*.usa" )` | `FConfigSection` is a case-insensitive `TMultiMap` (`GetTypeHash(FString)` = `appStrihash`, `UnTemplate.h:1317-1319`); `TMapBase::Add` pushes new pairs at the head of the hash chain (`UnTemplate.h:1506-1507`, and `Rehash` rebuilds in array order, `:1485-1490`) so `Find("paths")` returns the **last** `Paths=` line → this overwrites the **last `Paths=` entry** of `Game.ini` with the save path | every ini (shipped `Default.ini:45-50`, `Config/System/web/*`) must keep **`Paths=../Save/*.usa` as the LAST `Paths=` line** — adding e.g. `Paths=../Music/*.ogg` after it would silently replace that path at the first boot. Document this in the web ini. |
| 6 | `MapPath` = `appBaseDir()` up to the first `"System"` + the last 4 chars of the `.unr` path's directory (`"../Maps"` → `"Maps"`) | requires the base dir to contain `System` (native run tree `…/System/`, web `/System/` — OK) and a 4-letter map dir; used only by the persistent-actor cache (§h.2) | none; keep the directory names |
| 7 | `Default.ini:50 Paths=../save/*.usa` (lower-case) vs disk `Assets/Save` | exact-case miss for save *loading* by `appFindPackageFile` until §h.3's fix; and after hazard 5 the path is rewritten to `../Save/*.usa` anyway | use `../Save/*.usa` in the web/dev inis |
| 8 | `LoadPersistentActorCache` / `SavePersistentActorCache` (`UnMisc.cpp:2072-2140`) move `*_pa.*` between `SaveSlotPath` and `SaveSlotPath/cache` with `FindFiles/Delete/Copy` | portable; on the web these writes must land in the persisted save store | ensure the frontend persists `Save/` recursively (incl. `cache/`, `SlotN/`) |

### i.4 `FConfigCacheIni` itself
`Core/Inc/FConfigCacheIni.h` (398 lines): `Init(InSystem, InUser, RequireConfig)` stores the names; `Find(Filename)` loads lazily via
`appLoadFileToString`; lookups are `FString`-keyed `TMultiMap`s (case-insensitive). No `_MSC_VER`, `TCHAR_CALL_OS` or Win32 call
inside — portable as-is; `appSaveStringToFile` writes 8-bit bytes when `sizeof(TCHAR)==1`, so `Game.ini`/`User.ini` stay ANSI.
`USystem` (`Core/Inc/UnCorObj.h:130-157`) carries HP2 members `INT LicenseeMode` (Legend, 4/12/2000; not a config property, stays 0 —
see §k), `FString SaveSlotPath`, `FString MapPath` in addition to v400's; its ctor/`StaticConstructor` live in the platform file and HP2's
`UnUnix.cpp:64-86` text is identical to the template's and to `UnVcWin32.cpp:134-156` (config props: `PurgeCacheDays`, `SavePath`,
`CachePath`, `CacheExt`, `Paths`, `Suppress`). **ACTION:** none — the template's `UnUnix.cpp` USystem block works with HP2's header.

## (j) `GLazyLoad` and the `Suppress=` list
- **`GLazyLoad`**: `Core.cpp:164 CORE_API UBOOL GLazyLoad=0;` (v400 default). Readers: `UnTemplate.h:1007,1031` (`TLazyArray`
  load/serialise), and two force-lazy windows `Engine/Src/UnTex.cpp:326-330` and `UnFont.cpp:25-34`. Setters: retail
  `Launch/Src/Launch.cpp:148` `GLazyLoad = !GIsClient || ParseParam(appCmdLine(),TEXT("LAZY"));`, the template launcher
  `Game/Src/Launch.cpp:891-893` (same idiom), HP2 `UCC.cpp:226,331`. So a client loads `TLazyArray`s **eagerly** unless `-LAZY` —
  identical policy to UT99; the web build of the template runs eager. **ACTION:** none. (If wasm heap becomes the constraint, `-LAZY`
  is the one-flag experiment: lazy arrays then re-open the package file via `GFileManager` at first use, which works on MEMFS.)
- **`Suppress=`**: `USystem::Suppress` (`UnCorObj.h:151 TArray<FName> Suppress;`) is a `CPF_Config` array (`UnUnix.cpp:81-82`);
  `appPlatformInit` applies `GSys->Suppress(i).SetFlags( RF_Suppress )` (`UnUnix.cpp:652-653`; template identical), which drops
  `debugf(NAME_x, …)` for those names. Shipped list (`Config/System/Default.ini:51-64`): `DevLoad DevSave DevNetTraffic DevGarbage DevKill
  DevReplace DevSound DevCompile DevBind DevBsp ScriptWarning Localization` (`ScriptLog`, `Log` commented out).
  **ACTION:** keep for release inis; in the dev/first-boot ini comment out `Suppress=Localization`, `Suppress=DevLoad`, `Suppress=DevBind`
  and `Suppress=ScriptWarning` — they hide precisely the symptoms of §h (package not found), §i.2 (`<?int?>`), §f (native bind) and
  433-bytecode mismatches.

## (k) Package versions — the runtime `PACKAGE_FILE_VERSION`, the shipped header bytes, the loader checks

### k.1 Where the version lives
`Core/Inc/UnObjVer.h`: `ENGINE_MIN_NET_VERSION 400`, `ENGINE_VERSION 433`, `ENGINE_NEGOTIATION_VERSION 400`, `PACKAGE_FILE_TAG 0x9E2A83C1`,
the Legend "LVer" block (`PACKAGE_FILE_VERSION_LICENSEE 0x00`, `VERSION_ENGINE_ONLY 0x00`: "Uses the upper two words of the VersionNum
DWORD to encode a licensee-specific version WORD"), and — instead of v400's `#define`s —
```cpp
extern CORE_API INT PACKAGE_FILE_VERSION;
extern CORE_API INT PACKAGE_MIN_VERSION;
```
Defined once, `Core/Src/Core.cpp:13-14`:
```cpp
CORE_API INT PACKAGE_FILE_VERSION = 79;
CORE_API INT PACKAGE_MIN_VERSION = 60;
```
No other assignment in the tree (`grep -rn 'PACKAGE_FILE_VERSION\s*=\|PACKAGE_MIN_VERSION\s*=' Source`). Consumers:
`ULinkerLoad` ctor seeds `ArVer = PACKAGE_FILE_VERSION; ArLicenseeVer = PACKAGE_FILE_VERSION_LICENSEE;` (`UnLinker.cpp:183-184`), then overrides
from the file (`:194-195` `ArVer = Summary.GetFileVersion(); ArLicenseeVer = Summary.GetFileVersionLicensee();`); `ULinkerSave` writes
`Summary.SetFileVersions( PACKAGE_FILE_VERSION, PACKAGE_FILE_VERSION_LICENSEE )` (`:896`), and `UnLinker.h:132-134`:
```cpp
INT GetFileVersion() const { return (FileVersion & 0xffff); }
INT GetFileVersionLicensee() const { return ((FileVersion >> 16) & 0xffff); }
void SetFileVersions(INT Epic, INT Licensee) { if( GSys->LicenseeMode == 0 ) FileVersion = Epic; else FileVersion = ((Licensee << 16) | Epic); }
```
`GSys->LicenseeMode` is not a config property (§i.4) → 0 → saves are written as plain `79`. Being a variable rather than a macro, any code
the port brings from the template that does `#if PACKAGE_FILE_VERSION >= …` would silently evaluate to 0 in the preprocessor —
**ACTION (k.1):** grep every template-imported file for `#if.*PACKAGE_(FILE|MIN)_VERSION` before merging (the IpDrv/WebRTCDrv copies
and XOpenGL use `ENGINE_VERSION`, which is still a macro; verify: `grep -rn 'if.*PACKAGE_FILE_VERSION' Source/{IpDrv,WebRTCDrv,NSDLDrv,NullDrv,NOpenALDrv,Game,UCC}`).

### k.2 What the shipped packages carry (header bytes read in this session)
Layout: `DWORD Tag` · `WORD Version` · `WORD Licensee` · `DWORD PackageFlags` · `DWORD NameCount`. Examples (`xxd -l 16`):
```
System/Core.u              c1 83 2a 9e 4f 00 00 00 01 00 00 00 4c 01 00 00   → tag ok, v79, lic 0, flags 1, 332 names
System/HGame.u             c1 83 2a 9e 4f 00 00 00 01 00 00 00 dc 24 00 00   → v79, lic 0, flags 1, 9436 names
Maps/Ch1Rictusempra.unr    c1 83 2a 9e 4f 00 00 00 01 00 00 00 f6 12 00 00   → v79, lic 0, flags 1, 4854 names
System/UWindow.u           c1 83 2a 9e 4f 00 00 00 05 00 00 00 95 05 00 00   → v79, lic 0, flags 5 (AllowDownload|ServerSideOnly)
```
Census over every file under `Assets/` whose first DWORD is the tag (script in session; 212 packages):
| Version | Count | Files |
|---|---|---|
| **79** | 132 | all 11 `System/*.u`, every shipped gameplay map except two, most `.utx`/`.uax` |
| **76** | 75 | **`Maps/Entry.unr`** (loaded at every boot, `UnGame.cpp:169`) and **`Maps/Greathall.unr`**; textures `Dungeon, FindHermione, FlipendoBlocks, GenFX, HPEdit, HP_1st, HP_Basic, HP_Logo, HP_Scripted, HP_Titles, Intro_CS, MenuArt.utx` (+13 localised `MenuArt.*_utx`), `Potions, SkyBox, TomRiddle, Transfig_Tex, Wizard, WizardDuel`; all of `Maps/Oldfiles`, `Maps/Studies`, `Maps/templates` (dev leftovers) |
| **69** | 1 | `Textures/Particles.utx` |
| **68** | 2 | `Textures/FlareFX.utx`, `Textures/UWindowFonts.utx` |
| **61** | 2 | `Textures/Detail.utx`, `Textures/Palettes.utx` |
Licensee WORD is **0 in every file**. (One non-package with a package extension was not found; the only other tagless file seen was a BMP.)
So the content is **not** single-version: the loader's legacy branches run in normal play — `FNameEntry` `Ar.Ver()<64` ANSI-bytes path
(`UnName.cpp:235-247`) for the two v61 textures, the `FUnrealfileSummary` `GetFileVersion()<68` heritage branch (`UnLinker.h:148-…`)
for v61, and whatever `<69`/`<76`/`<79` branches exist in `UTexture`/`UFont`/`UModel`/`ULevel` serialisers for the boot map `Entry.unr`.
This is the same situation as UT99 (v61–v69 content) the template already runs, and it is why the template's no-parens
`InternalConstructor` fix (§d.7, found on ≤v61 map loads) matters here too.

### k.3 What the loader accepts
`Core/Src/UnLinker.cpp:202-213`:
```cpp
if( Summary.Tag != PACKAGE_FILE_TAG )
{	GWarn->Logf( LocalizeError("BinaryFormat"), *Filename );
	throw( LocalizeError("Aborted") ); }
…
if( Summary.GetFileVersion() < PACKAGE_MIN_VERSION ) //LVer added by Legend on 4/12/2000
	if( !GWarn->YesNof( LocalizeQuery("OldVersion"), *Filename ) )
		throw( LocalizeError("Aborted") );
```
Accepts tag-correct files with Epic version ≥ 60 (all shipped content: min 61) without prompting; below 60 it asks via `YesNof` (on the
web/Ansi feedback context that would block or auto-answer — never reached with this content). There is no upper-bound check (a v80+ file
would be parsed as v79). The template's web-build patches around `OldVersion`/`version mismatch` (fail-soft `return NULL` instead of
`throw`, `ut99:Core/Src/UnObj.cpp:1996-2131`) target HP2's split file `UnLinker.cpp` (433 moved the linker out of `UnObj.cpp`).

**Exception-type hazard in the same files** (GCC is strict where MSVC 6 was lax): `throw( LocalizeError("Aborted") )` throws a
`const TCHAR*`, `appThrowf` throws a `TCHAR*`. Handlers: `UnLinker.cpp:298-306`
```cpp
// !! PSX2 gcc doesn't like catch( TCHAR* Error )
#if _MSC_VER
catch( TCHAR* Error )
#else
catch( char* Error )
#endif
{ GObjLoaders.RemoveItem( this ); throw( Error ); }
```
and `Engine/Src/UnGame.cpp:961-965` (`catch( TCHAR* CatchError )` / `catch( char* CatchError )`). A `char*` handler does **not** match a
`const char*` exception → on GCC a nested "BinaryFormat/Aborted" or `LOAD_Throw` error from `VerifyImport` skips the cleanup
(`GObjLoaders` keeps a dangling half-verified linker) and escapes to an outer `catch( const TCHAR* )` (`UnObj.cpp:2256,2380,2416,2444,2583`).
**ACTION (k.3):** both sites → `catch( const TCHAR* Error )` (a `const` handler also catches the non-const `appThrowf` throws); rethrow with
`throw;`. Then apply the template's web fail-soft pattern to HP2's `UnLinker.cpp`/`UnObj.cpp` throw sites (owned by the core-merge work,
see `template-core.md`).

### k.4 Net version
`ENGINE_MIN_NET_VERSION 400` / `ENGINE_NEGOTIATION_VERSION 400` / `ENGINE_VERSION 433`: `UnLevel.cpp:1187`
`Connection->NegotiatedVer = Min(RemoteVer,ENGINE_VERSION);`. Both WebRTC peers run the same binary, so negotiation is moot.
**ACTION:** none.

## (l) Everything else that will bite

### l.1 Functions HP2 Core/Engine call that only `UnVcWin32.cpp` defines (the Unix core must gain them)
| Function | Declared | Unix definition today | Callers in our build | Template has it? | ACTION |
|---|---|---|---|---|---|
| `const TCHAR* appUserDir()` | `UnFile.h:235` | **none** (`UnVcWin32.cpp:1048`) | `UnMisc.cpp:1915,1935,2002,2005`, `FOutputDeviceFile.h:42` (log path), `UnCamera.cpp:665` (`%sShot%04i.bmp`), `:899`, `UnReplay.cpp:40` (`appUserDir()+InName+".rep"`) | no | `return TEXT("");` — every join is empty-safe (`FString::operator*=` only adds a separator when the left side is non-empty, `UnTemplate.h:1152-1157`) |
| `FTime appProcessSeconds()` | `UnFile.h:263` | **none** (`UnVcWin32.cpp:501`, `GetProcessTimes`) | `UnGame.cpp:843,1465`, `Render/Src/UnRender.cpp:703` | no | `return appSeconds();` (§g.1) |
| `FTime appSeconds()`, `appSecondsSlow()` | `UnFile.h:256-262` | only the `ASMLINUX` inline in HP2's `UnGnuG.h` (removed) | everywhere | DOUBLE versions | §g.1 |
| `bool IsOSVer2kOrXP()` | `Engine/Src/UnScript.cpp:2841` (local `CORE_API` decl) | **none** (`UnVcWin32.cpp:1211`) | native 327 `AActor::execIsOSVer2kOrXP` (`UnScript.cpp:2844-2849`, `*(bool*)Result = IsOSVer2kOrXP ();`), script `Engine/Classes/Actor.uc:1664`; no `.uc` caller in the source drop | no | `CORE_API bool IsOSVer2kOrXP(){ return true; }` in the Unix core (without it: undefined reference at link). Note the 1-byte `bool` write into a 4-byte UBOOL return slot — same as retail on little-endian, leave |
| `FFileStream::Read/Create×2/Destroy(INT)` | `Core.h:392-395` | **none** (`UnVcWin32.cpp:706-935`) | `UnMisc.cpp` portable half itself | no | §e.2 |
| `void* appCreateProc(URL,Parms,UBOOL bRealTime)`, `UBOOL appGetProcReturnCode(void*,INT*)` | `UnFile.h:82-83` | yes in HP2's `UnUnix.cpp:316-357` (fork/`system`) | `AStatLog.cpp:24,35,45,72,88`, `UnAudio.cpp:584-611` (editor sound-compress path, `#ifdef _WINDOWS`) | template's is v400's `void appCreateProc(URL,Parms)` (`ut99:UnFile.h:78`) | keep HP2's 3-arg `void*` signatures when taking the template's `UnUnix.cpp`; on web return `NULL` / `0` |
| `appMsgf`, `appGetLastError`, `appDebugMessagef` | `UnFile.h:57-60` (declared `const void` — odd but legal) | none | none in Core/Engine | no | omit; add empty stubs only if a driver references them |
| `appSetSaveSlotDir`, `Save/LoadPersistentActorCache`, `appTrimSpaces` | `UnFile.h` | yes (`UnMisc.cpp:1834-1841, 2072-2140`) | `UnGame.cpp` | n/a | none |
(`appMalloc` looks undefined in a naive scan — it is an inline `GMalloc` forwarder in `UnFile.h`; fine.)

### l.2 `UnTemplate.h`: HP2 re-based `TArray` on a *template* base → two-phase-lookup errors everywhere
HP2 (433) split v400's `FArray`/`TArray` into `template<class T> class TRefArray` (`UnTemplate.h:206-352`, members
`protected: T* Data; INT ArrayNum;` at 346-348 — same byte order as v400's `FArray`, so layouts and `sizeof`=12 are unchanged) and
`template<class T> class TArray : public TRefArray<T>` (`:353-681`, adds `INT ArrayMax`), with `class FArray : public TArray<BYTE>` (`:682`).
Inside `TArray<T>` the base is *dependent*, so every unqualified `ArrayNum`/`Data` (≈65 lines in 353-681, e.g. `~TArray(){ … Remove( 0, ArrayNum ); if( Data ) appFree( Data ); … }`)
is an error on GCC (`'ArrayNum' was not declared in this scope`; only `-fpermissive` downgrades it) and on clang (hard error). v400 never
hit this because its `FArray` base was not a template. HP2 has **0** `this->` in `UnTemplate.h`; the template has 20, covering only the
v400 dependent bases (`TTransArray` 683-753, `TLazyArray` 944, `TMultiMap` 1568-1601) — **the template's file cannot be dropped in**
(it lacks `TRefArray`, and 433 code uses `TRefArray` directly, e.g. `UnSkeletalMesh.h:260-263 TRefArray <FAnimVec> KeyQuat; …`).
**ACTION (l.2):** keep HP2's `UnTemplate.h`; add `using TRefArray<T>::Data; using TRefArray<T>::ArrayNum;` (and any base member functions
called unqualified, e.g. `IsValidIndex`, `Num`) at the top of `TArray`; then port the template's `this->` edits for `TTransArray`
(18 candidate lines in 755-903), `TLazyArray`, `TMap`/`TMultiMap` (14 lines in 1626-1692). The TArray `Realloc/Remove` definitions: HP2 has them
inline in the header, the template has them out-of-line in `UnMisc.cpp` — keep exactly one definition (HP2's) to avoid ODR/duplicate symbols
when merging `UnMisc.cpp`. This is the largest mechanical edit of the first compile.

### l.3 Exceptions: `catch( char* )` does not catch `const TCHAR*` — see §k.3 (`UnLinker.cpp:298-306`, `UnGame.cpp:961-965`).

### l.4 Build switches HP2's code keys on, and what retail used
| Switch | Where defined | Retail value (evidence) | Port value |
|---|---|---|---|
| `UNICODE`/`_UNICODE` | `.dsp` `/D` | **on** (`Core.dsp:46`; retail `Assets/System/Core.dll` holds its format strings as UTF-16: `strings -el Core.dll` → `Character set: %s`, `Default.ini`, `DefUser.ini`, `%sUser.ini` and — confirming §i.2 shipped — `default.ini`) | off (§d.6) |
| `STATS` | `UnBuild.h:56` | 1 | 1 (§g.3) |
| `DO_GUARD`, `DO_GUARD_SLOW` | `UnBuild.h` | 1 / 0 (1 under `_DEBUG`) | template values |
| `HPDEMO` | `UnBuild.h:80-83` = 0 | 0 (full game) | 0 (`UnGame.cpp:913` demo map lock) |
| `DEMOVERSION` | `UnBuild.h:76` = 0 | 0 | 0 (`UnGame.cpp:935` UT "DM-TurbineDemo" lock) |
| `LEGEND` | not defined in any `.dsp` or header (`grep -rn LEGEND Source --include=*.dsp` → none) | probably **off** — the `#if defined(LEGEND)` "Render Control Interface" blocks (`UnGame.cpp:1510`, `UnPhysic.cpp:499,718,1522,1902`, `Render/Src/UnRender.cpp` ×9, `UnLight.cpp:2357,2500`) call `APlayerPawn::ClearScreen/RecomputeLighting`, which retail `Engine.dll` exports (`?ClearScreen@APlayerPawn@@UAE_NXZ`) but which are plain virtuals defined regardless; unverified | leave undefined; verify by disassembling retail `Engine.dll` `UGameEngine::Draw` for a call through the `ClearScreen` vtable slot |
| `ANIM48` | not defined | off | off (`UnSkeletalMesh.h/.cpp` alternative 48-bit anim keys) |
| `NO_SOUND_PLEASE` | commented out, `UnEngine.cpp:12` | off | off |
| `HAVE_VAG` | `UnAudio.cpp` | 0 (PS2 VAG) | 0 |
| `__INTEL_BYTE_ORDER__`, `__INTEL__` | `UnGnuG.h` | 1 | 1 (template header defines both, `ut99:UnGnuG.h:16-17`; used by `UnFile.h:19` bitfield order, `UnMesh.h:21`, `UnTex.h:48`, `UnSkeletalMesh.h:248` `TTypeInfo<FAnimVec>::FastSerialize`) |
| `__STATIC_LINK` | Loki/PS2 makefiles | off in retail Win32 | off (§f) |
| `_WINDOWS` | `.dsp` | on | off — gates only `UnAudio.cpp:576,605` (editor `sx.exe` compression) |

### l.5 Headers that embed Windows types
`Engine/Inc/UnDDraw.h` (DirectDraw `DDSURFACEDESC2` et al. for S3TC, own `typedef long LONG; typedef void* LPVOID;`) and `Engine/Inc/S3tc.h` are
included only by `UnTex.cpp:12-16` under `#ifdef WIN32` → must become `#if _MSC_VER` (§d.5) or MinGW compiles them (and then needs `s3tc.lib`).
`Engine/Inc/UnEngineWin.h` (1982 lines, the Win32 launcher body: `CreateThread` splash at 116, `appUserDir()*"Running.ini"`/`"Detected.ini"`
1733/1813, GDI) is included only by `Editor/Src/EdHook.cpp:68` (replaced by `NullEdHook.cpp`) and the Win32 launcher — never by our build.
`Engine/Src/winstuff.cpp` (`__int64 GetTimeIndex()`, no callers) is correctly absent from `Engine/CMakeLists.txt`. `Core/Inc/AudioMutex.h` §e.1.
`Engine/Inc/Palette.h` §d.2.

### l.6 Reviewed and fine (the 433 subsystems the brief named)
- **`UnSkeletalMesh.cpp` (990) / `UnSkeletalMesh.h` (516)**: only `#ifdef ANIM48` (off) and `#if __INTEL_BYTE_ORDER__` (on); `FArchiveCountMem`
  plain; uses `TRefArray` (§l.2) and `appRound` in `FAnimVec::IntVal` (`:242`) — **correct only after the template's `appRound`** (HP2's GCC
  `appRound` truncates, §a.2).
- **`ULodMesh.cpp` (363)**: one `#if !__PSX2_EE__` (233) → keep `appFloor(Frame)` (needs the template's `floorf` `appFloor`).
- **`UnParticle.cpp`/`UnParticleFX.cpp`/`UnParticleList.cpp` + headers**: no platform conditionals, no asm, no Win types; `UnParticleFX.h:19`
  `const int PriorityTable[10]={…}` at namespace scope has internal linkage (no ODR issue); `UnParticleFX.cpp:21` includes `UnStat.h`.
- **`UnGesture.cpp` (485) / `Engine/Inc/UGesture.h`** (the brief's `UnGesture.h` is `UGesture.h`, included *inside* the generated class body at
  `EngineClasses.h:3706`): plain C++, mouse-gesture recogniser fed by script axis input — no platform code.
- **`UnReplay.cpp` (142)**: `GFileManager` I/O under `appUserDir()` — fine with `""`.
- **`UnIn.cpp` (493)**: input bindings; for-scope candidates `:131→137`, `:157→163` are v400-inherited (template fixed).
- **`UnCamera.cpp`**: the two `_MSC_VER` semantic bugs (§d.5); screenshot/`SnapShot` writers use `appUserDir()`.
- **`UnGame.cpp`**: `HPDEMO`/`DEMOVERSION`/`LEGEND` gates (l.4), `catch(char*)` (k.3), `appProcessSeconds` (l.1).
- **`UnEngine.cpp`**: `NO_SOUND_PLEASE` (off); `InitAudio` loads `ini:Engine.Engine.AudioDevice` — the ini, not code, picks the driver.
- **`UnLevel.cpp`**: persistent-actor cache (§h.2), `#if ENGINE_VERSION>230` (true), `WelcomePlayer` `_MSC_VER` signature (§d.5).
- **`UnAudio.cpp`**: `#ifdef _WINDOWS` editor compression (`sx -eaxa_blk`, `check(0)` otherwise), `#if HAVE_VAG`, one `__PSX2_EE__` RIFF walk (§b),
  `#pragma DISABLE_OPTIMIZATION` (22, unmatched — ignored on GCC).
- **`Engine/Inc/UnSoundContainer.h:15-27`**: `#if !__GNUC__ enum ESoundSlot; #endif … #if __GNUC__ int Slot; #else ESoundSlot Slot; #endif` —
  already GCC-aware (4 bytes either way).
- **`Engine/Inc/UnEngineGnuG.h`** (15 lines): defines `ELevelTick` for `__GNUG__` because `UnLevel.h` hides it behind `_MSC_VER`; included by
  `Engine.h:34-36 #if __GNUG__` — works for gcc, MinGW and em++.
- **`Core/Inc/FCodec.h`** (BWT/RLE/Huffman/MTF): pure C++. **`Core/Inc/UnMsg.h`** (46 lines, HP2/U2Ed-era): `#define WM_USER_EXEC (WM_USER + 0 + 923)` … editor window-message ids — needs `<windows.h>`'s `WM_USER`; included only by `Editor/Src/EdHook.cpp:18` (backslash path, not built) and `UnrealEd/Inc/ViewportFrame.h` — never by the game build; leave.
- **`Core/Src/UnGUID.cpp`** (HP2's own): `typedef unsigned long unsigned32;` — 32-bit on i386/wasm32/i686-MinGW (would break on LP64; the port is 32-bit);
  `get_system_time` via `gettimeofday` (the `__PSX2_EE__` arm deleted per §b).

### l.7 wasm-specific footnotes (beyond the template's known web layer)
- `setjmp`-free `guard()` is mandatory (§a.2). With exceptions disabled on the web the template's fail-soft NULL returns replace the
  `throw`s (§k.3).
- Unaligned reads in the script VM (`UnScript.h` non-PSX2 arms, `P_GET_SKIP_OFFSET`, `ReadInt/ReadFloat/ReadName`, `HANDLE_OPTIONAL_DEBUG_INFO`
  `*(INT*)&Script(…)`) are legal on wasm32 (alignment is a hint) and x86.
- `-fpack-struct=4` (HP2 CMake wasm/MinGW blocks, `Source/CMakeLists.txt:79,170`) reproduces MSVC `/Zp4` for `FTime`/`QWORD`/`DOUBLE`
  members; `GCC_PACK(1)` BMP headers in `UnCamera.cpp` are unaffected.
- `fork/system/waitpid` in `appCreateProc` compile against Emscripten stubs (fail at runtime) — `AStatLog` batchers are never started
  in single-player (`bWorldLog` off), the audio compressor is editor-only.
- The one wasm runtime trap found in Core: `UnMisc.cpp:2030` non-POD varargs (§i.3). Re-scan HGame-side natives? None exist (§f.4 item 10).

## Prioritized checklist — Core + Engine edits for the first native-Linux compile

Order = the order in which the compiler/linker/boot will hit them. P0 = compile, P1 = link, P2 = boot to Entry, P3 = web/MinGW
correctness that should land in the same pass because it is cheap. Each line names the section with the evidence.

**P0 — make Core compile (one commit: headers must change together)**
- [ ] `Core/Inc/UnGnuG.h` ← template's file verbatim + HP2 re-adds: `FLOAT`/`DOUBLE` `GSecondsPerCycle` decision, `FTime appSeconds()`/`appSecondsSlow()` decls, `ASMKNI 0`, `__UNIX__`/`__LINUX__` decision (§a.2, §g.2).
- [ ] `Core/Inc/UnFile.h:108-124` guard/unguard ← template's `try/catch` form (no `__Context`/setjmp) (§a.2).
- [ ] `Core/Inc/Core.h`: drop lines 72-80; `PLATFORM_WIN32` `TCHARU` typedef; `GPackage` → `THIS_PACKAGE` block; `#include "UnStaticExports.h"` (§a.1, §f.4-1).
- [ ] `Core/Inc/UnFile.h:206-217`: `Package=GPackage` defaults → `THIS_PACKAGE` (§f.4-2).
- [ ] `Core/Inc/UnObjBas.h`: keep 4-arg `DECLARE_*`; delete `__STATIC_LINK` arms; template `IMPLEMENT_CLASS` (+`STATIC_EXPORT`) and `IMPLEMENT_PACKAGE`; `new( (EInternal*)X )TClass;` without parens (§f.4-3, §d.7).
- [ ] `Core/Inc/UnScript.h:28-30`: `IMPLEMENT_FUNCTION` + `STATIC_EXPORT` (§f.4-4).
- [ ] `Core/Inc/UnTemplate.h`: `using TRefArray<T>::Data; using TRefArray<T>::ArrayNum;` in `TArray`; `this->` in `TTransArray`/`TLazyArray`/`TMap`/`TMultiMap` as the template (§l.2).
- [ ] `Core/Inc/UnMath.h`: delete the `__PSX2_EE__` FVector/VU0 arms (192-656 + `#endif` 925, 1953-2007, 2028-2076) and the `ASMLINUX` arms (§b, §c).
- [ ] `Core/Inc/UnMem.h:35-37`, `UnScript.h` PSX2 arms, `UnAnsi.cpp:75,327-334`, `UnClass.cpp:383-391,408-421`, `UnProp.cpp:1416-1424`, `UnObj.cpp:3531-3540`, `UnCorSc.cpp:427-432`, `UnGUID.cpp:174-175`: take the non-PSX2 arm (§b.1).
- [ ] `Core/Src/UnClass.cpp:1439`: `wcscmp` → `appStrcmp` (§d.6).
- [ ] `Core/Src/UnMisc.cpp:2047` (use at 2054): hoist `const TCHAR* p` out of the `for` (§i.3 #3); plus the for-scope list in Appendix A.1 (take template resolutions; `UnLinker.cpp:413-418` by hand) (§d.3).
- [ ] `Core/Inc/UnCoreNative.h:16`: fix `(##type` (hygiene; unreachable with `__STATIC_LINK` off) (§d.1).
- [ ] `Core/Inc/FFileManagerLinux.h` ← template's + HP2's `ArIsSaving = ArIsPersistent = 1;` in `FArchiveFileWriter` (§h.2).

**P0 — make Engine compile**
- [ ] `Engine/Inc/Palette.h:18`: `__declspec(dllexport)` under `#if _MSC_VER` (§d.2).
- [ ] `Engine/Src/UnTex.cpp:12-16` and the `Compress/Decompress` block: `#ifdef WIN32` → `#if _MSC_VER` (§d.5; required for MinGW, harmless for Linux).
- [ ] `Engine/Src/ULodMesh.cpp:233-237`: take `appFloor(Frame)`; `Engine/Src/UnAudio.cpp:794-…`: take the non-PSX2 RIFF walk (§b.1).
- [ ] Engine for-scope sites (Appendix A.1) — all in files the template already fixed; take the template's hunks.

**P1 — make it link (Unix core)**
- [ ] `Core/Src/UnUnix.cpp` ← template's, plus an "HP2 433 additions" block: `FTime appSeconds()`/`appSecondsSlow()`/`appProcessSeconds()` (rebased counter), `const TCHAR* appUserDir(){ return TEXT(""); }`, `bool IsOSVer2kOrXP(){ return true; }`, HP2-signature `void* appCreateProc(…, UBOOL)` + `appGetProcReturnCode` (§a.3, §g.1, §l.1).
- [ ] New `Core/Src/UnFileStream.cpp`: `FFileStream::Read/Create×2/Destroy(INT)` (regular + Ogg via hook or libvorbis, XA stubbed) + a synchronous `Pump()` (§e.2).
- [ ] `GSecondsPerCycle` one type in `Core.h:437`, `Core.cpp:143`, `UnGnuG.h` (§g.2).
- [ ] `--whole-archive` link of every package (already in HP2's CMake) (§f.4-7).

**P2 — boot to Entry with readable text**
- [ ] `Core/Src/UnMisc.cpp:1960`: `default.ini` → `Default.ini`, initialise `DTemp`, only `SetString` on success, `delete DConfig` (§i.2).
- [ ] `Core/Src/UnMisc.cpp:2011`: `"..\\Save"` → `".." PATH_SEPARATOR "Save"`; `:2030` `*GSys->SaveSlotPath` (§i.3).
- [ ] `Core/Src/UnMisc.cpp` `FFileStream::Destroy` wait loop: `#if !__UNIX__` → `#if _MSC_VER` (exit hang) (§e.2-3).
- [ ] `appFindPackageFile`: port the template's `appFindFileNoCase` per-path retry (`Startup.unr`/`potions`/`hp_fx`/`adv1Willow.unr`…) (§h.3).
- [ ] `Game/Src/Launch.cpp:883`: `appInit( TEXT("Game"), … )` (§i.1).
- [ ] Web/dev `Default.ini`: `Paths=../Save/*.usa` and keep it the **last** `Paths=` line; dev ini: un-suppress `Localization`, `DevLoad`, `DevBind`, `ScriptWarning` (§i.3 #5, §j).
- [ ] `Core/Src/UnLinker.cpp:298-306`, `Engine/Src/UnGame.cpp:961-965`: `catch( const TCHAR* )` (§k.3).

**P3 — same pass, cheap, prevents known web/MinGW failures**
- [ ] `FFileManagerLinux::GetGlobalTime` → 0 (at least on `__EMSCRIPTEN__`) so persistent-actor caches and user inis survive sessions (§h.2).
- [ ] `Engine/Src/UnCamera.cpp:370-398`: remove the `#if _MSC_VER` around the viewport teardown; `:534-543` MSVC BMP row loop (§d.5).
- [ ] `Core/Src/UnAnsi.cpp:282-290`: `return Result;` in `appSprintf` (§d.7).
- [ ] wasm CMake: keep `-Wnon-pod-varargs` visible (drop `-Wno-non-pod-varargs`, keep `-Wno-error=…`) and fix every hit (§d.3).
- [ ] Optional: `const` + `operator<` on `FTime`; web-only removal of `Clock(GStat.MemTime)` in `FMallocAnsi.h` after measuring (§g.1, §g.3).
- [ ] Verification gates after the first build: `grep -rn __PSX2_EE__ Source/{Core,Engine,Render}` → 0; `grep -rnE 'asm *(\(|volatile)' Source/{Core,Engine}` → MSVC-only; boot log shows `Character set: ANSI`, no `Can't bind to native class`, `Language` = `int`, and `VERIFY_CLASS_SIZE`/offset checks silent.

## Appendix A — inventories

### A.1 `for`-scope reuse candidates in GCC-compiled Core/Engine (heuristic; 66 sites / 33 files; compiler is the arbiter)
Format `file:declaration-line → use-line (var)`. All files exist in the template (v400-inherited, already fixed there) except `UnLinker.cpp`.
```
Core/Inc/FFileManagerGeneric.h:108→112 (i)   Core/Inc/FMallocDebug.h:191→198 (C), 236→240 (C), 437→445 (s), 440→443 (n)
Core/Inc/FMallocWindows.h:100→105 (Index)    Core/Inc/FOutputDeviceFile.h:71→73 (i)
Core/Src/UnLinker.cpp:413→418 (j)  [HP2-only file]
Core/Src/UnMath.cpp:23→33 (i)                Core/Src/UnMisc.cpp:1852→1856 (c), 2250→2252 (len), 2274→2276 (Count)
Core/Src/UnMisc.cpp:2047→2054 (p, const TCHAR*; found by reading, §i.3)
Core/Src/UnName.cpp:138→145 (i)              Core/Src/UnObj.cpp:1423→1426 (i), 2886→2898 (i), 2968→2971 (j)
Engine/Inc/UnActor.h:61→62 (Top; likely false positive)   Engine/Inc/UnModel.h:154→157 (i)
Engine/Src/UnActCol.cpp:117→121 (i)          Engine/Src/UnActor.cpp:1145→1161 (i), 1243→1255 (i)
Engine/Src/UnAudio.cpp:706→735 (j)           Engine/Src/UnCamera.cpp:705→722 (i), 780→797 (i), 880→884 (j), 968→975 (i), 977→981 (j)
Engine/Src/UnCanvas.cpp:240→334 (i)          Engine/Src/UnChan.cpp:865→871 (Test), 977→978 (Top), 1067→1070 (R)
Engine/Src/UnDynBsp.cpp:274→286 (Link), 303→308 (i)       Engine/Src/UnEngine.cpp:288→291 (C)
Engine/Src/UnFPoly.cpp:315→353 (i)           Engine/Src/UnGame.cpp:723→732, 896→905, 1175→1186 (i), 1188→1192 (j), 1373→1377, 1990→1999 (i)
Engine/Src/UnIn.cpp:131→137, 157→163 (i)     Engine/Src/UnLevAct.cpp:543→628 (i), 728→740 (i), 1216→1271 (Test), 1388→1423 (Check)
Engine/Src/UnLevTic.cpp:646→659, 784→795 (i) Engine/Src/UnLevel.cpp:820→840, 1217→1221 (i)
Engine/Src/UnPath.cpp:848→859, 1184→1207 (i) Engine/Src/UnPawn.cpp:1752→1768 (i)   Engine/Src/UnPhysic.cpp:2509→2510 (i)
Engine/Src/UnPrim.cpp:877→895 (i)            Engine/Src/UnRoute.cpp:261→274 (j)
Engine/Src/UnScript.cpp:2312→2322 (i), 2328→2330 (j), 2452→2456 (j)
Engine/Src/UnTex.cpp:654→664, 727→729, 925→933 (i), 1100→1102 (destWidth), 1105→1107 (destHeight)
Engine/Src/UnURL.cpp:433→436 (i)
```
(The scanner's type list does not include `const`-qualified or `unsigned`-prefixed-pointer declarations; expect a few more from the compiler.)

### A.2 `__PSX2_EE__`: 32 lines / 15 files — Core 22, Engine 2, Render 7, UCC 1 (table in §b.1).

### A.3 asm sites: table in §c. Multi-line asm string literals (lexable only while their group is skipped): `UnMath.h` 18, `UnRender.h` 2, HP2 `UnGnuG.h` 3 → 0 after the edits.

### A.4 Symbols the static registry must resolve
`GLoaded<Pkg>` (Core, Engine, Render, Fire, Editor, SoftDrv, each driver) · `autoclass<Class>` (every `IMPLEMENT_CLASS`) · `int<Class>exec<Func>`
(every `IMPLEMENT_FUNCTION`: Core 242, Engine 28) · numbered natives via `GRegisterNative` at static init. HP2 script-only packages
(HGame, HPParticle, HProps, HPModels, HPSounds, UWindow, UnrealShare) need none.

### A.5 `Source/Core/Src` treatment
Built: `UnProp UnObj UnLinker UnCorSc UnName UnMem UnMisc UnMath UnCoreNet UnCoreNative(empty TU) UnClass UnCache UnBits UnAnsi Core UFactory UExporter`
+ `UnGUID` (POSIX) / `UnMinGW` (MinGW) + `UnUnix` + `UnStaticExports` (static) + **new `UnFileStream`**. Not built: `UnVcWin32.cpp` (MSVC Win32 core:
rdtsc FTime, `FFileStream` thread + coda + vorbis, `appUserDir`, `IsOSVer2kOrXP`), `UnPSX2.cpp`, `AudioMutex.cpp` (never shipped), `VuCoreMath.dsm/.vsm`,
`Core.dsp`, `makefile` (Loki/PS2).

### A.6 Scratchpad artefacts used (session scratchpad, not in the repo)
`forscope.py` + `forscope3.py` (A.1), `nonpod.py` / `nonpod2.py` (§d.3, found `UnMisc.cpp:2030`), `va.cpp` / `va2.cpp` (gcc non-POD varargs
behaviour), header census script (§k.2), `old-hp2-core-engine-specifics.md` (the first run of this task; sections (d)-(l) were re-verified
against the tree before being carried over, with these corrections: HP2 *removed* v400's `GetGlobalTime→0` rather than lacking it;
package versions are 61/68/69/76/79, not all 79; HP2's `UCC.cpp` is still the original; `#pragma pack(push,1)` is honoured by GCC).


## Status: complete

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
_pending_

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


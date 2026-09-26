# Windows (MinGW-w64 cross) target — engine-ut99 template as an HP2 replication checklist

Key: `windows-mingw-template`
Scope: how engine-ut99 (UT99 v400 web port) builds its Windows target with a MinGW-w64 cross toolchain, and exactly
what the HP2 port (UE1 build 433) must replicate. **Windows is parity work; the web build is the goal.** Every item is
tagged **[WEB-1]** (needed for the first web build — usually because the same code/decision is shared with the
native-Linux/web targets) or **[LATER]** (Windows-only parity).

(Re-run of this analysis; every claim below was re-verified against the trees on 2026-09-26.)

## 0. Executive summary
_(pending)_

## 1. Template: BUILD.md Windows section and CLAUDE.md rules
Sources: `/home/ion/engines/engine-ut99/BUILD.md` L12-27 (build matrix) and L91-155 ("Windows builds (MinGW-w64) — step 2.33");
`/home/ion/engines/engine-ut99/CLAUDE.md` (no Windows-specific rule; the four foundations below); commits `301a9ca` (2.33),
`b725899` (2.33 docs), `76df82e` (2.34) — read from a scratch copy of the template's `.git` (no git was run inside
`/home/ion/engines/*`).

| # | Template rule (BUILD.md, quoted or tightly paraphrased) | Where | HP2 ACTION | Tag |
|---|---|---|---|---|
| 1.1 | `cmake --preset windows && cmake --build --preset windows -j"$(nproc)"` → `build-windows/UnrealTournament/UnrealTournament.exe` | L93-96 | Same two commands from `Source/`; HP2 output is `build-windows/Game/Game.exe` (launcher target is `Game`, PLAN §2). **Already wired** in `Source/CMakePresets.json` L59-86 | LATER |
| 1.2 | "One self-contained 32-bit `.exe`: `BUILD_STATIC` links every package in, so `appGetDllHandle` resolves through the internal registry and there is **no `Core.dll`/`Engine.dll` and no `LoadLibrary`**" | L98-100 | Same mechanism the web and native presets use (`UNREAL_STATIC` + `UnStaticExports` + `--whole-archive`). HP2's `Core/Src/UnStaticExports.cpp` is already copied; the `windows` preset already sets `BUILD_STATIC=ON`. Nothing Windows-specific to write — but `UnUnix.cpp`'s `appGetDllHandle` must be the template's static-registry version (Phase A takes the template's `UnUnix.cpp`) | WEB-1 (shared mechanism) |
| 1.3 | "`objdump -p` shows only stock system DLLs" | L100 | Acceptance check (§18): `i686-w64-mingw32-objdump -p build-windows/Game/Game.exe \| sed -n 's/^\tDLL Name: //p'` ⊂ `{ADVAPI32 GDI32 IMM32 KERNEL32 msvcrt ole32 OLEAUT32 OPENGL32 OpenAL32 SETUPAPI SHELL32 USER32 VERSION WINMM WS2_32}.dll` (the template CI's allow-list, `/home/ion/engines/engine-ut99/.github/workflows/build.yml`, windows job) | LATER |
| 1.4 | "⚠ **32-bit only.** `Source/CMakeLists.txt` demotes an x86_64 compiler to `-m32` and `FATAL_ERROR`s if it cannot. The toolchain file targets `i686-w64-mingw32`" | L102-105 | HP2 already has the demotion block (`Source/CMakeLists.txt` L129-139) and the toolchain file (byte-identical, §3) | WEB-1 (demotion also guards native) / LATER (toolchain) |
| 1.5 | "same MinGW-w64 GCC that MSYS2's `MINGW32` environment ships, so the preset also works from an MSYS2 prompt (where pacman's SDL2 answers instead of the fetched one)" | L103-105 | Keep `elseif(MINGW AND CMAKE_CROSSCOMPILING) include(SDL2Mingw)` exactly (HP2 L237-238 already does). ⚠ Under MSYS2 the OpenAL branch falls to `find_package(OpenAL)` (pacman `mingw-w64-i686-openal`) and **no `OPENAL_MINGW_RUNTIME_DLL` is set, so nothing is copied beside the exe** — acceptable (MSYS2 has the DLL on PATH) but say so in HP2's BUILD.md | LATER |
| 1.6 | "**The compiler is not the platform.** Almost every failure was a `#if _MSC_VER` or `#if __GNUG__` used to mean "Windows" or "POSIX"… Rekeyed onto `PLATFORM_WIN32` (or IpDrv's own `__WINSOCK__`/`__BSD_SOCKETS__`)" | L107-110 | Apply the same rekeying to HP2's 433 sources — site list in §16. HP2 has *more* such sites than v400 (Loki/PSX2 heritage: `__LINUX_X86__`, `__PSX2_EE__`, `__STATIC_LINK`) | LATER (but several §16 edits are also WEB-1 because the web is not `_MSC_VER` either) |
| 1.7 | Platform layer: "MSVC now gets the Win32 core; every GCC/Clang target keeps the portable `UnUnix.cpp`… genuinely Win32 calls in **`Core/Src/UnMinGW.cpp`** (`appGetSystemErrorMessage`, `appGetGUID` via `CoCreateGuid`, `WSAStartup`)" | L111-117 | Already mirrored in HP2 `Source/Core/CMakeLists.txt` L26-39; `UnMinGW.cpp` copied byte-identical. What is **not** done: HP2's `UnUnix.cpp`/`UnUnix.h` are still HP2's Loki-era files without the 2.33 Win32 hunks (§11, §15) | WEB-1 for the CMake shape (done); LATER for the Win32 hunks |
| 1.8 | IpDrv: "WinSock vs BSD sockets, the resolver thread (`CreateThread` vs `pthread_create`), `fcntl` vs `ioctlsocket`, `socklen_t` (one `<ws2tcpip.h>` include…)" | L118-119 | HP2's `Source/IpDrv/` is the template's, copied verbatim (only the Dreamcast `pthread` lines dropped from its CMakeLists) — the 2.33 rekey is already in it. Keep it through the 433 adaptation (§7) | LATER (already present) |
| 1.9 | UnTex.cpp: "the DXT block was `#ifdef WIN32` but calls Epic's `s3tc.lib`, which **no CMake target links**… `WIN32` meant `_MSC_VER`" | L120-121 | HP2's `Engine/Src/UnTex.cpp` has the **same** structure as v400: `#ifdef WIN32` at L747 around the real `Compress`/`Decompress` (`S3TCencode`/`S3TCdecode`/`HeckbertQuantize`), `#else` stubs returning 0 at L956-966, plus header includes under `#ifdef WIN32` at L12-16. Apply the template's one-line rekey L747 `#ifdef WIN32` → `#if _MSC_VER`. Linux/web already take the stub (they never define `WIN32`); MinGW would compile the real block and fail at link. §8/§12 prove the runtime never needs the real block | LATER (inert for Linux/web — they take the stub today) |
| 1.10 | UnParams.cpp: "duplicate-symbol guard said `__EMSCRIPTEN__`, but the cause is whole-archiving, i.e. `UNREAL_STATIC`" | L122-123 | HP2 `Editor/Src/UnParams.cpp` has **no guard at all** (verified §8) while `Engine/Src/UnParams.cpp` defines the same five helpers → wrap Editor's copies in `#ifndef UNREAL_STATIC` | **WEB-1** (the static `native` link hits it first) |
| 1.11 | "`-fpack-struct=4`. UE1 assumes Win32 `/Zp4`… Native x86-32 Linux matches by luck… MinGW follows the MSVC ABI and aligns to 8, so every class holding a double came out long" | L127-131 | HP2 already has it in the web block (`Source/CMakeLists.txt` L79) and the MinGW block (L170). **Not** on native Linux — exactly like the template (§6, §17) | WEB-1 (web block, present) / LATER (MinGW block, present) |
| 1.12 | "`ABrush::bColoredPad` is NOT applied on MinGW… MinGW-w64 lays bitfields out the *MSVC* way (`-mms-bitfields` is its default)" | L132-134 | HP2's `EngineClasses.h` has **no** `bColoredPad`; whether it needs one on GCC-SysV (native/web) is decided by the layout guard (§17). If it is ever added, guard it `#ifndef __MINGW32__` from day one | WEB-1 (native/web pad decision) / LATER (MinGW half) |
| 1.13 | "`DWORD`/`SIZE_T`/`BITFIELD` likewise come from `<minwindef.h>` on Windows and must stay *identical* types… UE1 serialises BITFIELD members through `operator<<`'s DWORD overload" | L134-136 | Carry the template `UnGnuG.h` PLATFORM_WIN32 typedef block into HP2's `UnGnuG.h` (PLAN §3 already says "take the template's file and re-add HP2's 433 bits") | LATER (inert on Linux/web; comes free with the template `UnGnuG.h`) |
| 1.14 | wine + Xvfb recipe; "Assert on **milestones in `System/UnrealTournament.log`**"; "copy the `.exe` in"; "`cp -rL`" | L138-149 | §19 (with a box-specific fix: `wineserver` is not on PATH here) | LATER |
| 1.15 | "**Out of scope for now:** hardware renderers on Windows (XOpenGL/UTGLR/NOpenGLES), and `ucc.exe`." | L155 | ⚠ **Stale in the template**: 2.34 (`76df82e`) enabled XOpenGL + UTGLR in the windows preset and verified both under wine. HP2's `BUILD.md` must not repeat the sentence. `ucc` *does* compile under the windows preset (UCC is `if(NOT EMSCRIPTEN)`; the template CI builds the whole preset) as `ucc-bin.exe`, but is unverified/unshipped | LATER |

The four `CLAUDE.md` foundations the Windows target stands on (all inherited from ut99dc, "do not re-derive"): static single
binary (`BUILD_STATIC`→`UNREAL_STATIC` + `--whole-archive` + `UnStaticExports`), ASM off via `UnGnuG.h`, POSIX platform in
`UnUnix.cpp`, `-fpack-struct=4`. Windows re-uses all four; nothing in the Windows target is a fifth mechanism.

## 2. Template: CMakePresets windows preset
Template: `/home/ion/engines/engine-ut99/Source/CMakePresets.json` L48-69 (HEAD = after 2.34 `76df82e`; 2.33 `301a9ca` had
`BUILD_XOPENGLDRV/BUILD_NOPENALDRV=OFF` and no `BUILD_NOPENGLDRV`). HP2: `Source/CMakePresets.json` L58-80 (already written by the
CMake-skeleton commit). Side-by-side of the cache variables:

| Cache variable | Template `windows` | HP2 `windows` (on disk) | Verdict / ACTION |
|---|---|---|---|
| `generator` / `binaryDir` / `toolchainFile` | `Unix Makefiles` / `${sourceDir}/../build-windows` / `${sourceDir}/cmake/toolchain-mingw-w64.cmake` | same | OK |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` | `RelWithDebInfo` | OK (→ `-O2` from the GCC branch + CMake's `-O2 -g -DNDEBUG`; the `.exe` carries DWARF — strip for a distributable, see §18) |
| `BUILD_STATIC` | `ON` | `ON` | OK — mandatory: MinGW + `SHARED` would need `__declspec(dllexport)` on every `*_API`, which `UnGnuG.h` defines empty |
| `BUILD_EDITOR` | `ON` | `ON` | OK — runtime dependency (template 2.39); HP2's own need is PLAN §2 "Editor ON unless the content analysis proves otherwise" |
| `BUILD_DCUTIL` | `OFF` | *(absent)* | OK — HP2 has no DCUtil option at all |
| `BUILD_SOFTDRV` / `BUILD_NULLDRV` | `ON` / `ON` | `ON` / `ON` | OK — these two are what make the wine+Xvfb check GPU-free |
| `BUILD_NOPENGLESDRV` | `OFF` | *(absent)* | OK — PLAN §2: no NOpenGLESDrv in this port (so the glad-clash problem of 2.34 cannot arise) |
| `BUILD_XOPENGLDRV` | `ON` | `ON` | OK — resolves GL via `SDL_GL_GetProcAddress` against system `opengl32.dll` |
| `BUILD_NOPENGLDRV` | `ON` | `ON` | OK — UTGLR against system `opengl32`, **no gl4es on Windows** (b725899: "UTGLR needs NO GL4ES on Windows") |
| `USE_GL4ES` | `OFF` | `OFF` | OK |
| `BUILD_NULLSOUNDDRV` / `BUILD_NOPENALDRV` | `ON` / `ON` | `ON` / `ON` | OK — but HP2's `NOpenALDrv` is still the template's v400 driver (libxmp, `.umx`); the Windows build of it is only meaningful after PLAN Phase E (ogg/vorbis + Layer II). Until then set `-DBUILD_NOPENALDRV=OFF` on the command line for a Windows bring-up (the launcher's `NormalizeSubsystem` then lands on `SoundDrv.NullAudioSubsystem`) |
| `FETCHCONTENT_SOURCE_DIR_XOPENGLDRV` / `…_UTGLR` / `GL4ES_SOURCE_DIR` | *(absent — template fetches the branch)* | `${sourceDir}/../../XOpenGLDrv`, `…/utglr-copy`, `…/gl4es` | ⚠ **These paths do not exist.** `/home/ion/hp2-ufrontend/` holds only `HarryPotter2/` and `ufrontend/`; the three fork clones sit **inside** `ufrontend/` (as `ufrontend/XOpenGLDrv`, `ufrontend/utglr-copy`, `ufrontend/gl4es`). Same finding as `renderer-forks.md` §1.1. ACTION (WEB-1, affects all five presets): move the clones to `/home/ion/hp2-ufrontend/{XOpenGLDrv,utglr-copy,gl4es}` before the first configure. `GL4ES_SOURCE_DIR` in the windows preset is dead weight (only read when `USE_GL4ES OR EMSCRIPTEN`) — harmless, keep for symmetry |

`displayName`/`description` differences are wording only. HP2's description correctly omits the template's stale "NOpenGLESDrv is OFF
here" sentence. **Build preset**: `{ "name": "windows", "configurePreset": "windows" }` present in both.

**Verdict:** the HP2 windows preset is already the template's 2.34 preset, adapted correctly (`Game.exe`, no DCUtil/NOpenGLES). The only
defect is shared with every other preset (the sibling-clone paths). No further preset work is needed for Windows parity; everything
remaining is source work (§14-§17).

## 3. Template: toolchain-mingw-w64.cmake, SDL2Mingw.cmake, OpenALMingw.cmake
All three files are **byte-identical** between the template and HP2 (`diff -q` against `/home/ion/engines/engine-ut99/Source/cmake/`,
also `TargetArch.cmake`, `OpenALSoft.cmake`, `ArtifactDeps.cmake`). Nothing HP2-specific is needed in any of them.

### 3.1 `toolchain-mingw-w64.cmake` (31 lines, `301a9ca`)
| Line(s) | Content | Why it matters |
|---|---|---|
| L14-16 | `CMAKE_SYSTEM_NAME Windows`; `MINGW_TARGET_ARCH` cache var default `i686`; `CMAKE_SYSTEM_PROCESSOR ${MINGW_TARGET_ARCH}` | `WIN32`/`MINGW` become true in CMake → the top-level `TARGET_IS_WINDOWS` block fires. `TargetArch.cmake` sees x86 (not x86_64) so the 64→32 demotion never runs for the MinGW build |
| L18-22 | `${_MINGW_TRIPLE}-gcc/-g++/-windres` | ⚠ On this box `i686-w64-mingw32-g++` is the **`-win32` thread-model** alternative (`update-alternatives` → `/usr/bin/i686-w64-mingw32-g++-win32`, "GCC 13-win32"). The engine uses no `std::thread`, so this is fine; if a fetched dependency ever needs C++11 threads, switch the alternative to `-posix` (then `libwinpthread-1.dll` must be linked statically — `-static` or `-Wl,-Bstatic -lwinpthread`) |
| L27-31 | `CMAKE_FIND_ROOT_PATH /usr/${_MINGW_TRIPLE}` + `MODE_PROGRAM NEVER`, `LIBRARY/INCLUDE/PACKAGE ONLY` | Keeps host glibc headers / `libSDL2.so` out ("conflicting declaration of int mkdir" is the giveaway) |

Box facts (verified with `command -v` / `dpkg -l`, no build run): `mingw-w64 13.0.0-2ubuntu1`, `i686-w64-mingw32-gcc` 13 (win32),
`i686-w64-mingw32-objdump` and `-windres` present, `/usr/i686-w64-mingw32/lib/{libws2_32.a,libole32.a,libopengl32.a}` present. PLAN §0
says "`i686-w64-mingw32-gcc` 13.2" — consistent.

### 3.2 `SDL2Mingw.cmake` (51 lines, `301a9ca`)
- Fetches `https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz`, SHA256 `83a5d740…38ff2`
  (L15-31). Version chosen to match the box's system SDL2 so native and Windows run the same SDL.
- Sets `SDL2_INCLUDE_DIR=<root>/i686-w64-mingw32/include` and `SDL2_LIBRARY` = `-lmingw32 libSDL2main.a libSDL2.a -ldinput8 -ldxguid
  -lsetupapi -lversion -luuid -limm32 -lole32 -loleaut32 -lshell32 -luser32 -lgdi32 -lwinmm -lm` (L44-51). ⚠ **Order is load-bearing**:
  `-lmingw32` and `SDL2main` before `SDL2`, because `SDL.h` renames `main` → `SDL_main` and `SDL2main` supplies `WinMain`; wrong order
  = undefined `WinMain`. This is why the launcher needs no Windows source change.
- Only included when `MINGW AND CMAKE_CROSSCOMPILING` (HP2 `Source/CMakeLists.txt` L237-238) — under MSYS2 `-lSDL2` is used.
- **HP2 ACTION:** none. Needs network on first configure (FetchContent URL); reuse `build-windows/_deps` thereafter. [LATER]

### 3.3 `OpenALMingw.cmake` (58 lines, `76df82e`)
- Fetches `https://github.com/kcat/openal-soft/releases/download/1.25.2/openal-soft-1.25.2-bin.zip`, SHA256 `67a0c4b8…65434` (L23-39).
- Links the **import library** `libs/Win32/libOpenAL32.dll.a` (L46-47) and records `OPENAL_MINGW_RUNTIME_DLL=bin/Win32/soft_oal.dll`
  shipped under the name `OPENAL_MINGW_RUNTIME_DLL_NAME=OpenAL32.dll` (L55-58, `CACHE INTERNAL` so the launcher's directory sees it).
- Why a DLL and not static (L8-18): the prebuilt drop is DLL+implib; building openal-soft from source pulls a C++17 subproject with its
  own allocator overrides into the engine (null-ctor crash seen on Android); a silent build is not an acceptable default.
- Included from `NOpenALDrv/CMakeLists.txt` L57-61 (`elseif(MINGW AND CMAKE_CROSSCOMPILING)`), identical in HP2. The POST_BUILD copy is in
  HP2 `Source/Game/CMakeLists.txt` L51-57 (template `UnrealTournament/CMakeLists.txt` L63-72) — **already ported**.
- ⚠ Phase E note: when HP2's `NOpenALDrv` gains libogg/libvorbis (PLAN §7) those must be **static FetchContent** builds so MinGW
  cross-compiles them too (they are plain C and do). Do not add a second prebuilt DLL: the allow-list in §18 has room for exactly one
  non-system DLL, `OpenAL32.dll`. Also drop libxmp from HP2's `NOpenALDrv` (no `.umx` in HP2) — that removes the `BUILD_STATIC`
  save/restore dance too.
- **HP2 ACTION:** none now. [LATER]

## 4. Template: Core platform-layer selection (UnVcWin32 / UnUnix / UnMinGW)
### 4.1 The rule (template `Source/Core/CMakeLists.txt` L21-42, commit `301a9ca`)

```cmake
if(MSVC)
  list(APPEND SRC_FILES "Src/UnVcWin32.cpp")
else()
  # ⚠ ORDER: UnGUID/UnMinGW before UnUnix, matching the original list. Source order is object order is
  # link order, so reversing it perturbs the web and Linux binaries for the sake of a Windows-only change.
  if(TARGET_IS_WINDOWS)
    list(APPEND SRC_FILES "Src/UnMinGW.cpp")
  else()
    list(APPEND SRC_FILES "Src/UnGUID.cpp")
  endif()
  list(APPEND SRC_FILES "Src/UnUnix.cpp")
endif()
if(BUILD_STATIC)
  list(APPEND SRC_FILES "Src/UnStaticExports.cpp")
endif()
```

| Toolchain | Platform files compiled | Header chain | Why |
|---|---|---|---|
| MSVC (not built by us; `BUILD_STATIC` is a `FATAL_ERROR` with MSVC) | `UnVcWin32.cpp` | `Core.h` → `UnVcWin32.h` (`#if _MSC_VER`) | Epic's Win32 core; its whole body is `#if _MSC_VER` (template L8) |
| GCC native Linux / Emscripten | `UnGUID.cpp` + `UnUnix.cpp` + `UnStaticExports.cpp` | `Core.h` → `UnGnuG.h` (`#elif __GNUG__`) | POSIX layer (SDL timers, `getcwd` base dir, static-registry `appGetDllHandle`) |
| **MinGW-w64** | **`UnMinGW.cpp` + `UnUnix.cpp` + `UnStaticExports.cpp`** | `Core.h` → `UnGnuG.h` (MinGW defines `__GNUG__`, not `_MSC_VER`) | the same portable layer; `UnMinGW.cpp` supplies the 3 things POSIX has no answer for |

The 2.33 failure mode worth remembering: the old list said `if(NOT TARGET_IS_WINDOWS) UnUnix.cpp else UnVcWin32.cpp`, so MinGW compiled
`UnVcWin32.cpp` to an **empty** TU (its body is `#if _MSC_VER`) and dropped `UnUnix.cpp` — "a link with ~20 missing `app*` symbols
(appPlatformInit, appBaseDir, appCycles, appGetDllHandle …) and nothing to say why".

### 4.2 What `UnMinGW.cpp` provides (94 lines, all under `#ifdef PLATFORM_WIN32`)
| Function | Implementation | Replaces on POSIX |
|---|---|---|
| `CORE_API const TCHAR* appGetSystemErrorMessage(INT Error)` (L30-46) | `FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM…)`, CR/LF stripped | nothing (only Windows headers `FFileManagerWindows.h`, `FOutputDeviceWindowsError.h` call it; declared in `UnFile.h:56`) |
| `void appGetGUID(void*)` (L57-64) | `CoCreateGuid` → needs `ole32` (top CMake `link_libraries(ws2_32 ole32)`) | `UnGUID.cpp` (OSF/DCE over `gettimeofday` + MAC) |
| `CORE_API void appWinSockInit()` (L75-88) | one-shot `WSAStartup(MAKEWORD(2,2))` | nothing — called by `UnUnix.cpp` before `gethostname` in `appComputerName` (L729) and `appGetLocalIP` (L882) |

⚠ Minor doc defect in the template (carried into HP2 verbatim): `UnMinGW.cpp` L71-73 says "IpDrv calls this from its own init (see
IpDrvPrivate.h)". It does not — IpDrv's `InitSockets` (`IpDrv/Src/UnSocket.cpp` L77-101) calls `WSAStartup(0x0101)` itself. Harmless
(WSAStartup is reference-counted) — fix the comment when touching the file, no code change.

### 4.3 The template's `UnUnix.cpp` Windows hunks (7 `PLATFORM_WIN32` sites, all from `301a9ca`)
| Template line | What | MinGW behaviour |
|---|---|---|
| L47-53 | includes: `<winsock2.h>`, `<windows.h>`, `<process.h>` instead of `<netdb.h>` | winsock2 before windows (order matters) |
| L138-156 | `USystem::Exec "RELAUNCH"`-style server spawn: `_spawnl(_P_NOWAIT,"./ucc.exe","ucc","server",…)` then `appRequestExit(0)` instead of `fork/execl` | |
| L352 | `appFreeDllHandle`: `FreeLibrary` (unreachable under `UNREAL_STATIC`, which is checked first) | |
| L425-432 | `appCreateProc`: `_spawnv(_P_NOWAIT,…)` instead of `fork/execv` | |
| L728-730 | `appComputerName`: `appWinSockInit()` before `gethostname` | avoids `WSANOTINITIALISED` before IpDrv loads |
| L744-754 | `appUserName`: `GetUserNameA` (fallback `"Player"`) instead of `getlogin` | |
| L881-883 | `appGetLocalIP`: `appWinSockInit()` | |

Plus in headers: `Core/Inc/UnUnix.h` L26-31 declares `CORE_API void appWinSockInit();` under `PLATFORM_WIN32`;
`Core/Inc/FFileManagerLinux.h` L287-293 uses `mkdir(path)` (no mode) under `PLATFORM_WIN32` — **`FFileManagerLinux` is the file
manager on every GCC target, Windows included** (the launcher picks it; `FFileManagerWindows` is never used by the game exe).

## 5. Template: every PLATFORM_WIN32 / TARGET_IS_WINDOWS / MINGW / __WINSOCK__ site
Command: `grep -rn 'PLATFORM_WIN32\|TARGET_IS_WINDOWS\|__MINGW32__\|MINGW\|__WINSOCK__\|mms-bitfields\|bColoredPad\|WINDOWS_IGNORE_PACKING_MISMATCH\|fpack-struct'`
over `/home/ion/engines/engine-ut99/Source` (Dreamcast/KOS/PVR/AICA/DCUtil/UnrealEd/Launch excluded; `_deps` excluded). Every hit, classified:

| File:line | Site | Kind | Present in HP2 today? | HP2 ACTION |
|---|---|---|---|---|
| `CMakeLists.txt` L223-237 | `TARGET_IS_WINDOWS` ← `if(WIN32)` | build | yes (L141-148) | none |
| `CMakeLists.txt` L291-294 | `-DPLATFORM_WIN32 -DWIN32 -DWINDOWS_IGNORE_PACKING_MISMATCH` | build | yes (L160-163) | none |
| `CMakeLists.txt` L295-300 | `if(MINGW) add_link_options(-static-libgcc -static-libstdc++)` | build | yes (L164-167) | none |
| `CMakeLists.txt` L309 | MinGW `-fpack-struct=4 -Wno-address-of-packed-member` | build/ABI | yes (L170) | none |
| `CMakeLists.txt` L313 | `link_libraries(ws2_32 ole32)` | build | yes (L172) | none |
| `CMakeLists.txt` L146 | web `-fpack-struct=4` | build/ABI | yes (L79) | none |
| `CMakeLists.txt` L422-426 | `elseif(MINGW AND CMAKE_CROSSCOMPILING) include(SDL2Mingw)` | build | yes (L237-238) | none |
| `CMakeLists.txt` L337 | MSVC `/Zp4` | build | yes (L189) | none (MSVC unused) |
| `Core/CMakeLists.txt` L29-42 | platform-file selection (§4) | build | yes (L29-39) | none |
| `IpDrv/CMakeLists.txt` L34-35 | `if(TARGET_IS_WINDOWS) target_link_libraries(IpDrv ws2_32)` | build | yes (L30-32) | none (redundant with the global `link_libraries`, harmless) |
| `NOpenALDrv/CMakeLists.txt` L57-61 | `elseif(MINGW AND CMAKE_CROSSCOMPILING) include(OpenALMingw)` | build | yes (identical) | none now; see §3.3 for Phase E |
| `Editor/CMakeLists.txt` L37,55 | `TARGET_IS_WINDOWS AND BUILD_WINDRV` → EdHook/Window | build | n/a — HP2's Editor list is already the stripped NullEdHook set | none |
| `cmake/OpenALSoft.cmake` L20,80,94 | strips `-fpack-struct=4` from openal-soft's own objects | build/ABI | yes (identical) | none; only used on Linux/web. **Rule to keep:** any third-party C/C++ built *inside* the engine tree (libogg/libvorbis in Phase E) must NOT inherit `-fpack-struct=4` if it has its own ABI with other binaries — they are static and private here, so inheriting is harmless, but do not pass their structs across a DLL boundary |
| `Core/Inc/Core.h` L63-68 | `#elif defined(PLATFORM_WIN32) typedef ANSICHARU TCHARU;` (MinGW's `<winnt.h>` sets `_TCHAR_DEFINED`) | source | **no** — HP2 `Core.h` L107-111 has only `#ifndef _TCHAR_DEFINED` | **port the hunk** (§16 row C1) |
| `Core/Inc/UnGnuG.h` L11-13, 27-33, 44-46, 62-70, 103-108, 118-124, 159-161, 180-182, 191-195 | `__WIN32__`; no `<alloca.h>`/`<strings.h>`; `<minwindef.h>`; keep `VOID`; `STDCALL __stdcall` and keep `__cdecl`; `GCC_HIDDEN` empty; `DWORD`/`SIZE_T` from minwindef; `BITFIELD` = `DWORD` | source | **no** — HP2's `UnGnuG.h` is the Loki file (`#error Unsupported platform` unless `__LINUX_X86__`/`__PSX2_EE__`) | comes free when PLAN Phase A takes the template `UnGnuG.h` (§16 row C2) |
| `Core/Inc/UnUnix.h` L26-31 | `appWinSockInit` decl | source | **no** — HP2 `UnUnix.h` is Loki's | comes free: PLAN "take theirs" for `UnUnix.h` |
| `Core/Inc/FFileManagerLinux.h` L287-293 | `mkdir(path)` without mode | source | **no** — HP2 L263 is epic's `mkdir(path, __S_IREAD && __S_IWRITE && __S_IEXEC)`: glibc-only macros (MinGW compile error) **and** a logical-AND bug (mode = 1 on Linux/web — ut99dc fixed it to `S_IRUSR\|S_IWUSR\|S_IXUSR`). HP2's file is otherwise 433-modified (no `GetGlobalTime/SetGlobalTime`, writer flags), so it is a merge, not a copy | port both hunks (§16 row C5) — the mode fix is WEB-1 (`UnGame.cpp` L250-251/445-446/470-471/486 create `SavePath`/`SaveSlotPath` through it) |
| `Core/Src/UnUnix.cpp` L47,138,352,425,728,744,881 | §4.3 | source | **no** — HP2 `UnUnix.cpp` is Loki's | comes free with "take the template's `UnUnix.cpp`, re-add 433 bits" |
| `Core/Src/UnAnsi.cpp` L225 | `appFindFiles` via `_findfirst` on Windows | source | n/a — HP2 has no `appFindFiles` in `Core/Src` (433 uses `GFileManager->FindFiles`) | none |
| `Core/Src/UnMinGW.cpp` | whole file | source | yes (byte-identical) | none |
| `Engine/Inc/EngineClasses.h` L1884-1886 | `#ifndef __MINGW32__ BYTE bColoredPad[3]; #endif` | ABI | **no** | §17 — add it exactly so, WEB-1 |
| `Engine/Src/UnTex.cpp` L759 | DXT block `#if _MSC_VER` (was `#ifdef WIN32`) | source | **no** — HP2 L747 is still `#ifdef WIN32` | §16 row E1 |
| `Editor/Src/UnParams.cpp` | `#ifndef UNREAL_STATIC` (was `#ifndef __EMSCRIPTEN__`) | source | **no guard at all** in HP2 | §16 row ED1, WEB-1 |
| `Fire/Src/FractalPrivate.h` L20 | `#if !_MSC_VER && !defined(__MINGW32__) typedef SQWORD __int64;` | source | **no** — HP2's counterpart `Fire/Inc/UnFractal.h` L19 is `#if !_MSC_VER` | §16 row F1 |
| `SoftDrv/Src/SoftDrvPrivate.h` L12-21 | `#undef IN / #undef OUT` under `PLATFORM_WIN32` (the template's `DrawSurf.cpp` has a local named `IN`) | source | not present, and **not needed today**: `grep -rnw 'IN\|OUT'` over HP2's Core/Engine/Render/Fire/SoftDrv/Editor/drivers finds only a comment and a string | optional: add the same 4 lines when PLAN takes the template's SoftDrv hunks, so a future local named `IN` cannot bite (§16 row S1) |
| `IpDrv/Src/IpDrvPrivate.h` L14-33, 91-96, 109, 120-131, 174 | `__WINSOCK__` from `PLATFORM_WIN32`, `<ws2tcpip.h>`, `CreateThread` resolver, `socklen_t` | source | yes (verbatim copy) | none (§7) |
| `IpDrv/Src/UnSocket.cpp` L16, 77, 120, 247, 276, 285 / `UnSocket.h` L12, 46 | resolver entry signature; `WSAStartup`; error text; `IP()` macro | source | yes | none |
| `IpDrv/Src/TcpLink.cpp` L47, 486 / `UdpLink.cpp` L96 | `fcntl` only under `__BSD_SOCKETS__`; `(const char*)&optval` cast | source | yes | none |
| `Game/Src/Launch.cpp` (template `UnrealTournament/Src/Launch.cpp`) | 2.33 `PLATFORM_WIN32 && PLATFORM_SDL` override → 2.34 generalised to `#ifndef __EMSCRIPTEN__ NormalizeSubsystem(...)` | source | yes (copied unadapted) | keep through the Phase B launcher adaptation; HP2's retail `Default.ini` names `WinDrv.WindowsClient` + `ALAudio.ALAudioSubsystem` (and `D3DDrv.D3DRenderDevice`), exactly the case NormalizeSubsystem exists for |
| `DCUtil/Src/Texture.cpp` | Dreamcast tool | — | n/a | none |

`-mms-bitfields` appears **only in comments** (EngineClasses.h L1880, BUILD.md L133): it is MinGW-w64's *default*, never passed
explicitly. `WINDOWS_IGNORE_PACKING_MISMATCH` is defined (CMakeLists L294) but **used by no source** in either tree, and **mingw-w64 13's headers do not
reference it either** (`grep -rn WINDOWS_IGNORE_PACKING_MISMATCH /usr/i686-w64-mingw32/include` → nothing). It is the Microsoft Windows SDK's
(10.0.17134+) opt-out for the `C_ASSERT(TYPE_ALIGNMENT(...))` packing checks in *MSVC's* `<winnt.h>`, which fire under `/Zp4`. So on
MinGW it is inert; it only matters for an MSVC build. Keep it (harmless, and HP2 already has it at `Source/CMakeLists.txt` L163).

## 6. Template: packing rules (-fpack-struct, -mms-bitfields, bColoredPad, WINDOWS_IGNORE_PACKING_MISMATCH)
### 6.1 The contract
UE1 reflects C++ classes against UnrealScript: `UStruct::Link` lays properties out in declaration order, aligns the class to 4
(`PropertiesSize = Align(PropertiesSize, PropertiesAlign)`, HP2 `Core/Src/UnClass.cpp` L394, `PropertiesAlign = 4` except under
`__PSX2_EE__`), and a derived class's first property starts at the parent's **aligned** size. Retail was MSVC `/Zp4` with MSVC bitfield
layout and **no tail-padding reuse**. Every GCC-family ABI we build must reproduce that byte-for-byte for every class whose C++ header
mirrors a `.u` (the `*Classes.h` files, `UnTex.h`, `UnFractal.h`, …).

### 6.2 Where the three ABIs differ — measured on this box (toy TU in the scratch dir; `g++ -m32` 15.2, `i686-w64-mingw32-g++` 13-win32)

| Probe | MSVC /Zp4 (retail, reference) | GCC i386 SysV (`native`) | GCC i386 SysV + `-fpack-struct=4` | MinGW default | MinGW + `-fpack-struct=4` (**windows preset**) |
|---|---|---|---|---|---|
| `struct{int a; double d;}` → offset of `d` | 4 | 4 | 4 | **8** | 4 |
| `struct{int a; long long q;}` → offset of `q` | 4 | 4 | 4 | **8** | 4 |
| polymorphic base ending in a `BYTE`, derived adds a `BYTE`: is the derived byte put in the base's tail padding? | **no** | **yes** (Itanium C++ ABI) | yes | **yes** (MinGW g++ is Itanium C++ ABI too) | yes |
| `ABrush`-shaped class ending `INT PolyFlags; BITFIELD bColored:1 GCC_PACK(4);` → `AMover`'s first `BYTE` offset (PolyFlags at 124) | 132 | **129** ✗ | **129** ✗ | 132 ✓ | 132 ✓ |
| same with `BYTE bColoredPad[3];` after `bColored` | — | 132 ✓ | 132 ✓ | **135** ✗ (sizeof 136) | **135** ✗ |
| same, MinGW with `-mno-ms-bitfields -fpack-struct=4` | — | — | — | 129 ✗ / 132 with pad | (don't) |

Conclusions the template acted on, now proven for this toolchain:
1. **`-fpack-struct=4` is required on MinGW** (doubles/`long long`/`QWORD` go to 8 otherwise) and on wasm32; it is a no-op for these
   probes on i386 SysV, which is why the template (and HP2) do not pass it for `native`. Template: `Source/CMakeLists.txt` L146 (web),
   L309 (MinGW); HP2: L79, L170. It must be **global** (`add_compile_options`), "packing must be uniform or the two halves of a class
   disagree".
2. **The `bColoredPad` compensates for GCC's SysV bitfield packing, which MinGW does not do** (MinGW-w64 defaults to `-mms-bitfields`):
   pad on every non-MinGW GCC/Clang target, never on MinGW → `#ifndef __MINGW32__`. Do **not** "fix" it by passing `-mno-ms-bitfields`
   to MinGW — that would break every *other* `BITFIELD x:1 GCC_PACK(4)` run that currently matches retail by virtue of MS layout.
3. **Tail-padding reuse is shared by SysV and MinGW** (both Itanium C++ ABI), so any hazard of that kind (a reflected class ending in
   a sub-4-byte member with a C++ subclass whose first member is a `BYTE`) must be fixed with `GCC_ALIGN(4)` on the subclass's first
   member, on **all** GCC targets, not only Linux. The template has exactly that in `Fire/Src/FractalPrivate.h` L197/L202/L315
   (`BYTE SparkType GCC_ALIGN(4)`, `BYTE FX_Heat GCC_ALIGN(4)`, `BYTE DropType GCC_ALIGN(4)`, inherited from ut99dc `cbdb054`).
4. **Size checks cannot see offset shifts.** The template's layout guard (`Core/Src/UnClass.cpp` L299-302 records `sizeof`,
   L376-381 warns `[layout] native %s: C++ sizeof=%i but script layout=%i -- member-offset shift risk`) caught the MinGW double problem
   on first boot ("Mover 1060 vs 1056, FireTexture 1296 vs 1292") and the MinGW over-pad ("Brush: C++ sizeof=612 but script
   layout=608"), but the ABrush/AMover SysV shift itself keeps both sizes equal and was found by a one-shot per-property offset
   comparison (step 2.32). The wine run's acceptance is "zero `[layout]` warnings **and** the mover fixture behaves like the web".

### 6.3 The flags, as the template sets them
| Flag / define | native | web | MinGW | MSVC |
|---|---|---|---|---|
| `-fpack-struct=4 -Wno-address-of-packed-member` | — | ✓ | ✓ | (`/Zp4`) |
| `-mms-bitfields` | — | — | implicit default | n/a |
| `-static-libgcc -static-libstdc++` | — | — | ✓ | — |
| `-DPLATFORM_WIN32 -DWIN32 -DWINDOWS_IGNORE_PACKING_MISMATCH` | — | — | ✓ | ✓ |
| `-DPLATFORM_POSIX -DPLATFORM_CASE_SENSITIVE_FS` | ✓ | ✓ | — | — |
| `bColoredPad[3]` present | ✓ | ✓ | — | — |

HP2's `Source/CMakeLists.txt` already matches this table row for row (L79, L160-177). Only the source-side items (pad, `GCC_ALIGN`)
are missing — §17.

## 7. Template: IpDrv WinSock vs BSD guards
HP2's `Source/IpDrv/` is the template's IpDrv copied verbatim (`diff -rq` shows only `CMakeLists.txt`, which dropped the Dreamcast
`pthread` link). Its WinSock/BSD split therefore already has the 2.33 rekey. For the record, the full set of switches:

| Where | Switch | WinSock (MinGW) | BSD (Linux; web has no sockets at runtime but compiles this) |
|---|---|---|---|
| `IpDrvPrivate.h` L14-24 | `#ifdef PLATFORM_WIN32` → `__WINSOCK__ 1`, `SOCKET_API "WinSock"`; else `__BSD_SOCKETS__ 1` | | |
| `IpDrvPrivate.h` L26-35 | `#if __WINSOCK__` includes | `<windows.h> <winsock.h> <conio.h> <ws2tcpip.h>` (`ws2tcpip.h` gives `socklen_t`) | — |
| `IpDrvPrivate.h` L37-51 | `#if __BSD_SOCKETS__` includes | — | `<sys/socket.h> <netinet/in.h> <arpa/inet.h> <netdb.h> <pthread.h> <fcntl.h>`… |
| `IpDrvPrivate.h` L91-96 / `UnSocket.cpp` L16-20 | `ResolveThreadEntry` signature | `DWORD STDCALL (void*)` (needs `STDCALL`=`__stdcall`, i.e. the template `UnGnuG.h`) | `void* (void*)` |
| `IpDrvPrivate.h` L109-111 | `pthread_t ResolveThread` member | absent | present |
| `IpDrvPrivate.h` L120-131 | resolver thread start | `CreateThread` + `CloseHandle` | `pthread_create` |
| `IpDrvPrivate.h` L174-178 | `bindnextport` length type | `INT` | `socklen_t` |
| `UnSocket.h` L12-15 / L18-42 | type shims | `__SIZE_T__ = INT` | `SOCKET=int`, `closesocket=close`, `ioctlsocket=ioctl`, `WSAGetLastError()=errno`, … |
| `UnSocket.h` L46-50 | `IP(sin_addr,n)` | `sin_addr.S_un.S_un_b.s_b##n` | byte cast |
| `UnSocket.cpp` L77-104 | `InitSockets` | `WSAStartup(0x0101)` (its own; not `appWinSockInit`) | `GInitialized = 1` |
| `UnSocket.cpp` L120, 247, 276, 285 | error text, non-blocking, etc. | WinSock names | errno names |
| `TcpLink.cpp` L47 / `UdpLink.cpp` L96 | `fcntl(O_NONBLOCK)` only `#if __BSD_SOCKETS__` (else `ioctlsocket(FIONBIO)`) | | |
| `TcpLink.cpp` L73-78 | `setsockopt(..., (const char*)&optval, ...)` — cast satisfies both | | |
| `TcpLink.cpp` L486 | accept `EINPROGRESS` too, only `__BSD_SOCKETS__` | | |
| `IpDrvClasses.h` L6, L262 | `#pragma pack(push,4)` only `#if _MSC_VER` | global `-fpack-struct=4` covers it | natural i386 layout |
| `UMasterServerCommandlet.cpp` L133 / `UUpdateServerCommandlet.cpp` L179 | `_kbhit/_getch` console polling `#if _MSC_VER` | compiles the `#else` (no console key polling) — fine for a commandlet; a `PLATFORM_WIN32` rekey would pull `<conio.h>` `_kbhit`, which MinGW has, but nothing needs it | |
| `UdpLink.cpp` L38, 56, 138, 178, 207 | `WebRTCLan_*` calls | `#ifdef __EMSCRIPTEN__` only — MinGW never references WebRTCDrv (which is web-only in both CMake trees) | |
| `IpDrv/CMakeLists.txt` L30-32 | `if(TARGET_IS_WINDOWS) target_link_libraries(IpDrv ws2_32)` | ✓ | — |

**HP2 ACTION:** none Windows-specific. The 433 adaptation of IpDrv (PLAN §4 step 6: `UnNetDrv.h`/`UnConn.h`/`UnNet.h`/`UnBunch.h`/
`UnChan.h` deltas) must keep every switch above keyed on `PLATFORM_WIN32`/`__WINSOCK__`/`__BSD_SOCKETS__`, never on `_MSC_VER`/`__GNUG__`.
One HP2 wrinkle: the 433 net headers use `FTime` for `UNetConnection::LastReceiveTime`/… (`Engine/Inc/UnConn.h` L80-83); `FTime` is
`long long`-based (`Core.h` L24-52) — harmless for IpDrv (not reflected), but it is an 8-byte member that MinGW would align to 8 without
`-fpack-struct=4` → another reason the flag must stay global. [LATER]

## 8. Template: UnTex.cpp DXT/S3TC guard and Editor UnParams.cpp duplicate guard
### 8.1 Template `Engine/Src/UnTex.cpp` (commit `301a9ca`)
| Template line | Guard | Content | Status |
|---|---|---|---|
| L12-16 | `#ifdef WIN32` (**left as is**) | `#include "UnDDraw.h"`, `"Palette.h"`, `"S3tc.h"` | headers exist in `Engine/Inc` and parse under MinGW (MinGW accepts `__declspec`), so the template did not rekey them |
| L752-758 | comment | "ufront 2.33: was `#ifdef WIN32`. These implementations call into Epic's S3TC encoder and Heckbert quantiser (Engine/Lib/s3tc.lib plus the `__declspec(dllexport)` declarations in Palette.h), which NO CMake target links … "WIN32" quietly meant "MSVC". Under MinGW it compiled and then failed at link with undefined S3TCencode/S3TCdecode/HeckbertQuantize." | — |
| L759 | `#if _MSC_VER` | real `UTexture::Compress` (`S3TCgetEncodeSize`/`S3TCencode`) and `UTexture::Decompress` (`S3TCgetDecodeSize`/`S3TCdecode`/`HeckbertQuantize`) | MSVC only |
| `#else` | — | `Compress(...) { return 0; }` / `Decompress(...) { return 0; }` | **every** template build (Linux, web, Windows) takes the stubs; no open DXT codec was added |

The template's `Engine/CMakeLists.txt` does **not** list `palette.cpp` (`grep -rln palette.cpp /home/ion/engines/engine-ut99/Source` → nothing), although
Epic's `Engine.dsp` L111 does (`SOURCE=.\palette.cpp`). So `HeckbertQuantize` has no definition in any template binary — consistent with
the stubbed caller.

### 8.2 HP2 `Engine/Src/UnTex.cpp` — the same structure, un-rekeyed
| HP2 line | Content | Consequence per target | ACTION |
|---|---|---|---|
| L12-16 | `#ifdef WIN32` → `UnDDraw.h`, `Palette.h`, `S3tc.h` (all three present in `Source/Engine/Inc/`) | Linux/web: skipped (nothing defines `WIN32`). MinGW: included; `Palette.h` L18 `__declspec( dllexport ) int HeckbertQuantize(...)` is accepted by MinGW g++ | rekey to `#if _MSC_VER` together with L747 (cleaner than the template, which left L12 alone; either is correct) — **LATER** |
| L745 | `typedef struct {BYTE rgba[4];} COLOR;` | shared | none |
| **L747** | `#ifdef WIN32` around `Compress` (L752-848) and `Decompress` (L850-954) | Linux/web: stub (correct by accident). **MinGW: the windows preset passes `-DWIN32` (`Source/CMakeLists.txt` L162) → compiles the real block → undefined `S3TCencode`, `S3TCgetEncodeSize`, `S3TCdecode`, `S3TCgetDecodeSize` at link** (`HeckbertQuantize` would resolve only because HP2 still compiles `palette.cpp`, see W3) | `#ifdef WIN32` → `#if _MSC_VER` + the template's comment — **LATER** (but do it in Phase A while the file is open; it is inert for native/web) |
| L956-966 | `#else` stubs `return 0;` | what every GCC target runs | none |

### 8.3 HP2 `Engine/Src/palette.cpp` (2101 lines) is in the build and must leave it
`Source/Engine/CMakeLists.txt` L10 lists `"Src/palette.cpp"`. It `#include "Palette.h"` (L21), whose L18 declares
`__declspec( dllexport ) int HeckbertQuantize(...)`. Neither HP2's `UnGnuG.h` nor the template's defines `__declspec` (`grep -rn declspec
Core/Inc` → only `UnVcWin32.h` L45-47 `DLL_IMPORT/DLL_EXPORT`, MSVC), so on **Linux g++ and emcc/clang the declaration is a hard error**
(`__declspec` is a Microsoft extension that GCC recognises only on Windows targets / Cygwin / MinGW; clang only with `-fms-extensions` or
`-fdeclspec`). Its only callers are inside the `#ifdef WIN32` block (`UnTex.cpp` L902, L917). **ACTION (WEB-1, = §20 W3): remove
`Src/palette.cpp` from `Engine/CMakeLists.txt`** — the template does not compile it either. Verification: `grep -rn 'HeckbertQuantize\|GenerateMipmap\|CopyPalettisedData\|IndexSearch'
Source --include=*.cpp | grep -v palette.cpp` must list only the `UnTex.cpp` MSVC block.

### 8.4 Template `Editor/Src/UnParams.cpp` duplicate guard (2.7d, rekeyed 2.33)
Template L22-30:
```cpp
// ufront 2.7d: Engine/Src/UnParams.cpp defines these same parse helpers (GetFVECTOR/GetSUBSTRING/
// GetFROTATOR). In the shared-lib (native) builds each package's copy lives in its own .so — no clash.
// A STATIC build whole-archives every package into ONE binary, so both copies collide (duplicate symbol).
// Engine is always linked, so skip Editor's copies there and let callers resolve to Engine's. Editor's
// UNIQUE helpers (GetBEGIN/GetEND/SetFVECTOR/GetFSCALE, below) stay compiled.
// ufront 2.33: the guard was `#ifndef __EMSCRIPTEN__` — the web being merely the first static build we
// shipped, not the reason. The reason is whole-archiving, i.e. UNREAL_STATIC, which equally covers the
// Dreamcast and Windows builds. Web behaviour is unchanged (it sets BUILD_STATIC too).
#ifndef UNREAL_STATIC
```
…closed by L171 `#endif // !UNREAL_STATIC (ufront 2.7d — Engine provides these shared parse helpers in a static build)`.

### 8.5 HP2 `Editor/Src/UnParams.cpp` — no guard at all
HP2 Editor defines `GetFVECTOR` (L25), `GetSUBSTRING` (L55), `GetFVECTOR` overload (L86), `GetFROTATOR` (L105, L145) — the **same five**
that HP2 `Engine/Src/UnParams.cpp` defines at L25/L55/L86/L105/L145 (`ENGINE_API` there, `EDITOR_API` here; both expand to nothing under
`UnGnuG.h`, so the mangled names are identical). Every HP2 preset sets `BUILD_STATIC=ON` + `--whole-archive` → duplicate-symbol link error
on native, web and Windows alike. **ACTION (WEB-1, = §20 W2):** insert the template's L22-30 comment + `#ifndef UNREAL_STATIC` before HP2 L20
("Get a floating-point vector" block) and `#endif // !UNREAL_STATIC` after the second `GetFROTATOR` (before `GetBEGIN`, HP2 L166). Keep
`GetBEGIN` (L166), `GetEND` (L183), `SetFVECTOR` (L198), `GetFSCALE` (L209) compiled. Verify: `diff <(sed -n 20,160p Editor/Src/UnParams.cpp)
<(sed -n 20,160p Engine/Src/UnParams.cpp)` shows the two copies are functionally the same (so resolving Editor's callers to Engine's copy
changes nothing).

## 9. Template: git history of the MinGW work (2.33 / 2.34)
Command (run against a scratch clone of the template's `.git`, `scratchpad/ut99.git`, so no git ran inside `/home/ion/engines/*`):
`git log --oneline --grep=2.33 --grep=2.34 --grep=MinGW --grep=Windows -i | head -40` → 19 commits. Classified:

| Commit | Subject | What matters for HP2 |
|--------|---------|------|
| `301a9ca` | build(2.33): Windows binaries via MinGW-w64 (cmake --preset windows) | **The port.** 23 files, +496/-32: `.gitignore`, `BUILD.md`, `Source/CMakeLists.txt`, `CMakePresets.json`, `Core/CMakeLists.txt`, `Core/Inc/{Core.h,FFileManagerLinux.h,UnGnuG.h,UnUnix.h}`, `Core/Src/{UnMinGW.cpp (new),UnUnix.cpp}`, `Editor/Src/UnParams.cpp`, `Engine/Inc/EngineClasses.h` (bColoredPad `#ifndef __MINGW32__`), `Engine/Src/UnTex.cpp`, `Fire/Src/FractalPrivate.h`, `IpDrv/Src/{IpDrvPrivate.h,TcpLink.cpp,UdpLink.cpp,UnSocket.cpp}`, `SoftDrv/Src/SoftDrvPrivate.h`, `UnrealTournament/Src/Launch.cpp`, `cmake/{SDL2Mingw,toolchain-mingw-w64}.cmake`. Every one of these has an HP2 row in §5/§16. Verified: web 29/29, Linux boot, Windows boot to CityIntro with 0 layout warnings + the 2.32 mover fixture under wine |
| `b725899` | docs(2.33): record what a Windows hardware renderer actually needs | UTGLR needs **no gl4es** on Windows; the blockers were utglr's SDL types being gated off-WIN32 and `#undef VOID` in `UnGnuG.h` |
| `76df82e` | build(2.34): hardware GL on Windows, all five renderers in one static binary | 10 files: `CMakeLists.txt`, `CMakePresets.json` (XOpenGL/UTGLR/NOpenAL ON), `UnGnuG.h` (keep `VOID` on Windows), `NOpenALDrv/CMakeLists.txt` + `cmake/OpenALMingw.cmake`, launcher CMake (POST_BUILD `OpenAL32.dll`, `UT_HAS_NOPENALDRV`) and `Launch.cpp` (`NormalizeSubsystem` for every non-web target), NOpenGLES glad symbol prefix (HP2: n/a). Fork-side fixes (utglr `_WIN32` asm/`__try` → `_MSC_VER`, WGL paths → SDL-owned context, SDL shims off `__UNIX__`) are already on the HP2 fork branches (§20 D9) |
| `e7f442d` | ci(2.34): build every push + PR on linux/windows/web, upload artifacts | the DLL import allow-list assertion (§18) |
| `a930e9b`, `6b3704b`, `0d623ca` | ci(2.34) fixes (cold emsdk cache, artifact names by `ENGINE_VERSION`, bash) | CI-only; `6b3704b`'s naming scheme → HP2 artifact `hp2-433-windows` |
| `7d3ed39` | fix(build): the Editor package is a RUNTIME dependency — build it natively too | why `BUILD_EDITOR=ON` in the windows preset |
| `4e78c02` | build(ucc): a static build links the headless package set whole-archive | UCC static link; `ucc.exe` on Windows still not a goal |
| `5674fc7` | Engine: fix savegame load on Unix (PATH_SEPARATOR vs FURL URL syntax) | not Windows-specific, but HP2 hard-codes `TEXT("..\\Save")` (`Core/Src/UnMisc.cpp` L2011, fallback when `appUserDir()` is empty) — a backslash path that only Windows accepts; relevant to every GCC target (§16 row C8) |
| `b548e28`, `a2f0ec7`, `520e08f`, `2074fe0`, `1d3dfa5`, `3703f35`, `7164de7`, `1fb5f26`, `7c31a53` | matched the grep on unrelated words ("windows" in comments etc.) | none |

Cross-engine record, `/home/ion/ufrontend/docs/migration/steps/2.33-windows-builds.md` and `2.34-build-matrix-and-ci.md` (read only). Lessons HP2 imports:
| Lesson | Source | HP2 status / ACTION |
|---|---|---|
| **`O_BINARY`**: MinGW `open()` defaults to text mode → CRLF/Ctrl-Z translation → short reads (the ut2004 crash) | 2.33 L145-148; 2.34 L204 | HP2's `FFileManagerLinux` uses `fopen(…,"rb")` (L219) / caller-supplied mode (L238, callers pass `"wb"`/`"ab"`); no raw `open()` in Core/Engine/drivers. **But** the launcher's `SimpleCopy` (`Game/Src/Launch.cpp` L747-765) uses `fopen(…,"r")`/`"w"` — text mode on MinGW: CR stripped then re-added, Ctrl-Z (0x1A) truncates. For an `.ini` copy that is harmless; change to `"rb"`/`"wb"` anyway (LATER, one line). Rule for Phase E: any new `open()`/`fopen()` in NOpenALDrv/Ogg/FFileStream code must be binary |
| **Error path swallows its own message**: `FOutputDeviceAnsiError::HandleError` calls `StaticShutdownAfterError()` before printing → early errors appear as a bare exit | 2.34 L309-325 | debug with `printf 'c\nbt\nq\n' \| winedbg ./Game.exe` and `i686-w64-mingw32-addr2line -f -C -s -e Game.exe 0x…` (PE ImageBase `0x400000`, so raw fault addresses resolve directly) |
| **`FOutputDeviceFile` prefixing `appBaseDir()` `#if WIN32`** made a directory literally named `Z:` in ut2004 | 2.33 | HP2 `Core/Inc/FOutputDeviceFile.h` L42 prefixes **`appUserDir()`** (433 change), not `appBaseDir()`. `UnUnix.cpp` today has **no `appUserDir`** (only `UnVcWin32.cpp` L1048-1070 defines it) → Phase A must add one (§15.3) |
| **`cp -rL`**, never `cp -r`, for run trees; copy (not symlink) the exe | 2.34 L301 | §19 |
| **Grab frames while the process is alive** (`ffmpeg -f x11grab` after `timeout` gives black); GL under Xvfb is Mesa llvmpipe | 2.34 L297, L331 | §19 step 6 |

## 10. Template: wine + Xvfb verification recipe
Template `BUILD.md` L138-153 (verbatim):
```bash
Xvfb :99 -screen 0 1024x768x24 &
DISPLAY=:99 WINEPREFIX=~/.wine-u1 wine UnrealTournament.exe -SOFTWARERENDERER   # or -NULLRENDERER
WINEPREFIX=~/.wine-u1 wineserver -k          # ⚠ always; wine leaves processes behind
```
Rules stated alongside it:
- needs `wine32` (32-bit exe); SoftDrv/NullDrv are CPU-only, so no GPU/GLX involvement.
- "Assert on **milestones in `System/UnrealTournament.log`**, not on a window appearing."
- "copy the `.exe` in (the engine derives its base dir from the real binary path) and dereference asset symlinks (`cp -rL`), or every
  package becomes a silent `FileNotFound`."
- "⚠ wine is not Windows — a regression net, not a substitute for a real-hardware pass."
- Verified at 2.33: boots to `CityIntro` on SoftDrv with zero layout warnings; the 2.32 multi-keyframe mover fixture reproduces the web
  oracle (red 100 % through t=5 s, green from t=6 s, stable 53.6 % vs web 53.2 %). The mover fixture was the functional gate for the
  ABrush/AMover layout.
- 2.34 extension (`2.34-build-matrix-and-ci.done.md` L35, L211-214): hardware GL under wine+Xvfb works through Mesa llvmpipe; grab the
  frame while the process runs (`sleep N && ffmpeg -f x11grab -video_size 1024x768 -i :99.0 -frames:v 1 -y shot.png`).

Box facts (dpkg/`command -v`, 2026-09-26): `wine 10.0` with `wine32:i386`, `xvfb`, `xvfb-run` installed; several other agents' prefixes
exist (`~/.wine-u1`, `~/.wine-u1-w32`, `~/.wine-ufront`, `~/.wine-ut2004-w32`, …). **HP2 must use its own prefix** (`~/.wine-hp2`) and
its own display number so it never kills or reconfigures another agent's wineserver (§19). The template's `wineserver -k` line assumes
`wineserver` on PATH; **on this box it is not** (`command -v wineserver` → nothing). Ubuntu's wine 10.0~repack-12ubuntu1 installs it at
`/usr/lib/i386-linux-gnu/wine/wineserver` and `/usr/lib/x86_64-linux-gnu/wine/wineserver` (found with `find / -name 'wineserver*'`).
Use the full path (§19). Also present: `wine32`, `wine64`, `wineboot`, `winedbg`, `Xvfb`, `xvfb-run`, `ffmpeg`,
`libgl1-mesa-dri:i386` 26.0.8 (the 32-bit Mesa that hardware-GL-under-wine needs).

## 11. HP2: current state of the same places
Snapshot of `/home/ion/hp2-ufrontend/HarryPotter2/Source` on 2026-09-26 against every place §1-§10 named. "Built" = reached by
`Source/CMakeLists.txt` L283-383 (`Core Engine Render IpDrv Fire UCC WebRTCDrv NSDLDrv OpenGLDrv XOpenGLDrv SoftDrv NullDrv SoundDrv
NOpenALDrv Editor Game`); the Windows-only originals (`WinDrv Window D3DDrv UnrealEd Launch ALAudio …`) are reference-only.

### 11.1 Build system (already Windows-ready)
| Place | HP2 state | Verdict |
|---|---|---|
| `CMakePresets.json` windows preset | L58-80 / build preset L84-90, = template 2.34 shape | done (§2); shares the sibling-path defect (§21 Q1) |
| `cmake/toolchain-mingw-w64.cmake`, `SDL2Mingw.cmake`, `OpenALMingw.cmake` | byte-identical to template | done (§3) |
| Top-level Windows block | L141-148 `TARGET_IS_WINDOWS`; L160-177 `-DPLATFORM_WIN32 -DWIN32 -DWINDOWS_IGNORE_PACKING_MISMATCH`, `-static-libgcc -static-libstdc++`, `-fpack-struct=4 -Wno-address-of-packed-member`, `link_libraries(ws2_32 ole32)`; L237-238 `include(SDL2Mingw)`; web `-fpack-struct=4` at L79; MSVC `/Zp4` at L189 | done (§5, §6) |
| `Core/CMakeLists.txt` platform selection | L26-43: `if(MSVC) UnVcWin32.cpp else (TARGET_IS_WINDOWS ? UnMinGW.cpp : UnGUID.cpp) + UnUnix.cpp`; `UnStaticExports.cpp` under `BUILD_STATIC` | done (§15) |
| `IpDrv/` | present — the template's IpDrv copied verbatim (not in the retail drop), `ws2_32` at `IpDrv/CMakeLists.txt` L30-32 | done for Windows (§7); the 433 net-header adaptation is PLAN §4 step 6 |
| `NOpenALDrv/CMakeLists.txt` L57-61 `include(OpenALMingw)`; `Game/CMakeLists.txt` L44-57 `HP2_HAS_NOPENALDRV` + POST_BUILD `OpenAL32.dll` copy | present | done (§18) |
| `Game/CMakeLists.txt` | console-subsystem exe (no `-mwindows`, no `WIN32_EXECUTABLE`), no `.rc` — same as the template. `Game/Src/Res/LaunchRes.rc` + `Unreal.ico` exist; `afxres.h` exists in mingw-w64 (`/usr/i686-w64-mingw32/include/afxres.h`), so `windres` could compile it | parity with template; icon/`-mwindows` is an optional LATER nicety (§14.4) |

### 11.2 Core platform layer (the files Windows depends on and HP2 has not merged yet)
| File | HP2 state | What MinGW would hit |
|---|---|---|
| `Core/Src/UnVcWin32.cpp` (1519 lines) | Epic/HP2 Win32 core, whole body `#if _MSC_VER` (L8); **not compiled** on any of our targets (MSVC-only in `Core/CMakeLists.txt`). HP2 433 content inside it that the portable layer must re-provide: `appUserDir()` (L1048-1070: `SHGetFolderPathW(CSIDL_PERSONAL)` + `"\\Harry Potter and the Chamber of Secrets\\"`), the file-streaming thread for `FFileStream` (`CreateThread(FileStreamingThread)` L1488-1492, vorbis/coda includes under `#pragma pack(push,8)` L654-662), the `CODASetNew/CODASetDelete` init (L1479-1486) | nothing (not compiled) — but every function above is a **gap in `UnUnix.cpp`** on all GCC targets (§15.3) |
| `Core/Inc/UnVcWin32.h` (353 lines) | included by `Core.h` L62-63 only `#if _MSC_VER` | nothing on MinGW (MinGW is `__GNUG__` → `UnGnuG.h`) |
| `Core/Src/UnUnix.cpp` (897 lines) | HP2's Loki-era file, body `#if __GNUG__` (L12), `__STATIC_LINK` at L214; **none** of the template's seven `PLATFORM_WIN32` hunks; no `appUserDir` | `<netdb.h>`/`fork`/`getlogin`/`dlopen` → compile errors on MinGW |
| `Core/Inc/UnUnix.h` (31 lines) | byte-identical to Epic v400; no `appWinSockInit` declaration; included from `Core.h` L77-80 only `#if __UNIX__` | fine once the template file replaces it |
| `Core/Src/UnMinGW.cpp` | byte-identical to template | done |
| `Core/Inc/UnGnuG.h` (338 lines) | Loki file: `#ifdef __LINUX_X86__ … #else #error Unsupported platform` (L10-32), `#define HANDLE DWORD` (L79), `#undef VOID` (L75), `typedef unsigned int size_t;` (L152), `#if __UNIX__` path/line-terminator block (L160-168) | cannot compile on any target as-is (§20 W7) |
| `Core/Inc/Core.h` | L28 `FTime` `long long` under `__GNUG__`; L55 `SUPPORTS_PRAGMA_PACK` keyed `_MSC_VER \|\| __ICC \|\| __LINUX__` (evaluated **before** `UnGnuG.h` is included at L64, so `__LINUX__` must come from the command line — the template's CMake never defines it); L77 `#if __UNIX__ #include "UnUnix.h"`; L107-111 TCHAR block without the `PLATFORM_WIN32` `TCHARU` branch | `SUPPORTS_PRAGMA_PACK`=0 on MinGW and web (harmless today: only `UnVcWin32.cpp` L654 and `ALAudio` L15 read it, neither built); `TCHARU` undefined on MinGW (§16 C1) |
| `Core/Inc/FFileManagerLinux.h` | `chmod(..., __S_IREAD\|__S_IWRITE)` L234/L254, `mkdir(path, __S_IREAD && __S_IWRITE && __S_IEXEC)` L263 | glibc-private `__S_I*` → compile error on MinGW; mode-1 dirs on Linux (§20 W6) |
| `Core/Src/UnMisc.cpp` L1629 | `FFileStream::Destroy` waits `while (Destroyed == 1) appSleep(1);` `#if !__UNIX__` | with the template `UnGnuG.h` (no `__UNIX__`) the loop compiles on every GCC target and **spins forever** — the streaming thread that clears `Destroyed` exists only in `UnVcWin32.cpp`. Rekey onto "a streaming thread exists" (§16 C7) |
| `Core/Src/UnMisc.cpp` L2011 | `GSys->SavePath = TEXT("..\\Save")` when `appUserDir()` is empty | Windows accepts it; POSIX/web would create a file literally named `..\Save` (§16 C8) |

### 11.3 Engine / Editor / Fire / SoftDrv / launcher
| Place | HP2 state | Target impact |
|---|---|---|
| `Engine/Src/UnTex.cpp` L12, L747 | `#ifdef WIN32` (not rekeyed) | MinGW link failure (§8.2) |
| `Engine/CMakeLists.txt` L10 | compiles `Src/palette.cpp` | Linux/web compile failure (`__declspec`), §8.3 |
| `Editor/Src/UnParams.cpp` | no `UNREAL_STATIC` guard | duplicate symbols on every static link (§8.5) |
| `Engine/Inc/EngineClasses.h` L2276 | `BITFIELD bColored:1 GCC_PACK(4);` — **same trailing lone bitfield as v400**, no pad | AMover shifted on native + web; correct on MinGW (§17) |
| `Engine/Inc/EngineClasses.h` L6-8, L3953-3955 | `#pragma pack(push,4)` only `#if _MSC_VER` — same as template | global `-fpack-struct=4` covers MinGW/web |
| `Core/Src/UnClass.cpp` L381 | the template's `[layout] native %s: C++ sizeof=%i but script layout=%i` guard is already in HP2 | ready for the wine acceptance check (§19) |
| `Fire/Inc/UnFractal.h` L19-20 | `#if !_MSC_VER typedef SQWORD __int64;` | MinGW: `__int64` is a builtin keyword → syntax error (§16 F1) |
| `Fire/Inc/UnFractal.h` L174, L179, L292 | `BYTE SparkType;` / `BYTE FX_Heat GCC_PACK(4);` / `BYTE DropType;` — no `GCC_ALIGN(4)` (template `FractalPrivate.h` L197/L202/L315 has it) | Itanium tail-padding reuse on **all** GCC targets incl. MinGW: `UFractalTexture` ends in `BYTE DrawPhase, AuxPhase`, so `UFireTexture::SparkType` / `UWaterTexture::DropType` move into its tail padding. Owned by `template-render-fire-softdrv-editor.md`; the Windows angle is only "MinGW is not exempt" |
| `SoftDrv/Src/SoftDrvPrivate.h` L10 | backslash include `"..\..\Render\Inc\Render.h"` | MinGW tolerates it; Linux/emcc do not (§20 W9) |
| `Game/Src/Launch.cpp` | template's launcher, copied unadapted: **no** `PLATFORM_WIN32` site left (2.34 generalised it to `#ifndef __EMSCRIPTEN__` `NormalizeSubsystem`, L324/L528); `SimpleCopy` L747-765 uses text-mode `fopen("r"/"w")` | fine; `SimpleCopy` → binary mode (§9) |
| `UCC/Src/UCC.cpp` | HP2's own: `#if WIN32` (L9, L73, L181) selects `FFileManagerWindows`/`FMallocWindows`/`<windows.h>`; `#elif __LINUX_X86__` (L31, L83, L148) | under MinGW `WIN32` is defined → the Windows file manager branch, never compiled by GCC before (§21 Q3). LATER |

### 11.4 Grep census (built dirs only)
`grep -rn -E '_MSC_VER|__GNUG__|\bWIN32\b|_WIN32|__WIN32__|__LINUX_X86__|__LINUX__|__UNIX__|PLATFORM_WIN32|__MINGW32__|__STATIC_LINK|_XBOX'`
over `Core Engine Render Fire UCC NSDLDrv SoftDrv NullDrv SoundDrv NOpenALDrv Editor Game`: 43 files. Beyond the rows above, every
remaining `_MSC_VER` site means "the MSVC compiler" and is **correct as-is for MinGW** (MinGW takes the GCC branch exactly like Linux):
`UnFile.h` L111 (guard/unguard `try`/`catch(TCHAR*)` form), L328 (`appGetVarArgs`); `UnScript.h` L23 and `UnObjBas.h` L349/L425
(`IMPLEMENT_FUNCTION`/`IMPLEMENT_CLASS`/`IMPLEMENT_PACKAGE` DLL-export forms); `UnTemplate.h` L972/L1026; `UnAnsi.cpp` L42/L142/L284/L292
(`appTimestamp`, `_isnan`, `appSprintf`); `UnAnsi.cpp` L29 `#ifdef _WIN32 <io.h>` (MinGW has `io.h`); `UnMisc.cpp` L1864 (command-line quote
stripping — the SDL launcher builds the command line from `argv`, so no quotes reach it); `UnObj.cpp` L1833 (diagnostic); `UnLinker.cpp`
L299, `UnGame.cpp` L961, `UnLevel.h` L122/L214, `UnLevel.cpp` L1127 (`TCHAR*` vs `char*` — identical types when `TCHAR==char`);
`AStatLog.cpp` L338; `UnURL.cpp` L71; `UnLight.cpp` L661; `UnCamera.cpp` L370/L482/L507/L534/L824/L849/L914/L939 (screenshot BMP
headers use `GCC_PACK(1)` on the GCC side, so packing is right). `__STATIC_LINK` sites (`Core.h` L496, `UnObj.cpp`, `UnObjBas.h`,
`UnCoreNative.cpp`, `UnEngineNative.cpp`, `UnRenderNative.h`, `UnFireNative.h`, `UCC.cpp`) are the 433 native-table mechanism, owned by the
Core analysis — platform-neutral.

One inherited bug worth knowing (not Windows-specific, present in the template too): `UnCamera.cpp` L537-541, the `#else` (GCC) branch of
the screenshot writer loops `for( INT i=SizeY; i>0; i-- ) … &Buf[i*SizeX+j]` — reads one row past the `ReadPixels` buffer and never writes
row 0. MinGW takes this branch too. ACTION (LATER, optional): use the MSVC loop (`i=SizeY-1; i>=0`) unconditionally, after checking that
the GCC render devices' `ReadPixels` returns rows in the same order as D3D's.

## 12. HP2: S3TC decision (does the runtime need s3tc decode for shipped textures?)
**Decision: the HP2 runtime does NOT need S3TC decode (or encode) on any target, Windows included. Stub both, exactly like the
template; never link `Thirdparty/s3tc/s3tc.lib`.**

About the library itself (`i686-w64-mingw32-nm`/`objdump -f`): a 43,920-byte `ar` archive of two `pe-i386` MSVC objects
(`Release/s3tc.obj`, `Release/S3_quant.obj`) exporting plain-C `_S3TCencode`, `_S3TCdecode`, `_S3TCgetEncodeSize`,
`_S3TCgetDecodeSize`, `_S3TCsetAlphaReference` (`S3tc.h` L21 wraps them in `extern "C"`), and importing the MSVC CRT helpers `__ftol`,
`__fltused`, `_floor` plus `_CodeAlphaBlock`/`_CodeRGBBlock`. So MinGW's ld *could* probably be made to link it (C ABI, i386 COFF; `__ftol`
and `__fltused` would need shims) — but it would be a Windows-only closed binary for a code path the runtime never takes (below), it cannot
exist on Linux or the web, and `HeckbertQuantize` (the other half of `Decompress`) lives in `palette.cpp`, which leaves the build (§8.3).
Not worth it.

### 12.1 Who calls `Compress`/`Decompress` (the only S3TC users)
`grep -rn 'Compress(\|Decompress(' Source --include=*.cpp --include=*.h`:
| Caller | Path | Runtime? |
|---|---|---|
| `Engine/Src/UnTex.cpp` L459-461 | `UTexture::CreateMips`: `if( Format == TEXF_RGBA8 ) { Compress( TEXF_DXT1, FullMips ); bHasComp = 1; Decompress( TEXF_P8 ); }` | `CreateMips` is called only by `Editor/Src/UnEdSrv.cpp` L467 (`TEXTURE IMPORT … MIPS=`) → **editor/import** |
| `Engine/Src/UnTex.cpp` L1414-1415 | `UEngineTextureFactory::FactoryCreateBinary`: `if( ParseParam(appCmdLine(),TEXT("COMPRESSDXT")) ) Texture->Compress(TEXF_DXT1,1);` | texture **import** only |
| `Editor/Src/UnEdFact.cpp` L1302, L1352, L1433 | texture factories (`Compress(TEXF_DXT1,1)`, `Decompress(Format)`) | **editor** |
| `Editor/Src/UMergeDXTCommandlet.cpp` L116 | `mergedxt` commandlet | **commandlet** |
| `Editor/Src/UMasterCommandlet.cpp` L63-186 | a *different*, local `Decompress(TArray<BYTE>…)` (delta patcher) | unrelated |

No render device, no `UTexture::Lock`/`Serialize`/`PostLoad`, and nothing in `Render/`, `SoftDrv/`, `NullDrv/`, `NSDLDrv/` calls either
function (`grep -rn 'S3TC\|s3tc' Source/{Render,SoftDrv,NullDrv,NSDLDrv}` → nothing).

### 12.2 How compressed mips are used at load/draw time
- `UTexture::Serialize` (`UnTex.cpp` L339-366): `SerializeMips( this, Ar, Mips ); if( bHasComp ) SerializeMips( this, Ar, CompMips );`
  — both sets are **stored** in the package; loading is plain byte copy, no decode.
- `UTexture::Lock` (`UnTex.cpp` L80-86):
  ```cpp
  UBOOL UseComp = bHasComp && RenDev && RenDev->SupportsTC && CompFormat==TEXF_DXT1;
  TArray<FMipmap>& WhichMips = UseComp ? CompMips : Mips;
  ```
  and L121 `TextureInfo.Format = UseComp ? CompFormat : Format`. So a device **with** `SupportsTC` receives the raw DXT1 blocks (uploaded as
  `GL_COMPRESSED_RGB_S3TC_DXT1_EXT` by the GL drivers — the GPU decodes); a device **without** it receives `Mips` in `Format`. The engine
  never converts one into the other at runtime.

### 12.3 What the shipped content actually contains (byte evidence)
Parsed every `Assets/{Textures/*.utx, Maps/*.unr, System/*.u}` export of class `Texture` (package reader in the scratch dir:
`scan_comp.py`, then `wm_mips.py` to walk `Mips`/`CompMips` after the tagged properties; each walk ended exactly at the export's
`SerialOffset+SerialSize`, `end_ok=True`). **3,988 textures, 9 with `bHasComp=1`, all `CompFormat=DXT1`, and every one of the 9 also
ships a complete `P8` `Mips` chain of the same depth:**

| Package | Texture | `Format` | `Mips` (count, top) | `CompMips` DXT1 (count, top bytes) |
|---|---|---|---|---|
| `System/HGame.u` | `QuidMatchBoxTexture` | P8 | 1, 128×128 | 1, 8192 |
| `System/HGame.u` | `FEOptionTickCheckedOverTex` | P8 | 1, 32×32 | 1, 512 |
| `System/HGame.u` | `FEOptionTickCheckedTex` | P8 | 1, 32×32 | 1, 512 |
| `System/HGame.u` | `LumosLightIcon` | P8 | 1, 64×64 | 1, 2048 |
| `System/HProps.u` | `skChairsRedComfyTex0` | P8 | 8, 128×128 | 8, 8192 |
| `System/HProps.u` | `skChairsRedCouchTex0` | P8 | 8, 128×128 | 8, 8192 |
| `System/HProps.u` | `skhanginglampGSTex0` | P8 | 9, 256×256 | 9, 32768 |
| `System/HProps.u` | `skTableWoodRectangle2Tex0` | P8 | 8, 128×128 | 8, 8192 |
| `System/HProps.u` | `skWoodFirewoodContainerTex0` | P8 | 8, 128×128 | 8, 8192 |

No `.utx` and no map carries a compressed texture. Every other texture is uncompressed (`bHasComp=0`). So a device without
`SupportsTC` (SoftDrv, NullDrv, or a GL device on hardware lacking the extension) draws the P8 chain and loses nothing.

### 12.4 Per-target consequence and ACTION
| Target | Device(s) | S3TC path taken | ACTION |
|---|---|---|---|
| all GCC targets (native, web, MinGW) | any | `UTexture::Compress/Decompress` = stubs (`return 0`) | rekey `UnTex.cpp` L747 (and L12) `#ifdef WIN32` → `#if _MSC_VER` so MinGW takes the stub too (§8.2) — **LATER** (inert for native/web); remove `palette.cpp` from the Engine build — **WEB-1** |
| Windows | XOpenGL / UTGLR over `opengl32.dll` | `SupportsTC=1` → DXT1 blocks to the driver; every desktop GL driver and wine's Mesa expose `GL_EXT_texture_compression_s3tc` | none |
| Windows / native | SoftDrv, NullDrv | `SupportsTC=0` → P8 | none |
| Web (WebGL2) | XOpenGL | fork sets `SupportsTC = TRUE` unconditionally; if `WEBGL_compressed_texture_s3tc` is absent the 9 textures fail to upload | fork-side gate (§21 Q2) — owner `renderer-forks.md`. **WEB-1** |
| Web (WebGL1) | UTGLR + gl4es | `SupportsTC = UseS3TC`, zeroed when the extension is missing | none (verify gl4es reports the extension only when WebGL does) |

Not needed for this port, ever (unless the owner wants an in-browser editor that imports RGBA8 textures): an open DXT1 encoder
(e.g. stb_dxt) + decoder to replace `s3tc.lib`. If that day comes, implement it behind `UTexture::Compress/Decompress` for all
targets at once — not a Windows-only branch.

## 13. HP2: closed libs (MpegLib / coda / s3tc) and the UNICODE question
_(pending)_

## 14. HP2 deliverable: windows preset + toolchain plan
_(pending)_

## 15. HP2 deliverable: Core platform-file selection
_(pending)_

## 16. HP2 deliverable: sources needing PLATFORM_WIN32 rekeying
_(pending)_

## 17. HP2 deliverable: packing rules and the ABrush bColoredPad question
_(pending)_

## 18. HP2 deliverable: what ships beside Game.exe
_(pending)_

## 19. HP2 deliverable: wine smoke recipe
_(pending)_

## 20. Master checklist (WEB-1 vs LATER)
Tags: **WEB-1** = needed before (or hit by) the first native/web build because the same source/decision is shared; **B** = native-only, Phase B; **LATER** = Windows-only parity (PLAN Phase F). "Done" = already present in the repo as of commit `e47a3c7`. Line numbers are HP2's unless prefixed `T:` (template).

### 20.1 Already done (verify, do not redo)
| # | Item | Where | Status |
|---|---|---|---|
| D1 | `windows` configure + build preset (`toolchainFile`, `BUILD_STATIC/EDITOR/SOFTDRV/NULLDRV/XOPENGLDRV/NOPENGLDRV/NULLSOUNDDRV/NOPENALDRV=ON`, `USE_GL4ES=OFF`) | `Source/CMakePresets.json` L58-80, L84-90 | done (§2) |
| D2 | `cmake/toolchain-mingw-w64.cmake`, `SDL2Mingw.cmake`, `OpenALMingw.cmake` (+ `OpenALSoft.cmake`, `TargetArch.cmake`, `ArtifactDeps.cmake`) | `Source/cmake/` | done, byte-identical to the template (§3) |
| D3 | Top-level Windows block: `TARGET_IS_WINDOWS`, `-DPLATFORM_WIN32 -DWIN32 -DWINDOWS_IGNORE_PACKING_MISMATCH`, `-static-libgcc -static-libstdc++`, `-fpack-struct=4 -Wno-address-of-packed-member`, `link_libraries(ws2_32 ole32)`, `include(SDL2Mingw)` under `MINGW AND CMAKE_CROSSCOMPILING` | `Source/CMakeLists.txt` L141-177, L237-238 | done (§5, §6) |
| D4 | Core platform-file selection (`MSVC` → `UnVcWin32.cpp`; else `UnMinGW.cpp` or `UnGUID.cpp`, then `UnUnix.cpp`; `UnStaticExports.cpp` under `BUILD_STATIC`) | `Source/Core/CMakeLists.txt` L29-43 | done (§4, §15) |
| D5 | `Core/Src/UnMinGW.cpp` (`appGetSystemErrorMessage`, `appGetGUID`, `appWinSockInit`) | byte-identical to `T:Core/Src/UnMinGW.cpp` | done |
| D6 | IpDrv with the 2.33 `__WINSOCK__`/`__BSD_SOCKETS__` rekey; `ws2_32` link | `Source/IpDrv/` (verbatim copy), `IpDrv/CMakeLists.txt` L30-32 | done (§7); keep through the 433 adaptation |
| D7 | NOpenALDrv `include(OpenALMingw)` branch; launcher POST_BUILD copy of `OpenAL32.dll`; `HP2_HAS_NOPENALDRV` | `NOpenALDrv/CMakeLists.txt` L57-61; `Game/CMakeLists.txt` L44-57 | done |
| D8 | Launcher `NormalizeSubsystem` (ViewportManager/AudioDevice/NetworkDevice fall to the best real driver) for every non-web target, Windows included | `Game/Src/Launch.cpp` L343-361, L528-549 | done (copied unadapted; keep through Phase B) |
| D9 | UTGLR fork's Windows-through-SDL gate (`UTGLR_ENGINE_UT99DC` → `UTGLR_USE_SDL`, `_MSC_VER`-only `__asm`) and XOpenGL's `SDLBUILD` gate | `OpenGLDrv/CMakeLists.txt` L42-43 defines `UTGLR_ENGINE_UT99DC` **and** `UTGLR_ENGINE_HP2`; forks at `ufrontend/utglr-copy/OpenGLDrv.h` L24-45, `ufrontend/XOpenGLDrv/Inc/XOpenGL.h` L41-53 | done for Windows; the 433 render face is Phase C |

### 20.2 WEB-1 — source edits the first native/web build needs, which also close Windows gaps
| # | File:line | Edit | Why (Windows angle in *italics*) | Section |
|---|---|---|---|---|
| W1 | `Source/CMakePresets.json` L15-17, L30-32, L73-75 (all presets) | point `FETCHCONTENT_SOURCE_DIR_XOPENGLDRV/_UTGLR`, `GL4ES_SOURCE_DIR` at the real clones (or move the clones) | configure fails otherwise (§21 Q1) | §14 |
| W2 | `Editor/Src/UnParams.cpp` L25-158 | wrap `GetFVECTOR` ×2, `GetSUBSTRING`, `GetFROTATOR` ×2 in `#ifndef UNREAL_STATIC … #endif` (template L30 / L171); keep `GetBEGIN/GetEND/SetFVECTOR/GetFSCALE` | duplicate symbols against `Engine/Src/UnParams.cpp` L25-112 in every static link (*Windows is static too*) | §8 |
| W3 | `Engine/CMakeLists.txt` L10 | remove `"Src/palette.cpp"` | `Engine/Inc/Palette.h` L18 is `__declspec(dllexport) int HeckbertQuantize(…)` — not a GCC/clang keyword on Linux/emcc; its only caller is inside the MSVC-only DXT block (`UnTex.cpp` L902/L917). The template's Engine does not compile it either (*MinGW accepts `__declspec` but there is still no caller*) | §8, §12 |
| W4 | `Engine/Src/UnTex.cpp` L12 and L747 | `#ifdef WIN32` → `#if _MSC_VER` (both) | today Linux/web take the `#else` stubs (L956-966) only because nothing defines `WIN32`; *the windows preset defines `-DWIN32` (top-level L162) and would compile the real `S3TCencode/S3TCdecode/HeckbertQuantize` block → link failure against the closed `Thirdparty/s3tc/s3tc.lib`*. One-line change, do it while the file is open in Phase A | §8, §12 |
| W5 | `Engine/Inc/EngineClasses.h` L2276 (after `BITFIELD bColored:1 GCC_PACK(4);`) | add `#ifndef __MINGW32__` / `BYTE bColoredPad[3];` / `#endif` with the template's comment (T: L1865-1886) | measured (§17): `AMover`'s eight leading `BYTE`s (L2314-2321) land 3 bytes early on i386-SysV and wasm32 without it; *MinGW already places them right and the pad would overshoot by 4* | §17 |
| W6 | `Core/Inc/FFileManagerLinux.h` L234, L254, L263 (+ includes L9-11) | `__S_IREAD\|__S_IWRITE` → `S_IRUSR\|S_IWUSR`; `mkdir(path, __S_IREAD && __S_IWRITE && __S_IEXEC)` → template L287-293 (`#ifdef PLATFORM_WIN32 mkdir(path)` else `S_IRUSR\|S_IWUSR\|S_IXUSR`); add `#include <sys/stat.h>` `#include <cerrno>` | mode-1 directories break saves natively; *`__S_I*` and two-arg `mkdir` do not exist on MinGW* | §16 |
| W7 | `Core/Inc/UnGnuG.h` (whole file) | take the template's file, re-add HP2's 433 bits (PLAN §3) — the template's `PLATFORM_WIN32` blocks come with it: `__WIN32__` (T:L11), no `<alloca.h>`/`<strings.h>` (T:L27-33), `<minwindef.h>` (T:L44-46), **keep `VOID`** (T:L62-70), `STDCALL __stdcall` (T:L103-108), `GCC_HIDDEN` empty (T:L118-124), `DWORD`/`SIZE_T` from the SDK (T:L159-161, L180-182), `BITFIELD = DWORD` (T:L191-195). Drop HP2's `#error Unsupported platform` (L32), `#define HANDLE DWORD` (L79), `typedef unsigned int size_t;` (L152), `#undef VOID` (L75), the `(INT)(F)` typos (L216, L238) | HP2's file cannot compile on any of our targets as-is; *every Windows type rule of §6 lives here* | §15, §16 |
| W8 | `Core/Inc/Core.h` L107-111 | add the template's `#elif defined(PLATFORM_WIN32) typedef ANSICHARU TCHARU;` branch (T:L63-69) | *MinGW's `<tchar.h>` defines `_TCHAR_DEFINED` and `TCHAR`, but not `TCHARU`; `UnCanvas.cpp` needs it* — 3 lines, inert elsewhere | §16 |
| W9 | `SoftDrv/Src/SoftDrvPrivate.h` L10 | `#include "..\..\Render\Inc\Render.h"` → `#include "Render.h"` (the CMake include path already covers `../Render/Inc`, `SoftDrv/CMakeLists.txt` L25) | backslash include path: *MinGW tolerates it*, Linux/emcc do not | §16 |
| W10 | `Core/Src/UnUnix.cpp`, `Core/Inc/UnUnix.h` | take the template's files, re-add 433 bits (PLAN §3) — brings the seven `PLATFORM_WIN32` hunks (§4.3) and the `appWinSockInit` declaration | *without them MinGW has no `_spawnl`/`GetUserNameA`/`WSAStartup` paths and `<netdb.h>` fails* | §15 |
| W11 | `Fire/Inc/UnFractal.h` L19 | `#if !_MSC_VER` → `#if !_MSC_VER && !defined(__MINGW32__)` (template `Fire/Src/FractalPrivate.h` L20) | *`__int64` is a MinGW builtin; the typedef is a syntax error there*. One line; inert on Linux/web | §16 |

### 20.3 B — native-only fixes surfaced by this review
| # | File:line | Edit | Why |
|---|---|---|---|
| B1 | `UCC/Src/UCC.cpp` L83 vs `UCC/CMakeLists.txt` L51 | HP2's UCC selects `FFileManagerLinux` under `__LINUX_X86__`; the copied CMake defines `__LINUX__` → native falls to `FFileManagerAnsi` (L88-90, no `FindFiles` → `ucc make`/`server` cannot enumerate packages). Define `__LINUX_X86__` (or rekey the `#elif` to `PLATFORM_POSIX`) | PLAN §4 step 5 |
| B2 | `Core/Src/AudioMutex.cpp` | leave out of the build (already is); no caller | §21 Q10 |

### 20.4 LATER — Windows parity (PLAN Phase F), in order
| # | Step | Command / file | Acceptance |
|---|---|---|---|
| L1 | Configure + build (network on first configure: SDL2 2.32.10 tarball, openal-soft 1.25.2 zip) | `cd Source && cmake --preset windows && cmake --build --preset windows -j"$(nproc)"` | `build-windows/Game/Game.exe` + `OpenAL32.dll` exist |
| L2 | Import allow-list | §18.3 one-liner | only `{ADVAPI32, GDI32, IMM32, KERNEL32, msvcrt, ole32, OLEAUT32, OPENGL32, OpenAL32, SETUPAPI, SHELL32, USER32, VERSION, WINMM, WS2_32}.dll` |
| L3 | Run tree | `tools/dev/mkrun-hp2.sh build-windows run-windows` with the `*.dll *.exe` exclusion (§21 Q5), `cp -rL` for assets, our `Game.exe`+`OpenAL32.dll` copied last | `run-windows/System/Game.exe` is ours (`sha256sum`) |
| L4 | wine smoke | §19 | `Game.log`: `Init: Command line:`, `Log: Bound to Engine`, `Bringing Level … up for play`, zero `[layout]`, no `Critical:`; exit code 0 after `-EXEC=` / `exit` |
| L5 | Hardware renderers under wine | `-XOPENGLRENDERER`, `-UTGLRRENDERER` + `tools/dev/shot.sh` | menu + one map rendered (after Phase C) |
| L6 | Audio | after Phase E: `AudioDevice=NOpenALDrv.NOpenALAudioSubsystem` selected by `NormalizeSubsystem`; `OpenAL32.dll` beside the exe | `Log: NOpenALDrv:` init line, no `alGetError` spam |
| L7 | Docs | HP2 `BUILD.md` "Windows builds (MinGW-w64)" section — copy the template's L91-155 shape, **without** the stale "hardware renderers out of scope" sentence; note the MSYS2 no-DLL-copy behaviour and the `wineserver` path | — |
| L8 | Optional CI | template `.github/workflows/build.yml` L116-177 (windows job) adapted: variant from `ENGINE_VERSION 433`, artifact `hp2-433-windows`, allow-list identical | green job |
| L9 | `UnMinGW.cpp` L71-73 comment (says IpDrv calls `appWinSockInit`; it does not — `UnSocket.cpp` L77-101 does its own `WSAStartup`) | comment only | — |
| L10 | `ucc-bin.exe` | §21 Q3 | out of scope until a Windows dedicated server is wanted |

## 21. Open questions and how to verify
Everything below was left open because it needs a build, a run, or a decision the owner has not made. Each row says how to close it.

| # | Question | Why it matters | How to verify / close | Tag |
|---|---|---|---|---|
| Q1 | **Do the presets configure at all?** All five presets (`Source/CMakePresets.json` L15-17, L30-32, L73-75) point FetchContent at `${sourceDir}/../../XOpenGLDrv`, `…/utglr-copy`, `…/gl4es` = `/home/ion/hp2-ufrontend/{XOpenGLDrv,utglr-copy,gl4es}`, which **do not exist**; the clones are at `/home/ion/hp2-ufrontend/ufrontend/{XOpenGLDrv,utglr-copy,gl4es}` (branches `hp2-v433-compat` / `hp2` / `hp2`, verified with `git branch --show-current`). A `FETCHCONTENT_SOURCE_DIR_<NAME>` that names a missing directory is a configure-time error | Nothing builds until this is fixed — Windows included | Either `mv /home/ion/hp2-ufrontend/ufrontend/{XOpenGLDrv,utglr-copy,gl4es} /home/ion/hp2-ufrontend/` (PLAN §0's layout; the clones are not part of the ufrontend repo — check `git -C ufrontend status --short` shows them untracked first) **or** rewrite the nine path values to `${sourceDir}/../../ufrontend/<dir>`. Then `cmake --preset native` must pass the "XOpenGLDrv: source = …" message | **WEB-1** |
| Q2 | **Compressed textures on a `SupportsTC` device.** 9 shipped textures carry DXT1 `CompMips` (§12). `UTexture::Lock` (HP2 `Engine/Src/UnTex.cpp` L85-86) hands the DXT1 bytes to any device with `SupportsTC=1`. XOpenGL sets `SupportsTC = TRUE` **unconditionally** (`/home/ion/engines/XOpenGLDrv/Src/XOpenGL.cpp` L444) and only *logs* a missing `GL_EXT_texture_compression_s3tc` (`CheckExtensions.cpp` L293-304); UTGLR gates it properly (`utglr-copy/OpenGL.cpp` L2249 `SupportsTC = UseS3TC;` L2270 `if (!SUPPORTS_GL_EXT_texture_compression_s3tc …) SupportsTC = 0;`) | On Windows `opengl32` (real GPU) and under wine's Mesa the extension is present, so Windows is safe. On **WebGL2** it needs `WEBGL_compressed_texture_s3tc`, absent on some mobile GPUs → the four HGame front-end textures (`FEOptionTickCheckedTex`, `FEOptionTickCheckedOverTex`, `LumosLightIcon`, `QuidMatchBoxTexture`) and the five `HProps` skin textures would fail to upload there | Phase C/D: in the XOpenGL fork, gate `SupportsTC` on the extension for `ENGINE_VERSION==433` (mirror UTGLR L2270). Test: options screen (the tick-box textures) and a room with `skChairsRedComfyTex0` (any Gryffindor common-room map). Owner: `renderer-forks.md` | WEB-1 (fork side) |
| Q3 | **`ucc-bin.exe` under MinGW.** `Source/UCC/Src/UCC.cpp` L73-77 `#if WIN32` selects `FFileManagerWindows` + `FMallocWindows` (Win32 API through the `A` entry points since `UNICODE` is undefined). Never compiled by GCC before | Only matters for a Windows dedicated server / `ucc make`; the template declares `ucc.exe` out of scope (BUILD.md L155) | `cmake --build --preset windows --target UCC` after Phase B; if `FFileManagerWindows.h` fails under MinGW, key UCC's file manager on `PLATFORM_POSIX`/`PLATFORM_SDL` → `FFileManagerLinux` like the launcher does (the template's `UCC/CMakeLists.txt` L51 `__LINUX__` define is the same idea) | LATER |
| Q4 | **Thread model of the cross toolchain.** `i686-w64-mingw32-g++` here is GCC **13-win32** (the `-win32` alternative; `-posix` is also installed). The engine uses no `std::thread` (IpDrv's resolver is `CreateThread` on Windows, `IpDrvPrivate.h` L124), so `-static-libgcc -static-libstdc++` gives a binary with no `libwinpthread-1.dll` import | A future dependency (Phase E's decoder, openal-soft if ever built from source) that needs C++11 threads would silently pull `libwinpthread-1.dll` | The §18 import allow-list check; if `libwinpthread-1.dll` appears, `update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix` and add `-Wl,-Bstatic -lwinpthread` (or `-static`) | LATER |
| Q5 | **Retail DLLs shadowing the build's.** `Assets/System` ships `OpenAL32.dll` (the 2002 OpenAL 1.0 implementation), `Core.dll`, `Engine.dll`, `ogg.dll`, `vorbis*.dll`, `Game.exe`, … (§18). A run tree assembled by copying `Assets/System/*` wholesale puts the retail `OpenAL32.dll` beside our `Game.exe`; the Windows loader takes whichever copy wins the copy order | Wrong `OpenAL32.dll` = an OpenAL 1.0 router that will not answer 1.1/EFX calls; wrong `Game.exe` = the retail binary run by mistake. `.gitignore` (`*.dll`, `*.exe`) only protects git | `tools/dev/mkrun-hp2.sh` (PLAN §4 step 7) must copy `Assets/System` **minus** `*.dll *.exe *.bat *.exec` (the same list as the frontend's `manifestExclude`) and copy `build-windows/Game/{Game.exe,OpenAL32.dll}` **last**. Check: `sha256sum run/System/OpenAL32.dll build-windows/Game/OpenAL32.dll` equal | LATER (native tree: harmless, but keep one script) |
| Q6 | **`FFileManagerLinux::MakeDirectory` mode.** HP2 `Core/Inc/FFileManagerLinux.h` L263 `mkdir(path, __S_IREAD && __S_IWRITE && __S_IEXEC)` — a logical AND, i.e. mode **1** (`--x------`); `__S_I*` are glibc-private names MinGW lacks | Natively, `Save/` and `SaveSlot` dirs created by `UnGame.cpp` come out unwritable → save games fail; on MinGW it does not compile | Take the template's L282-294 form (`S_IRUSR\|S_IWUSR\|S_IXUSR`, `#ifdef PLATFORM_WIN32 mkdir(path)`), add `<sys/stat.h>`/`<cerrno>` (template L11-12), fix the two `chmod` calls (HP2 L234/L254 `__S_IREAD \| __S_IWRITE` → `S_IRUSR \| S_IWUSR`). Check after a native run: `stat -c %a run/Save` prints `700` | **WEB-1** (native first) |
| Q7 | **`APlayerStart::bEnabled:1`** (HP2 `EngineClasses.h` L3493) is an *unpacked* trailing bitfield; the template's v400 twin (L2803) produced the one "pre-existing PlayerStart" `[layout]` warning the template tolerates (BUILD.md L150 area, commit `301a9ca` VERIFIED/LINUX line) | Cosmetic unless a native subclass with a leading `BYTE` appears — none exists in HP2 (`grep 'public APlayerStart' EngineClasses.h` → nothing) | Expect one `[layout] native PlayerStart` line on first boot; ignore unless the guard names anything else | LATER |
| Q8 | **Does wine's GL suffice for XOpenGL/UTGLR on HP2?** The template screenshotted both under wine at 1024×768 (commit `76df82e`) through Mesa's WGL→GLX | Needed only to *verify* Phase C on the Windows binary without hardware | `DISPLAY=:99 WINEPREFIX=~/.wine-u1 wine Game.exe -XOPENGLRENDERER -log` + `tools/dev/shot.sh`; needs `libgl1-mesa-dri:i386` | LATER |
| Q9 | **Is `WINDOWS_IGNORE_PACKING_MISMATCH` still needed?** Defined (`Source/CMakeLists.txt` L163) but no header in mingw-w64 13 reads it (§5) | Inert on MinGW; matters only for an MSVC build, which `BUILD_STATIC` forbids anyway | Keep (harmless); note it in BUILD.md as MSVC-only | LATER |
| Q10 | **`AudioMutex`** (`Core/Src/AudioMutex.cpp`, Win32 `CRITICAL_SECTION`) — not in `Core/CMakeLists.txt`, and `grep -rn AudioMutex Engine ALAudio Render Game NSDLDrv NOpenALDrv` finds **no user** | Nothing to port; if Phase E's `NOpenALDrv` wants a lock, use `SDL_mutex` (SDL2 is linked on every target) rather than resurrecting this file | Leave out of the build; delete or keep as reference | none |
| Q11 | **MSYS2 build of the same preset.** Under MSYS2 `MINGW32`, `CMAKE_CROSSCOMPILING` is false → `-lSDL2` and `find_package(OpenAL)` are used and **no `OpenAL32.dll` is copied** (no `OPENAL_MINGW_RUNTIME_DLL`) | Documented behaviour, not a bug; pacman's `mingw-w64-i686-openal` DLL is on PATH there | Say so in HP2's BUILD.md; nobody on the team builds there today | LATER |
| Q12 | **Stripping.** `RelWithDebInfo` leaves DWARF in `Game.exe` (tens of MB) | A distributable wants `i686-w64-mingw32-strip --strip-debug` (or `objcopy --only-keep-debug` to a side file, as the template's web build keeps a symbol map) | Add to BUILD.md's Windows section; never strip the CI/wine artifact you are debugging | LATER |

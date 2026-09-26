# web-layer-template — the Emscripten/web layer of engine-ut99, end to end, as an HP2 replication checklist

**Key:** `web-layer-template` · **Run:** 2026-09-26 (fresh re-run; supersedes the 2026-09-25 draft) ·
**Template (READ ONLY):** `/home/ion/engines/engine-ut99` · **Port target:** `/home/ion/hp2-ufrontend/HarryPotter2` ·
**Frontend clone:** `/home/ion/hp2-ufrontend/ufrontend` (branch `hp2`) · **Ancestor:** `/home/ion/epic-sources/ut99-v400`

_Status: in progress — sections are appended as they are researched._

## 0. State of the HP2 repo today (web layer only)
_pending_

## 1. Build system: top-level `Source/CMakeLists.txt` (`if(EMSCRIPTEN)` + three web build modes)

Template: `/home/ion/engines/engine-ut99/Source/CMakeLists.txt` (722 lines). HP2: `Source/CMakeLists.txt` (403 lines),
already hand-written on the template (Dreamcast/PSVita/NOpenGLES removed, `UT_WEB_*` renamed `HP2_WEB_*`,
launcher dir `UnrealTournament/` → `Game/`, target `UnrealTournament` → `Game`).

### 1.1 Options that feed the web build

| # | Template (file:line) | Meaning | HP2 (file:line) | ACTION |
|---|---|---|---|---|
| 1.1.1 | `option(UT_WEB_PERF …)` `:19` | shipping link (`-O3 -g0`, assertions off, symbol map) | `option(HP2_WEB_PERF …)` `:27` | done — keep the rename consistent in every consumer (`Game/CMakeLists.txt:180`, `CMakePresets.json:34,56`) |
| 1.1.2 | `option(UT_WEB_DEBUG …)` `:20` | `-O0 -g` compile | `HP2_WEB_DEBUG` `:28` | done |
| 1.1.3 | `option(BUILD_EDITOR … ON)` `:39` (+ the 2.39 essay `:24-38`: Editor is a *runtime* dependency of content) | forced ON for web `:112` | `:35` ON, forced `:65` | done. **Verify for HP2**: grep HP2 maps/meshes for `Editor.` imports (`strings Assets/Maps/*.unr \| grep -c Editor`). Keep ON unless proven unneeded (costs a static lib only). |
| 1.1.4 | `option(BUILD_NOPENALDRV … OFF)` `:43` | real audio, forced ON for web `:130` | `:38`, forced `:70` | done (but NOpenALDrv itself is still the UT99 libxmp/.umx driver — see §7.1) |
| 1.1.5 | `option(UT_BUNDLED_OPENAL … ON)` `:44` | native Linux ships openal-soft `.so` | `HP2_BUNDLED_OPENAL` `:39` | done |
| 1.1.6 | `option(BUILD_STATIC …)` `:47` → forced ON for web `:109` | `UNREAL_STATIC` + `--whole-archive` | `:40`, forced `:63` | done |
| 1.1.7 | `_UFRONT_DEF_{NOPENGLES,XOPENGL,GL4ES}` `:57-65` | web defaults ON for all three HW renderers | `_HP2_DEF_{XOPENGL,GL4ES}` `:44-50` | done — NOpenGLESDrv intentionally absent (HP2 decision). |
| 1.1.8 | `BUILD_SOFTDRV`, `BUILD_NULLDRV` ON `:68-69` | CPU renderer / no-op renderer | `:52-53` | done |
| 1.1.9 | `USE_GL4ES` implies `BUILD_NOPENGLDRV` `:70-74` | UTGLR via gl4es | `:54-57` | done |

### 1.2 The `if(EMSCRIPTEN)` block — line by line

Template `:108-157`; HP2 `:62-85`.

| # | Template line | Why (template's own reason) | HP2 | ACTION |
|---|---|---|---|---|
| 1.2.1 | `set(BUILD_STATIC ON)` `:109` | Emscripten cannot `dlopen` UE1 `.so` packages; single binary via `UNREAL_STATIC` + whole-archive | `:63` | done |
| 1.2.2 | `set(USE_SDL ON)` `:110` | NSDLDrv is the viewport; SDL2 from `-sUSE_SDL=2` | `:64` | done |
| 1.2.3 | `set(LOW_MEMORY OFF)` `:111` | — | absent (HP2 has no `LOW_MEMORY` option) | none |
| 1.2.4 | `set(BUILD_EDITOR ON)` `:112-118` | bots/skins through Editor classes ("2.6f") | `:65` | done; see 1.1.3 verification |
| 1.2.5 | `BUILD_DCUTIL/PVRDRV/AICADRV OFF` `:119-121` | Dreamcast | absent | none (do not copy) |
| 1.2.6 | `set(BUILD_NOPENGLESDRV ON)` `:128` (+ `:125-127` "forced with set(), not option()" trap) | the 2.19 stale-cache trap | HP2 applies the same `set()` idiom to `BUILD_XOPENGLDRV`, `USE_GL4ES`, `BUILD_NOPENGLDRV` `:66-68` | done (better than template) |
| 1.2.7 | `set(BUILD_NULLSOUNDDRV ON)` `:129` | silent fallback audio | `:69` | done |
| 1.2.8 | `set(BUILD_NOPENALDRV ON)` `:130` | real audio | `:70` | done |
| 1.2.9 | `add_definitions(-DASM=0)` `:133` | portable (non-x86) code paths | `:71` | done. **Verify** every HP2 `#if ASM` site has a C fallback (HP2 Render/SoftDrv/Core `UnMath.h` carry 433 asm incl. PS2 VU0; `grep -rn '#if ASM\|#if !ASM\|__asm' Source/{Core,Engine,Render,SoftDrv,Fire}`) — owned by the core report, but a missing fallback is a web compile blocker. |
| 1.2.10 | `add_compile_options(-pthread)` `:141` | **compile**-time `-pthread` defines `__EMSCRIPTEN_PTHREADS__`, which selects the threaded branches (NSDLViewport direct WebGL ctx, XOpenGL resolver, `appRequestExit`'s `MAIN_THREAD_EM_ASM`); link-only `-pthread` silently compiles the single-threaded branch → `InvalidStateError` (Emscripten #24792). Also gives FetchContent'd trees atomics/bulk-memory. | `:76` | done |
| 1.2.11 | `add_compile_options(-fpack-struct=4 -Wno-address-of-packed-member)` `:146` | reflected C++ class sizes must equal script property sizes (Win32 `/Zp4`); wasm32 does not match by default | `:79` | done. HP2 433 relies on this even more (433 `DECLARE_CLASS` size checks). **Verify** with the template's 2.32 layout guard idea (a per-class `sizeof` vs `GetPropertiesSize()` log at boot) on the first web boot. |
| 1.2.12 | `add_compile_options(-Wno-error=non-pod-varargs -Wno-non-pod-varargs)` `:149` | clang rejects FString through `debugf`/`appErrorf` varargs | `:81` | done |
| 1.2.13 | Exceptions note `:150-156`: engine objects stay `-fignore-exceptions` (Emscripten default); `-fexceptions` overflows V8's stack via `invoke_` trampolines through the deep package loader; `-fwasm-exceptions` is incompatible with ASYNCIFY; openal-soft alone is `-fexceptions` | `:82-84` (comment only; behaviour = default) | done. **Do not** add `-fexceptions`/`-fwasm-exceptions` to engine targets. Consequence: every `throw` in HP2 Core/Engine is a module abort → §5 patch list. |

### 1.3 The three web build modes (compile side)

Template `:346-372` (`UT_WEB_DEBUG` → `-O0 -g`; `UT_WEB_PERF` → `-O2`; else `-O2 -g`), with the ufront 2.54
"do not tidy the duplicate `-O`" warning `:373-386` (the per-config `CMAKE_CXX_FLAGS_<CFG>` is where `-DNDEBUG`
comes from; blanking it turns `assert()` on engine-wide). HP2 `:194-206` — identical logic, warning comment dropped.

| Preset | Template cache vars (`CMakePresets.json`) | HP2 cache vars | compile | link (launcher) |
|---|---|---|---|---|
| `wasm` (dev default) | `CMAKE_BUILD_TYPE=RelWithDebInfo`, `UT_WEB_PERF=OFF`, `UT_WEB_DEBUG=OFF`, `SOFTDRV_MAX_SCREEN=8192` `:30` | `HP2_WEB_PERF=OFF`, `HP2_WEB_DEBUG=OFF`, `SOFTDRV_MAX_SCREEN=8192` + 3 `FETCHCONTENT_SOURCE_DIR_*`/`GL4ES_SOURCE_DIR` `:32-40` | `-O2 -g` | `-g -sASSERTIONS=1` |
| `wasm-debug` | inherits `wasm`, `UT_WEB_DEBUG=ON` `:38` | `HP2_WEB_DEBUG=ON` `:48` | `-O0 -g` | `-g -sASSERTIONS=1` |
| `wasm-perf` (shipping) | inherits `wasm`, `CMAKE_BUILD_TYPE=Release`, `UT_WEB_PERF=ON` `:46` | same with `HP2_WEB_PERF` `:56` | `-O2` | `-O3 -g0 -sASSERTIONS=0 --emit-symbol-map` |

ACTIONS:
- 1.3.1 Keep the three presets exactly. **Restore the 2.54 comment** (`-O` duplicate + `NDEBUG` provenance) into HP2 `Source/CMakeLists.txt` near `:194`, so nobody "tidies" `CMAKE_CXX_FLAGS_<CFG>` away.
- 1.3.2 Both trees need `source ~/emsdk/emsdk_env.sh` first (the preset's `toolchainFile` is `$env{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`).
- 1.3.3 `SOFTDRV_MAX_SCREEN=8192` is passed by HP2's `wasm` preset but **HP2's SoftDrv sources never read it** (`grep -rn SOFTDRV_MAX_SCREEN Source/SoftDrv/Src` → nothing; only `SoftDrv/CMakeLists.txt:37-38` define it). Either port the template's 2.19f static-table bound into HP2's own SoftDrv (template `Source/SoftDrv/Src/SoftDrvPrivate.h`) or accept the inert define; **verify** HP2 SoftDrv's own fixed-size tables survive a 5120-px device-pixel canvas (template 2.71 finding: the web canvas is the display's *device* pixel size and UE1's resize path bypasses `SetRes`'s clamp).

### 1.4 SDL2 and fetched dependencies

| # | Template | HP2 | ACTION |
|---|---|---|---|
| 1.4.1 | `SDL2_LIBRARY` empty on Emscripten `:415-421` (explicit `-lSDL2` links the non-mt archive → "`--shared-memory is disallowed by SDL_atomic.c.o`") | `:233-236` | done |
| 1.4.2 | XOpenGLDrv fetch `:447-457` (`10-p/XOpenGLDrv@ut99-v400-compat`) | `:252-262` (`@hp2-v433-compat`) + preset override `FETCHCONTENT_SOURCE_DIR_XOPENGLDRV=${sourceDir}/../../XOpenGLDrv` (= `/home/ion/hp2-ufrontend/XOpenGLDrv`) | **BLOCKER (today): the three forks are cloned in the WRONG place** — they sit *inside the ufrontend clone* as untracked dirs: `/home/ion/hp2-ufrontend/ufrontend/XOpenGLDrv` (branch `hp2-v433-compat`, tip `87f6e89`), `/home/ion/hp2-ufrontend/ufrontend/utglr-copy` (branch `hp2`, `e60807b`), `/home/ion/hp2-ufrontend/ufrontend/gl4es` (branch `hp2`, `98441ed5`); `git -C /home/ion/hp2-ufrontend/ufrontend status` lists all three as `??`. The HP2 presets resolve `${sourceDir}/../../X` to `/home/ion/hp2-ufrontend/X`, which does not exist → configure fails. ACTION: `mv` the three dirs up one level to `/home/ion/hp2-ufrontend/` (never symlink `/home/ion/engines/*`), and make sure nobody `git add`s them into the ufrontend clone. Also reconcile gl4es: the clone is on branch `hp2`, while HP2's CMake default `GL4ES_GIT_TAG` is `fix/cubemap-bind-active-unit` (`Source/CMakeLists.txt:313`) — pick one and make the default match the clone. |
| 1.4.3 | UTGLR fetch `:459-469` (`10-p/utglr-copy@main`) | `:264-274` (`@hp2`) | as above |
| 1.4.4 | gl4es `:525-560`: `ptitSeb/gl4es@master`, `NOX11 NOEGL STATICLIB DEFAULT_ES=2 NO_INIT_CONSTRUCTOR`, `CMAKE_POLICY_VERSION_MINIMUM 3.10`, archive redirected to `${CMAKE_BINARY_DIR}/lib`, alias `gl4es::GL` | `:307-343`: **`10-p/gl4es@fix/cubemap-bind-active-unit`** (a fork branch, not upstream) | HP2 deviates on purpose? Not documented. **ACTION:** record the choice in HP2 `BUILD.md`; `meta.json` will carry it (`ArtifactDeps` reads `GL4ES_GIT_REPOSITORY/TAG`, or `local:` when `GL4ES_SOURCE_DIR` is set — which HP2's presets always set, so every HP2 artifact will say `gl4es: local:/…/gl4es`). |
| 1.4.5 | `WebRTCDrv` only on web `:494-497` | `:295-298` | done |
| 1.4.6 | `UCC` never on web `:488-490` | `:289-291` | done |
| 1.4.7 | `UWeb` in `INSTALL_TARGETS` `:476` (2.38: UT99 ships `UWeb.u`) | not present | correct — **do not copy** (HP2 ships no `UWeb.u`; §14) |
| 1.4.8 | openal-soft `ufront_openal_soft(PLATFORM WEB EAX OFF)` `:614-615` | `:367-368` | done |
| 1.4.9 | `IpDrv` always built+linked (also on web) `:476,484` | `:279,286` | done — needed on web because `WebRTCDrv` auto-switch starts from the ini's `IpDrv.TcpNetDriver` and `AUdpLink` → `WebRTCLan`. Whether HP2 needs IpDrv at all on web: only if any HP2 script instantiates `IpDrv.*` (`grep -l IpDrv Assets/System/*.u` → verify). Keeping it costs little. |

## 2. Launcher CMake: `Source/UnrealTournament/CMakeLists.txt` web link (flags, preloads, LINK_DEPENDS, meta)

Template: `Source/UnrealTournament/CMakeLists.txt` (303 lines), the web block is `:94-303`. HP2: `Source/Game/CMakeLists.txt`
(284 lines), web block `:75-284`. A normalised diff (`UT_WEB_PERF→HP2_WEB_PERF`, `UT_HAS_NOPENALDRV→HP2_HAS_NOPENALDRV`)
shows HP2 = template minus Dreamcast, with only these differences: `project(Game)`, `HP2_ASSETS_DIR` (defaults to
`${CMAKE_SOURCE_DIR}/../Assets`, unused by the link — the template's `UT99_ASSETS_DIR` is equally unused), comment
text `Game.ini`, and `--target=hp2`. Every link flag is therefore already present; this section is the *why* table to
keep them correct, plus the HP2-specific fix-ups.

### 2.1 Non-web parts that the web link depends on

| # | Template line | What | HP2 | ACTION |
|---|---|---|---|---|
| 2.1.1 | `:26-28` | under Emscripten do **not** `target_link_libraries(... NSDLDrv)` — it is already in the whole-archive set; wasm-ld rejects the duplicate (GNU ld tolerates it) | `Game/CMakeLists.txt:22-24` | done |
| 2.1.2 | `:39-49` | `LINK_PACKAGES = INSTALL_TARGETS - self`, `-Wl,--whole-archive … --no-whole-archive` | `:31-34` | done. NB: `INSTALL_TARGETS` includes `OpenAL`? **No** — openal-soft is linked through `NOpenALDrv`'s `target_link_libraries(... OpenAL)`, not whole-archived. Keep it that way (whole-archiving openal-soft would pull its utils/tests objects). |
| 2.1.3 | `:59-61` `UT_HAS_NOPENALDRV=1` | launcher knows the real audio driver exists (Windows ini normalisation, web override) | `:44-46` `HP2_HAS_NOPENALDRV=1` | **HP2 `Source/Game/Src/Launch.cpp` is byte-identical to the template and still tests `UT_HAS_NOPENALDRV`** (see §4) — rename the macro in Launch.cpp or the CMake define; today the define is dead and the ifdef'd code is compiled out on Windows. |

### 2.2 The web link flags (`target_link_options(... PRIVATE ...)`, template `:97-198`, HP2 `:78-179`)

| # | Flag | Why (template's reason, condensed) | Keep for HP2? |
|---|---|---|---|
| 2.2.1 | `-sASYNCIFY` | main-loop inversion: the blocking UE1 `MainLoop` yields via `emscripten_sleep` each frame (§4.2) | YES |
| 2.2.2 | `-sASYNCIFY_STACK_SIZE=262144` | Asyncify unwind buffer; UE1 frames are deep | YES (raise if HP2 hits "unreachable"/asyncify stack overflow in a deep script call chain at the yield point — only the stack *at `emscripten_sleep`* is saved, which is shallow in the main loop) |
| 2.2.3 | `"-sASYNCIFY_IMPORTS=['emscripten_sleep','emscripten_set_main_loop','__syscall_openat']"` | the only imports allowed to unwind; `__syscall_openat` listed for FetchFS on-demand open ("ABI-consistent with the siblings") | YES, verbatim (the frontend glue's FetchFS assumes it) |
| 2.2.4 | `-sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1` | one binary creates **either** a WebGL1 (UTGLR/gl4es, SoftDrv blit) or WebGL2 (XOpenGL) context at runtime; NSDLViewport picks by render-device name | YES |
| 2.2.5 | `-sGL_ENABLE_GET_PROC_ADDRESS=1` | glad (XOpenGL) and gl4es resolve entry points by name | YES |
| 2.2.6 | `-sFULL_ES2=1` | **required for UTGLR**: gl4es emits client-side vertex arrays that raw WebGL rejects; FULL_ES2 emulates them with temp VBOs; harmless for XOpenGL | YES |
| 2.2.7 | `-sUSE_SDL=2` | SDL2 port (picks `libSDL2-mt.a` under `-pthread`) | YES |
| 2.2.8 | `-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456` | 256 MB initial, growable | YES. HP2's content is bigger per level than UT99 DM maps (cut-scenes, dialogue sounds); growth handles it. Note growth + pthreads = every HEAP view must be refreshed on grow (Emscripten does it for its own glue; custom JS must use `HEAPU8` via `Module` getters). |
| 2.2.9 | `-sSTACK_SIZE=2097152` | 2 MB C stack for the engine thread (deep loader + script VM) | YES. **Verify** HP2's script VM depth (HP2 cut-scene/dialog state code). If a boot dies with "stack overflow" (assertions build) raise to 4 MB; ASSERTIONS=1 in the dev build detects it. |
| 2.2.10 | `-sFORCE_FILESYSTEM` | WasmFS otherwise omits the JS `FS.*` API that the host glue uses for user data / mods / file browser | YES |
| 2.2.11 | `-sWASMFS` | VFS in wasm; the FetchFS seam is a WasmFS **JSImpl backend** (`vendor/glue/wasmfs-shim.js` → host `Module.UE.fs.backend`) | YES |
| 2.2.12 | `"-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_close','_ue_exec_command','_wasmfs_create_jsimpl_backend','_wasmfs_create_file','_ufront_wasmfs_register','_ufront_wasmfs_set_pending']"` | host stands the FS backend up from JS; `ue_exec_command` is the ABI's console entry (§8). **No `_webrtc_deferred_open`** — UE1 engines don't use the deferred-connect pair; exporting an undefined symbol is a hard wasm-ld error | YES, verbatim. `_ue_exec_command` must be defined by HP2's launcher (§4.6). `_ufront_wasmfs_*` are defined by the vendored shim (§8). |
| 2.2.13 | `"-sEXPORTED_RUNTIME_METHODS=['callMain','stringToUTF8','UTF8ToString','HEAPU8','stackSave','stackAlloc','stackRestore','lengthBytesUTF8','PThread']"` | `PThread` is needed so the host can hand the FS sync channel to the worker running `main()`; omitting it = silent no-op + death on first asset read | YES, verbatim |
| 2.2.14 | `"-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['$ueWorkerBoot']"` | the worker bootstrap in `wasmfs-shim.js` hangs off a `__postset` that is dead-stripped silently unless referenced | YES, verbatim |
| 2.2.15 | `-pthread -sPROXY_TO_PTHREAD` | engine `main()` runs on a worker (decision D2): OPFS `FileSystemSyncAccessHandle` + `Atomics.wait` are worker-only | YES |
| 2.2.16 | `-sOFFSCREENCANVAS_SUPPORT` + `"-sOFFSCREENCANVASES_TO_PTHREAD=#canvas"` (**quoted**: an unquoted `#` is a CMake comment → "string index out of range") | transfers `#canvas` to the worker so NSDLViewport's direct `emscripten_webgl_create_context` is legal there | YES, keep the quoting |
| 2.2.17 | `-sOFFSCREEN_FRAMEBUFFER` (ufront 2.43d) | without it `emscripten_webgl_commit_frame` is a stub; Windows/D3D11 Chromium then showed ~1 fps while the engine ran at 80 fps | YES |
| 2.2.18 | `-sPTHREAD_POOL_SIZE=4` | main() + openal-soft's mixer thread + its event thread (= 3) + 1 spare; a thread created with an empty pool starts late (audio underrun) | YES (4). **HP2 note:** HP2's Core has `FFileStream` (`Core/Src/UnMisc.cpp:1611-1760`, created by `appInit` via `FFileStream::Init(Channels)` at `:1996`), whose pump thread `FileStreamingThread` is started **only by the Win32 core** (`Core/Src/UnVcWin32.cpp:665-690, 1489` `CreateThread`); HP2's `UnUnix.cpp` starts none and `FFileStream::Destroy` has `#if !__UNIX__ //!!TODO: vogel: implement`. If the audio port makes FFileStream live on POSIX with a pthread, that is a 4th long-lived thread → raise the pool to 5. Preferred for web: no new thread — pump streams from `UAudioSubsystem::Update()` on the engine thread (the template NOpenALDrv's "single music path, no background thread" rule). |
| 2.2.19 | `-sNO_DISABLE_EXCEPTION_CATCHING` | openal-soft throws/catches AL errors; only its objects are `-fexceptions` | YES |
| 2.2.20 | `"SHELL:--js-library …/vendor/glue/webrtc-shim.js"` | defines the ABI's C-imported `webrtc_*` symbols (forwarders to `Module.UE.net.*`) | YES |
| 2.2.21 | `"SHELL:--js-library …/vendor/glue/wasmfs-shim.js"` | FetchFS JSImpl backend + `$ueWorkerBoot` | YES |
| 2.2.22 | perf: `-O3 -g0 -sASSERTIONS=0 --emit-symbol-map` (`:199-209`) | shipping; the symbol map is a side file `SDLLaunch.js.symbols` that must **never** reach the published tree | YES. Verified safe on both sides: the map is written inside `build-wasm-perf/` (HP2 `.gitignore` ignores `/build-*/`), and `ufrontend/packages/build/bin/ufront-build.mjs` copies only `SDLLaunch.{wasm,js,data}` into the served root and archives the map to `ufrontend/.ufront/symbols/<target>-<gitSha>.js.symbols` (never published). |
| 2.2.23 | dev/debug: `-g -sASSERTIONS=1` (`:210-212`) | named stacks, readable errors | YES |

### 2.3 Preloaded config (the only baked data)

Template `:213-227`:
```cmake
foreach(_ini Default.ini DefUser.ini)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../Config/System/web/${_ini}")
    set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS
      " --preload-file ${CMAKE_SOURCE_DIR}/../Config/System/web/${_ini}@/System/${_ini}")
  endif()
endforeach()
```
HP2 `Game/CMakeLists.txt:203-208` — identical. **Today HP2's `Config/System/web/` contains only `README-ut99.md`**, so
the `if(EXISTS)` silently skips both inis: the link succeeds, `SDLLaunch.data` is not produced (meta.json then has no
`checksums.data`), and `appInit` aborts at boot with `MisingIni` (`Core/Src/UnMisc.cpp:1926`).

ACTIONS:
- 2.3.1 Author `Config/System/web/Default.ini` and `DefUser.ini` (§9) **before** the first web link.
- 2.3.2 Replace the silent `if(EXISTS)` with a hard check in HP2 (`if(NOT EXISTS …) message(FATAL_ERROR "web ini missing: …")`) — a deviation from the template that turns a runtime `MisingIni` into a configure error. (Optional; document it if done.)
- 2.3.3 **HP2-specific: the lowercase `default.ini` read.** HP2's `appInit` opens a *second* config cache on the literal
  name `"default.ini"` (`Core/Src/UnMisc.cpp:1955-1963`, "gk 10/15/01"):
  ```cpp
  FConfigCache* DConfig=ConfigFactory();
  DConfig->Init(TEXT("default.ini"),GUserIni,RequireConfig);
  GConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), Temp, ARRAY_COUNT(Temp) );
  DConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), DTemp, ARRAY_COUNT(DTemp));
  GConfig->SetString( TEXT("Engine.Engine"), TEXT("Language"), DTemp );
  ```
  On a case-sensitive FS (WasmFS MEMFS, Linux) `default.ini` ≠ the preloaded `/System/Default.ini`, `DConfig->GetString`
  fails, and **`DTemp` is uninitialised** — the engine language becomes stack garbage → every `Localize()` lookup
  misses (`<?int?…?>` strings, `LocalizeError` returns garbage). ACTION (pick one, record it): (a) patch to
  `TEXT("Default.ini")` and initialise `DTemp[0]=0` + only `SetString` on success — preferred, platform-neutral;
  or (b) also preload the same file as `@/System/default.ini`. Do (a).
- 2.3.4 `appInit` derives `GIni = appUserDir() + InPackage + ".ini"` (`UnMisc.cpp:1913-1915`) and `GUserIni = appUserDir()+"User.ini"` (`:1933-1935`). **HP2's Unix core has no `appUserDir()`** (defined only in `Core/Src/UnVcWin32.cpp:1048`, which returns `My Documents\Harry Potter and the Chamber of Secrets\`). HP2's `UnUnix.cpp` must define `appUserDir()` and on web it must return `""` (so `Game.ini`, `User.ini`, the log and `Save/` land where the frontend persists them — see §9.6/§10.5). Returning `""` also selects `GSys->SavePath = "..\\Save"` (`UnMisc.cpp:2002-2011`) — a **backslash** path; verify HP2's POSIX file manager normalises `\` → `/` (template `FFileManagerLinux` does via `appStrrchr`/fixups? — check `Core/Inc/FFileManagerLinux.h` in HP2) or set `SavePath=../Save` explicitly on POSIX.
- 2.3.5 The derived ini name depends on the package string passed to `appInit` — the template passes `TEXT("UnrealTournament")` (`Launch.cpp:883`), HP2's Win32 launcher passes `GPackage=appPackage()` = `"Game"` (`Source/Launch/Src/Launch.cpp:18,83,141`; the retail exe is `System/Game.exe`). HP2's SDL launcher must pass **`TEXT("Game")`** so the derived file is `Game.ini` (and the log `Game.log`), matching the frontend's `targets/hp2` descriptor (§10.5).

### 2.4 `LINK_DEPENDS` (the three stale-artifact traps)

| # | Template line | Input | HP2 | ACTION |
|---|---|---|---|---|
| 2.4.1 | `:230-232` | `Config/System/web/{Default,DefUser}.ini` — editing the web config must relink (else stale `.data`) | `:211-213` | done (but see 2.3.1: the files must exist; a LINK_DEPENDS on a missing file makes make fail "No rule to make target" — **this is actually what will happen today**, so the link does fail, just with a confusing message) |
| 2.4.2 | `:237-239` | `vendor/glue/{webrtc,wasmfs}-shim.js` | `:218-220` | done |
| 2.4.3 | `:292-293` | `cmake/emit-artifact-meta.mjs` (else stale `meta.json`) | `:273-274` | done |

### 2.5 Output name and location

Template `:244-247` → HP2 `:225-228`:
```cmake
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME SDLLaunch SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/SDLLaunch)
```
`SDLLaunch` is the **frozen cross-engine served-loader basename** (the frontend's `boot.ts` loads `SDLLaunch.js`). Outputs:
`build-wasm{,-debug,-perf}/SDLLaunch/SDLLaunch.{js,wasm,data}` + `meta.json` (+ `SDLLaunch.js.symbols` on perf).
HP2: done. The target is called `Game` natively (`build-native/Game/Game`) and `SDLLaunch` on the web — keep that split.

### 2.6 POST_BUILD steps (order matters)

1. `offscreen-depth24.sh` on `SDLLaunch.js` (template `:254-256`, HP2 `:235-237`) — §3.4.
2. `emit-artifact-meta.mjs` (template `:294-302`, HP2 `:275-283`) — runs **after** the loader is final so the js checksum is of the served file. §3.3/§10.

Both are POST_BUILD on the same target; CMake runs them in declaration order (depth first, then meta). HP2 keeps that order. Done.

### 2.7 Configure-time inputs to `meta.json`

| Input | Template | HP2 | Note |
|---|---|---|---|
| `UE_ABI_VERSION` from `vendor/glue/ue_abi.h` (`file(STRINGS … REGEX "define[ \t]+UE_ABI_VERSION")`) `:261-262` | same `:242-243` | single source of truth — never hardcode |
| `META_GIT_SHA` (`git -C ${CMAKE_SOURCE_DIR}/.. rev-parse --short HEAD`) `:263-264` | same | fallback only; the emitter re-resolves at build time |
| `META_EMCC` from `${EMSCRIPTEN_ROOT_PATH}/emscripten-version.txt` `:266-272` | same | |
| `META_DEPS` via `ufront_artifact_dep()` for openal-soft, **libxmp**, xopengl, utglr, gl4es `:275-285` | same `:256-266` | **HP2 ACTION:** drop the `libxmp` line once NOpenALDrv stops fetching libxmp (HP2 music is Ogg Vorbis, §7.1) and add the Vorbis decoder dep instead (e.g. `ufront_artifact_dep(META_DEPS vorbis …)` if fetched; nothing if the Emscripten `-sUSE_VORBIS=1` port is used — then record it in `buildFlags`). |
| `find_program(NODE_EXECUTABLE node REQUIRED)` `:286` | same | node on PATH at configure |

## 3. `cmake/ArtifactDeps.cmake`, `cmake/OpenALSoft.cmake`, `emit-artifact-meta.mjs`, `offscreen-depth24.sh`

State: `diff -r engine-ut99/Source/cmake HP2/Source/cmake` → **identical** (`ArtifactDeps`, `OpenALSoft`,
`OpenALMingw`, `SDL2Mingw`, `TargetArch`, `toolchain-mingw-w64`). `Source/Game/cmake/emit-artifact-meta.mjs` and
`Source/Game/offscreen-depth24.sh` are **byte-identical** to the template's.

### 3.1 `cmake/ArtifactDeps.cmake` (34 lines) — keep verbatim

`ufront_artifact_dep(<list> <label> <fetchcontent-name> <repo> <ref> [<local-source-dir>])` appends
`--dep-<label>=<repo>@<ref>` or `--dep-<label>=local:<dir>` (when `FETCHCONTENT_SOURCE_DIR_<UPPER(name)>` or the
6th argument is set). Refs are branches/tags, never commits (owner's rule: rebuild = latest tip); a branch is
re-fetched on every **configure**, never on a bare `cmake --build` (`:16-20`).

HP2 consequence: HP2's presets **always** set `FETCHCONTENT_SOURCE_DIR_XOPENGLDRV`, `FETCHCONTENT_SOURCE_DIR_UTGLR` and
`GL4ES_SOURCE_DIR`, so every HP2 `meta.json` will record `xopengl`, `utglr`, `gl4es` as `local:/home/ion/hp2-ufrontend/…`.
That is correct and honest (§1.4.2) — do not "fix" it; when the forks' `hp2` branches are pushed, a publish preset
without the overrides will record `10-p/XOpenGLDrv@hp2-v433-compat` etc.

### 3.2 `cmake/OpenALSoft.cmake` (108 lines) — keep verbatim

`ufront_openal_soft(PLATFORM WEB|NATIVE EAX ON|OFF)` → target `OpenAL`. WEB: `10-p/openal-soft-web@ufront/web`
(openal-soft 1.25.2 + Web Audio/AudioWorklet backend), STATIC, `ALSOFT_BACKEND_WEB ON`, `ALSOFT_DLOPEN OFF`,
`ALSOFT_CPUEXT_NEON OFF`, `ALSOFT_REQUIRE_SSE4_1 ON`, `-msimd128 -msse4.1`, **`-fexceptions` for its C++ only**,
utils/examples/tests/install OFF, `ALSOFT_EMBED_HRTF_DATA ON`. It strips the engine's `-fpack-struct=4
-Wno-address-of-packed-member` from **every** target in its tree incl. subdirectories (`:78-98`; its bundled `fmt`
bit a first version), and removes clang-compat flags from `CMAKE_CXX_FLAGS` for its configure (`:54-55`).

HP2 ACTIONS:
- 3.2.1 Keep `EAX OFF` for the web **unless** the HP2 audio port decides to emulate ALAudio's EAX calls through openal-soft's
  EAX layer. HP2's shipped `ALAudio` uses EAX (`Thirdparty/OpenAL/eax.lib`, `EAXSet/EAXGet`); the template's NOpenALDrv uses
  EFX `AL_EFFECT_EAXREVERB` instead. If the HP2 NOpenALDrv keeps HP2's EAX preset calls, flip to `EAX ON` (template comment
  `Source/CMakeLists.txt:612-613`: "Flip it if a driver ever needs EAXSet/EAXGet (engine-ut2004's ALAudio does)").
- 3.2.2 The call must stay **after** the global `add_compile_options(-pthread …)` (HP2 `Source/CMakeLists.txt:365-375` is after `:76` — OK).

### 3.3 `emit-artifact-meta.mjs` (81 lines) — ADAPT (it is still the UT99 one)

What it writes (template `:59-77`), field by field, with what HP2 must set:

| Field | Template value | Source | HP2 value / ACTION |
|---|---|---|---|
| `targetId` | `args.target \|\| 'ut99'` | `--target=` from CMake | CMake passes `--target=hp2` (`Game/CMakeLists.txt:278`) ✔. **Change the fallback** `'ut99'` → `'hp2'` so a hand-run emitter cannot mislabel. |
| `engineFamily` | `'UE1'` | const | `'UE1'` (build 433 is UE1; `ENGINE_FAMILIES=['UE1','UE2']` in `ufrontend/packages/glue/src/artifact/meta.js:13`) |
| `buildConfig` | `'wasm-integrated'` | const | same (enum `BUILD_CONFIGS`, `meta.js:14`) |
| `abiVersion` | `Number(args.abi)` | `ue_abi.h` `UE_ABI_VERSION` (=1) | same; validated `=== glue ABI_VERSION` (`glue/src/shim/index.js:16` = 1) |
| `loaderStyle` | `'global-module'` | const | same — the only `HOST_SUPPORTED_LOADERS` value (`meta.js:21`); **never add `-sMODULARIZE`/`-sEXPORT_ES6`** |
| `provenance.repo` | `'engine-ut99'` | const | **→ `'HarryPotter2'`** (the repo dir basename; `checkEngineIdentity` in `ufrontend/packages/build/src/publish/engine.mjs:162-180` compares the *repo basename* with `target.json publish.engine.repo`, and `build-inputs.mjs:94` derives the artifact path `<enginesRoot>/<repo>/build-wasm-perf/SDLLaunch` from the same field). Keep the descriptor and this string equal. |
| `provenance.gitSha` | re-resolved at build time via `git -C <emitter dir> rev-parse --short HEAD`, fallback `--gitSha` | — | same. Verified: HP2 is a git repo (`git rev-parse --short HEAD` → `e47a3c7`, 3 commits), so the emitter resolves a real sha. A dirty tree is refused by `ufront-publish` (`engineArtifactCurrent`, `engine.mjs:103`). |
| `provenance.builtAt` | `new Date().toISOString()` sans ms | — | same (publish checks it is newer than the run start, `engine.mjs:143-150`) |
| `toolchain.emscripten` | `--emcc` | `emscripten-version.txt` | same |
| `toolchain.base` | `'maximqaxd/ut99dc (UnrealTournament v400)'` | const | **→ `'Harry Potter and the Chamber of Secrets (UE1 build 433), templated on engine-ut99'`** (free text, not validated) |
| `toolchain.<dep>` | from `--dep-*` args | `ArtifactDeps` | automatic; drop `libxmp` when HP2's NOpenALDrv stops using it (§2.7) |
| `buildFlags` | hand-maintained list: `-sASYNCIFY -sFULL_ES2 -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH -sFORCE_FILESYSTEM -sWASMFS -pthread -sPROXY_TO_PTHREAD -sOFFSCREENCANVAS_SUPPORT -sOFFSCREENCANVASES_TO_PTHREAD=#canvas` | const | keep in step with the link block. Template omits `-sOFFSCREEN_FRAMEBUFFER` (2.43d) — **add it** in HP2's copy (descriptive only; nothing validates it). If Vorbis comes from `-sUSE_VORBIS=1 -sUSE_OGG=1`, list those too. |
| `exports.symbolContract` | `1` | const | same |
| `checksums.{wasm,js,data?}` | sha256 of `SDLLaunch.{wasm,js,data}` | — | same; `data` only if `SDLLaunch.data` exists (it will once §9 inis exist) |

Also update the header comment (`:3`, `:7` mention UnrealTournament/"Launch.*") — cosmetic.

### 3.4 `offscreen-depth24.sh` (29 lines) — keep verbatim

Rewrites the emitted loader so `-sOFFSCREEN_FRAMEBUFFER`'s emulated default framebuffer uses `DEPTH_STENCIL`
(`34041`, D24S8) on `DEPTH_STENCIL_ATTACHMENT` (`33306`) instead of `DEPTH_COMPONENT16` (`33189`) on `DEPTH_ATTACHMENT`
(`36096`); asserts **exactly 2 storage + 1 attachment** rewrites or fails the build (toolchain drift detector).
Affects every device drawing to FBO 0 (UTGLR/gl4es, SoftDrv's blit path, NOpenGLES); XOpenGL owns its own FBO.
HP2: keep; it runs POST_BUILD before the meta emitter (§2.6). If a newer emsdk changes the patterns the build fails
loudly — update the patterns, never delete the assert.

## 4. Launcher source: `Launch.cpp` `__EMSCRIPTEN__` paths

Template: `Source/UnrealTournament/Src/Launch.cpp` (938 lines). HP2: `Source/Game/Src/Launch.cpp` — **byte-identical**
(`md5sum` both `9de4607c5e7c99e09c6738a7f86fb3fd`), i.e. not adapted at all. Every row below is therefore an HP2 action.

> Note on the brief: it mentions "EM_ASM status lines" and "FetchFS 'Received' lines" in `Launch.cpp`. Neither exists in
> the current template launcher, nor in engine-unreal1's `Source/Unreal/Src/SDLLaunch.cpp` (grep for `EM_ASM`/`setStatus`/
> `Received` finds nothing in either launcher). The only JS-bridging in the launcher is `EM_ASYNC_JS(ufront_yield_frame)`;
> the other two EM_ASM sites are in `UnUnix.cpp` (`appRequestExit`) and `NSDLViewport.cpp` (§6). The `Received '<pkg>'`
> line is a `debugf` in `Engine/Src/UnChan.cpp:1322-1326` (UFileChannel, net downloads, ufront 2.79) — see §5.4. Loader
> status/progress text comes from the host glue, not the engine.

### 4.1 Web-only globals and exports (template `:13-146`)

| # | Template | What it is | HP2 ACTION |
|---|---|---|---|
| 4.1.1 | `:13-16` `#include <emscripten.h>`, `<emscripten/heap.h>`, `<unistd.h>` | | keep |
| 4.1.2 | `:47-49` `EM_ASYNC_JS(void, ufront_yield_frame, (void), { await new Promise(r=>setTimeout(r,0)); })` | the per-frame **present point** when no cap budget remains: a *timer* task (2.43 measured: MessagePort resumes lost 44 % of frames; rAF couples to refresh). Its import `__asyncjs__ufront_yield_frame` is auto-registered with Asyncify (default `ASYNCIFY_IMPORTS` includes `__asyncjs__*`) | keep verbatim |
| 4.1.3 | `:54` `UEngine* GWebRTCEngine = NULL;` | set after `Engine->Init()`; used by `WebRTCDrv.cpp` (`extern`) for listen-status push and by `ue_exec_command` | keep (name is part of the WebRTCDrv contract; rename only together with `WebRTCDrv/WebRTCDrv.cpp`) |
| 4.1.4 | `:56-114` pacing ring `GUfrontPace[4096]`, `UfrontPaceRecord`, `UfrontPaceDump` (CSV `ufront_pacing v3`) | frame-interval diagnostics read by `ufrontend/packages/e2e/tools/pacing-probe.cjs`; `start_s` is `appSeconds()` | keep; **type-adapt**: HP2 `appSeconds()` returns `FTime` (`Core/Inc/UnGnuG.h:263-271`, `Core/Inc/Core.h:24-47`), not `DOUBLE`. `FTime` has `GetFloat()` (float!) and `operator-(FTime)→float`, no conversion to double. Use `appSeconds().GetFloat()` or (better, precision) `appSecondsSlow()`-based double helper. See 4.3.4. |
| 4.1.5 | `:121-146` `extern "C" char* EMSCRIPTEN_KEEPALIVE ue_exec_command( const char* Cmd )` | **ABI entry** (`vendor/glue/ue_abi.h` §8): runs a console command through `GWebRTCEngine->Exec(Cmd, FStringOutputDevice)` and returns the captured text (static buffer, valid until next call); `ufront_pacing [reset]` answered locally | keep; HP2 `TCHAR` is `char` on the GCC path (`Core/Inc/Core.h:108-110`, `_UNICODE` undefined) so the `const char*`↔`TCHAR*` pass-through is valid. HP2 `UEngine::Exec(const TCHAR*, FOutputDevice&)` and `FStringOutputDevice` (`Core/Inc/UnTemplate.h:1343`) exist. |

### 4.2 Device globals (template `:152-175`)

`FMallocAnsi`, `FOutputDeviceFile Log`, `FOutputDeviceAnsiError Error`, `FFeedbackContextAnsi Warn`, `FFileManagerLinux FileManager`,
`FConfigCacheIni`. HP2 has all six headers in `Core/Inc/`. ACTIONS:
- 4.2.1 `FOutputDeviceFile` is the **GLog→console** sink on web only if it mirrors to stdout — HP2's copy does **not** (see §5.1 row C1). Without it the host's `Module.print` taps (`Bringing .* up for play`, notices, telemetry, crash reason) see nothing.
- 4.2.2 `FOutputDeviceFile` opens `appUserDir()+appPackage()+".log"` (HP2 `Core/Inc/FOutputDeviceFile.h:42-49`) → needs the POSIX `appUserDir()` (§2.3.4) and `appPackage()` = `"Game"` so the log is `/System/Game.log` (excluded from the manifest by `*.log`).

### 4.3 Render-device selection and subsystem overrides (template `:255-600`)

| # | Template | HP2 ACTION |
|---|---|---|
| 4.3.1 | `GRenderSwitches[]` `:271-285`: `XOPENGLRENDERER`, `UTGLRRENDERER`, `NOPENGLESRENDERER`, `SOFTWARERENDERER`, `NULLRENDERER` | **Remove `NOPENGLESRENDERER`** (HP2 builds no NOpenGLESDrv; the switch would only log "not available"). The `args` strings must match the frontend's `targets/hp2/target.json videoDrivers[].args` exactly (`-XOPENGLRENDERER`, `-UTGLRRENDERER`, `-SOFTWARERENDERER`, `-NULLRENDERER`). |
| 4.3.2 | `GRenderPreference[]` `:287-292` XOpenGL → UTGLR → NOpenGLES | drop NOpenGLES → `{XOpenGLDrv.XOpenGLRenderDevice, OpenGLDrv.OpenGLRenderDevice}` |
| 4.3.3 | `IsPackageClassAvailable()` `:301-321` — web: `StaticFindObject(UClass, ANY_PACKAGE, ShortName)` (no file load, no throw); native: `StaticLoadClass(... LOAD_NoWarn)` | keep. On web this relies on every statically-linked package registering its classes at startup (`UNREAL_STATIC` autoregistration). **Verify** HP2's 433 static-registration path (`UnStaticExports`, `GNativeLookupFuncs`) registers `UClass` objects before `InitEngine` — if registration is lazy (per package load), `StaticFindObject` returns NULL for every renderer and the preference falls through to "no known render device" (`:450`). |
| 4.3.4 | `SelectRenderDevice()` `:385-452` + `PersistRenderChoiceIfSwitched()` `:459-477` (writes `[Engine.Engine] ufrontUserRenderDevice` and `GConfig->Flush(0)`) | keep. HP2 retail `GameRenderDevice=D3DDrv.D3DRenderDevice` (`Config/System/Default.ini:22`) is not compiled → step 2 rejects it → preference order picks XOpenGL. Good. |
| 4.3.5 | Web overrides in `InitEngine()` `:483-527` (before `Engine->Init()`): `ViewportManager=NSDLDrv.NSDLClient`, `AudioDevice=NOpenALDrv.NOpenALAudioSubsystem`, `NetworkDevice=WebRTCDrv.WebRTCNetDriver`, `[NSDLDrv.NSDLClient] CaptureMouse=True`, `[Engine.GameInfo] bWorldLog=False`, `bLocalLog=False`, `[IpServer.UdpServerUplink] DoUplink=False` | keep the first four. `bWorldLog`/`bLocalLog` exist in HP2's `Engine/Classes/GameInfo.uc:110-111` → keep (harmless, avoids `appCreateProc`). **Drop** `IpServer.UdpServerUplink DoUplink` (no IpServer in HP2; writes a dead section). Guard the NOpenALDrv line on `HP2_HAS_NOPENALDRV` (web always has it). |
| 4.3.6 | Non-web `NormalizeSubsystem()` `:329-363, 528-551` (ViewportManager/AudioDevice/NetworkDevice fallback when the ini names an absent package: retail HP2 names `WinDrv.WindowsClient`, `ALAudio.ALAudioSubsystem`, `IpDrv.TcpNetDriver`) | keep for native/Windows (HP2's retail ini triggers exactly this). |
| 4.3.7 | `ServerActors.Empty()` `:571-591` (after `ConstructObject`, before `Init()`) | keep (generic safety): HP2's retail `[Engine.GameEngine]` still lists the six UT99 `ServerActors` (`Default.ini:69-74`: `IpDrv.UdpBeacon`, `IpServer.*`, `UWeb.WebServer`) — none exist in HP2. HP2 `UGameEngine` has `ServerActors` (`Engine/Inc/UnGame.h:27`). Also omit them from the web Default.ini (§9). |
| 4.3.8 | `GWebRTCEngine = Engine;` `:593-596` | keep |
| 4.3.9 | `FirstRun` `:555-559` writes `[FirstRun] FirstRun=ENGINE_VERSION` | keep; note HP2's retail `[FirstRun]` also has `Reconfig=1 ForceSoftware=0` read by the Win32 launcher's hardware detection (`Engine/Inc/UnEngineWin.h`) — irrelevant on web. |

### 4.4 Main loop (template `:630-731`)

| # | Template | HP2 ACTION |
|---|---|---|
| 4.4.1 | `DOUBLE OldTime = appSeconds();` … `FLOAT DeltaTime = NewTime - OldTime; Engine->Tick( DeltaTime );` | **Compile blocker on HP2**: `appSeconds()` is `FTime`. Rewrite with `FTime OldTime = appSeconds(); FTime NewTime = appSeconds(); FLOAT DeltaTime = NewTime - OldTime;` (`FTime::operator-` returns float) and `Engine->CurrentTickRate = TickCount / (OldTime - SecondStartTime)`; compare `OldTime > SecondStartTime + 1` via `(OldTime - SecondStartTime) > 1.f`. HP2's own Win32 loop is in `Engine/Inc/UnEngineWin.h:~1900-1915` (`MainLoop`) — mirror its FTime usage. |
| 4.4.2 | `GWindowManager->Tick( DeltaTime )` | HP2 `Core/Inc/Core.h:433` declares it; fine. |
| 4.4.3 | frame-rate cap `:667-699`: `[Engine.Engine] FrameRateCap` (default 120; `<=0` uncapped), read once; **no command-line switch** (2.61: the launcher sets it through `-ini:System:[Engine.Engine]:FrameRateCap=<n>`) | keep; add `FrameRateCap=` to HP2's web Default.ini (§9). Why it matters for HP2 too: `ULevel::Tick` floors `DeltaSeconds` (template note `:674-678`) — **verify** HP2 `Engine/Src/UnLevTic.cpp` has the same floor; if so an uncapped web loop runs the game fast. |
| 4.4.4 | web yield `:700-722`: `SleepMs = (Budget - elapsed)*1000; if(SleepMs>1) emscripten_sleep(SleepMs); else ufront_yield_frame();` then `UfrontPaceRecord(...)` | keep verbatim (the yield is the present point). **Never** switch to `emscripten_set_main_loop` (its `simulate_infinite_loop` unwind collides with `main()`'s C++ try/catch — template `:705-706`). |
| 4.4.5 | native branch `appSleep( Max(0.f,Delta) )` | keep |

### 4.5 `main()` (template `:805-938`)

| # | Template | HP2 ACTION |
|---|---|---|
| 4.5.1 | `appStrcpy( GModule, argv[0] ); appStrcpy( THIS_PACKAGE, appPackage() );` and the global `extern "C" {TCHAR THIS_PACKAGE[64]=TEXT("Launch");}` (`:152`) | **Link/runtime blocker on HP2:** HP2's Core declares `extern "C" DLL_EXPORT TCHAR GPackage[];` (`Core/Inc/Core.h:473`) and uses it as the **default package of every `Localize*()`** (`Core/Inc/UnFile.h:206-214`); the template launcher defines `THIS_PACKAGE` instead → `GPackage` undefined at link. Define `extern "C" {TCHAR GPackage[64]=TEXT("Launch");}` as HP2's Win32 launcher does (`Source/Launch/Src/Launch.cpp:18`). And do **not** fill it from `appPackage()` on web: HP2's Unix `appPackage()` returns `GModule` verbatim (`Core/Src/UnUnix.cpp:627-632`), and on web `argv[0]` is `./this.program`, so `LocalizeError(...)` would look in `./this.program.int` and the log would be `./this.program.log`. Set `GPackage`/`GModule` to `"Game"` explicitly (retail `System/Game.int` exists), or make `appPackage()` strip dir+extension like the template's. |
| 4.5.2 | `chdir( "/System" );` `:876-882` **before** `appInit` (so `appBaseDir()`=getcwd=`/System/`) | keep. HP2's `appBaseDir()` already uses `getcwd` (`Core/Src/UnUnix.cpp:589-601`), so the ordering is all that matters. The host also mkdirs+chdirs `/System` in `onRuntimeInitialized` (`ufrontend/packages/frontend/src/shell/boot.ts:411-418` comment). |
| 4.5.3 | `appInit( TEXT("UnrealTournament"), …, FConfigCacheIni::Factory, 1 )` `:883` | **→ `TEXT("Game")`** (derived ini `Game.ini`, log `Game.log`). HP2's `appInit` signature matches (`Core/Src/UnMisc.cpp:1845`). |
| 4.5.4 | `GIsClient = !ParseParam("SERVER")`, `GLazyLoad = !GIsClient \|\| ParseParam("LAZY")` | keep |
| 4.5.5 | `-LOG` → `Warn.AuxOut = GLog; GLog = &Warn;` | keep |
| 4.5.6 | `EXEC=` file, `MainLoop`, `CleanUpOnExit` (deletes `Running.ini`, `appPreExit`, `appExit`), `catch(...) HandleError` | keep. On web `catch(...)` is dead (catching off for engine objects) — a thrown error reaches Emscripten's abort; that is by design. `HandleError` calls `SDL_ShowSimpleMessageBox` (harmless no-op on web). |
| 4.5.7 | HP2 extras to carry over from its own Win32 launcher (`Source/Launch/Src/Launch.cpp`) and `UnEngineWin.h`: splash (`EASplashWaitTime`), `Running.ini`/`Detected.ini`, hardware auto-detect (`[FirstRun] Reconfig`) | **do not port to web** (Windows-only UX). `-LOAD=<slot>` / `SAVESLOT=` are engine-side (`UnGame.cpp:246-256`, `UnMisc.cpp:2019-2026`) and work from the web command line as-is (subject to §5.3 path fixes). |

### 4.6 What the host passes on the command line (so the launcher/engine must accept it)

From `ufrontend/packages/frontend/src/shell/boot.ts:28-39` `buildEngineArguments`: `[<configResult.cmdLine>, 'CPUSPEED=3000', ...engineOpts]`,
plus `-benchmark SECONDS=N` under `?benchmark=`. `engineOpts` carry the renderer switch (`-XOPENGLRENDERER` …) and every setting as
`-ini:<File>:[<Section>]:<Key>=<Value>` (player name, FrameRateCap, HRTF, …). `-nohomedir` is accepted as a no-op (2.37).

HP2 ACTIONS (both are Core/Engine ports, listed here because the launcher contract depends on them):
- 4.6.1 Port `appIsGlobalOptionToken()` (template `Core/Src/UnMisc.cpp:1080-1094`, decl `Core/Inc/UnFile.h:361`) and use it in HP2's
  `UGameEngine::Init` first-token test (`Engine/Src/UnGame.cpp:180-184`: `!ParseToken(...) || (SERVER…) || Parm[0]=='-'`). Without it,
  a launch with no map (`cmdLine` empty) makes `CPUSPEED=3000` the startup URL.
- 4.6.2 Port `appApplyIniOverrides()` (template `UnMisc.cpp:1119-1150`, decl `UnFile.h:369`) and call it in HP2 `appInit` right after
  `GConfig->Init( GIni, GUserIni, RequireConfig )` (HP2 `UnMisc.cpp:1952`) — **before** the "gk 10/15/01" Language block and
  `UObject::StaticInit`. HP2's `FConfigCacheIni::Find` already translates `User`/`System` to the real files
  (`Core/Inc/FConfigCacheIni.h:120-123`), so `iniToken` `System` → `Game.ini`, `User` → `User.ini`.
- 4.6.3 `-benchmark` (HP2 `UEngine`?) — verify HP2 parses `-benchmark`/`SECONDS=`; if not, the host's `?benchmark=` is inert (not a blocker).

## 5. Core/Engine patch sites (`__EMSCRIPTEN__` and "exceptions off → return NULL")
_pending_

## 6. NSDLDrv web paths
_pending_

## 7. Audio (NOpenALDrv / Module.alsoftConfig), WebRTCDrv, SoftDrv, XOpenGLDrv, OpenGLDrv web notes
_pending_

## 8. vendor/glue ABI: exports/imports the engine must provide
_pending_

## 9. Web ini set to author for HP2 (`Config/System/web/`)
_pending_

## 10. Artifact contract (SDLLaunch.{js,wasm,data}, meta.json, targetId `hp2`)
_pending_

## 11. Wasm limits, required exports, pthread / proxy-to-pthread / OffscreenCanvas, frontend capability probes
_pending_

## 12. Smoke test (`tools/web-smoke.mjs`) and how to adapt it
_pending_

## 13. Web-era commits (2.7b..2.79) and anything undocumented
_pending_

## 14. UT99-specific things that must NOT be copied
_pending_

## 15. Numbered replication checklist (short form, build order)
_pending_

## 16. Open questions / verify list
_pending_

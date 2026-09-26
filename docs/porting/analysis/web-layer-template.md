# web-layer-template — the Emscripten/web layer of engine-ut99, end to end, as an HP2 replication checklist

**Key:** `web-layer-template` · **Run:** 2026-09-26 (fresh re-run; supersedes the 2026-09-25 draft) ·
**Template (READ ONLY):** `/home/ion/engines/engine-ut99` · **Port target:** `/home/ion/hp2-ufrontend/HarryPotter2` ·
**Frontend clone:** `/home/ion/hp2-ufrontend/ufrontend` (branch `hp2`) · **Ancestor:** `/home/ion/epic-sources/ut99-v400`

_Status: in progress — sections are appended as they are researched._

## 0. State of the HP2 repo today (web layer only)

Snapshot taken 2026-09-26 (HP2 `main` = `306b4f0`, 4 commits). What of the web layer exists and in what state:

| Piece | HP2 path | State vs template (`diff -rq --strip-trailing-cr`) | Consequence |
|---|---|---|---|
| Top-level CMake `if(EMSCRIPTEN)` + presets | `Source/CMakeLists.txt`, `Source/CMakePresets.json` | hand-adapted (renames `UT_WEB_*`→`HP2_WEB_*`, no Dreamcast/NOpenGLES) | §1 — mostly done |
| Launcher CMake (web link) | `Source/Game/CMakeLists.txt` | template minus Dreamcast, `--target=hp2` | §2 — done except the inis it preloads |
| `cmake/*.cmake`, `emit-artifact-meta.mjs`, `offscreen-depth24.sh` | `Source/cmake/`, `Source/Game/cmake/`, `Source/Game/` | **byte-identical** | §3 — emitter strings still say UT99 |
| Launcher source | `Source/Game/Src/Launch.cpp` | **byte-identical** (md5 `9de4607c…`) | §4 — not adapted; several compile/link blockers on 433 |
| NSDLDrv | `Source/NSDLDrv/{Inc,Src}` | **0 diffs** | §6 — must be adapted to HP2's `UViewport`/`UClient` |
| NOpenALDrv | `Source/NOpenALDrv/` | **0 diffs** (UT99 v400 `UAudioSubsystem` face, libxmp/.umx music) | §7.1 — will not compile against HP2 `UnAudio.h`; rewrite from HP2 `ALAudio` using it as the pattern |
| WebRTCDrv | `Source/WebRTCDrv/` | **0 diffs** | §7.2 — adapt to HP2 `UNetDriver`/`UNetConnection` (433 face) |
| IpDrv | `Source/IpDrv/` | only `CMakeLists.txt` differs | needed as base of WebRTCDrv auto-switch |
| XOpenGLDrv / OpenGLDrv wrappers | `Source/XOpenGLDrv/`, `Source/OpenGLDrv/` | 0 / 1 (`CMakeLists.txt`) diffs; the renderer code is fetched from the forks | §7.4/§7.5 |
| SoftDrv | `Source/SoftDrv/` | **HP2's own 433 SoftDrv**, 6 source files differ, no `WEB-NOTES.md` | §7.3 — port the template's web fixes by analogy |
| `vendor/glue/{ue_abi.h,webrtc-shim.js,wasmfs-shim.js}` | `vendor/glue/` | **identical** to template (ABI 1) | §8 — keep verbatim |
| `tools/web-smoke.mjs` | `tools/web-smoke.mjs` | **identical** (still UT99 paths/asserts) | §12 — adapt |
| Web inis | `Config/System/web/` | **only `README-ut99.md`** — no `Default.ini`/`DefUser.ini` | §9 — BLOCKER for first web link (LINK_DEPENDS on missing files) and boot (`MisingIni`) |
| Renderer forks | expected at `/home/ion/hp2-ufrontend/{XOpenGLDrv,utglr-copy,gl4es}` | **still inside `/home/ion/hp2-ufrontend/ufrontend/`** as untracked `??` dirs (re-checked 2026-09-26: `git -C ufrontend status --short` → `?? XOpenGLDrv/ ?? gl4es/ ?? utglr-copy/`; tips `87f6e89` / `e60807b` / `98441ed5`) | §1.4.2 — `mv` them one level up before the first configure; PLAN.md §0 describes them as siblings, which is not true on disk |
| Build trees | `build-native/` exists (native configure was run); no `build-wasm*` | no web configure has been attempted yet | — |

Bottom line: the web **build scaffolding** (CMake, presets, emitter, depth patch, glue shims) is in place and
faithful; **every web source path** (Launch.cpp, NSDLDrv, NOpenALDrv, WebRTCDrv, Core/Engine `__EMSCRIPTEN__` sites)
is still the template's code, unadapted to 433, and the web ini set does not exist.

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

**Why this section exists.** The web build compiles every engine object with Emscripten's default
`-fignore-exceptions` (§1.2.13): `throw` compiles to `__cxa_throw` → `abort()`, every `catch` block is dead code,
and every UE1 `guard/unguard` is inert. So *every* code path where the engine relies on "a loader throws, a caller
catches" must be rewritten to "resolve to NULL, caller checks NULL". The template did that site by site
("2.6f/2.6g/2.12/2.79o"), always behind `#ifdef __EMSCRIPTEN__` so native keeps the retail throw semantics.
HP2 today has **zero** `__EMSCRIPTEN__` sites in `Source/{Core,Engine,Render,Fire,Editor}`
(`grep -rn __EMSCRIPTEN__` → nothing), so all of the below are open actions.

Guard pattern (use exactly this shape everywhere so a grep finds them):
```cpp
#ifdef __EMSCRIPTEN__
	return NULL; // ufront (2.6f): <why> (catching off — a throw aborts across Asyncify frames)
#else
	appThrowf( LocalizeError("..."), ... );
#endif
```
`engine-unreal1` carries the same set (`Core/Src/UnObj.cpp:1588,1634,1644,1655,1821`, `Engine/Src/UnGame.cpp:163,367,547`),
confirming these are the "mirrors engine-unreal1" sites and there are no extra unreal1-only loader sites to pick up.

### 5.1 Core: diagnostics and logging (make every abort *named*)

| # | Template site | Code (template) | HP2 site | ACTION |
|---|---|---|---|---|
| C1 | `Core/Inc/FOutputDeviceFile.h:84-100` (ufront 2.8, **not** `#ifdef`'d) | after the file write: `if( Event != NAME_Title ) { fputs(FName::SafeString(Event),stdout); fputs(": ",stdout); fputs(Data,stdout); fputc('\n',stdout); }` | `Core/Inc/FOutputDeviceFile.h:69-84` (write block ends `:80`, `GLogHook` at `:82-83`) — **absent** | Add the stdout mirror inside the `!FName::SafeSuppressed(Event)` block, after the `LogAr` write. This *is* "GLog to console": the host's `Module.print` → `console.log` and all the frontend/smoke log taps (`Bringing … up for play`, `appError called`, `[appThrowf]`) depend on it. HP2's block is gated by `if( !GIsCriticalError \|\| Entry )` (`:33`) exactly like the template's — keep. |
| C2 | `Core/Src/UnMisc.cpp:688-697` (`appThrowf`, 2.7b/2.7c) | `#ifdef __EMSCRIPTEN__ printf("[appThrowf] %s\n", TempStr); fflush(stdout); #endif` before `throw( TempStr );` | `Core/Src/UnMisc.cpp:674-679` — absent | Add verbatim. Turns every remaining uncatchable throw into a named `[appThrowf] <reason>` console line — the tool that finds the next site to patch. |
| C3 | `Core/Src/UnLinker.h:335` and `:1062-1063` (2.35, not web-guarded) | `appThrowf( TEXT("%s: %s"), LocalizeError("OpenFailed"), InFilename );` | `Core/Src/UnLinker.cpp:168` (ULinkerLoad ctor) and `:892` (ULinkerSave ctor) — bare `LocalizeError("OpenFailed")` | Apply both (name the file). Platform-neutral. |
| C4 | `Core/Src/UnUnix.cpp:23-25` | `#ifdef __EMSCRIPTEN__ #include <emscripten.h> #endif` | HP2 `Core/Src/UnUnix.cpp` is the **Loki-era** Unix port (no SDL, `#include <dlfcn.h> <netdb.h>`), not the template's SDL `UnUnix.cpp` | Carry the include with the `appRequestExit` hunk (C5). HP2's UnUnix is one of the 41 three-way-merge conflicts (PLAN.md §3); the web hunks below must survive that merge. |
| C5 | `Core/Src/UnUnix.cpp:205-238` (2.12, 2.13.2) | in `appRequestExit` non-force branch: `#ifdef __EMSCRIPTEN_PTHREADS__ MAIN_THREAD_EM_ASM({ #else EM_ASM({ #endif if(typeof window!=='undefined'){ console.log('[SHELL] Engine exit requested, returning to launcher'); setTimeout(function(){ window.location.href = window.location.pathname; },0);} }); ` | `Core/Src/UnUnix.cpp:167-181` — absent | Add verbatim. `MAIN_THREAD_EM_ASM` is mandatory under `PROXY_TO_PTHREAD` (a worker has no `window`; plain `EM_ASM` silently skips and leaves a dead canvas). The frontend's E2E waits on this navigation. **HP2 check:** which HP2 UI paths call `appRequestExit(0)` vs `Force`? HP2's "Quit" in the front-end menu (`HGame`/`UWindow` console `exit`) must go through the non-force path; `Core/Src/UnUnix.cpp:120,888` already call `appRequestExit(0)`. |
| C6 | `Core/Src/UnUnix.cpp:673-677` (2.7b) | `appBaseDir()`: `#if defined(__EMSCRIPTEN__) if(getcwd(BaseDir,sizeof(BaseDir))) appStrcat(BaseDir,"/");` (SDL_GetBasePath returns "/" under Emscripten) | `Core/Src/UnUnix.cpp:589-601` already uses `getcwd` unconditionally | Nothing to add *if* HP2 keeps its getcwd. If the merge takes the template's SDL `appBaseDir`, keep the `__EMSCRIPTEN__` getcwd branch. Either way `main()` must `chdir("/System")` before `appInit` (§4.5.2). |
| C7 | *(template has it, not web-guarded)* `appUserDir()` in `UnUnix.cpp` | returns `""` (no home dir; `-nohomedir` accepted as no-op) | **missing on POSIX** (only `UnVcWin32.cpp:1048`) | Define `appUserDir()` in HP2's `UnUnix.cpp`; web must return `""` (§2.3.4). Link blocker, not just web. |
| C8 | `Core/Src/UnMisc.cpp:1080-1094`, `:1119-1150` (not web-guarded, 2.37c/`2c25d55`) | `appIsGlobalOptionToken`, `appApplyIniOverrides` | absent | Port both (§4.6.1/4.6.2) — the web host passes `CPUSPEED=3000` first and every setting as `-ini:…`. |
| C9 | `Core/Src/UnMisc.cpp` `appFindPackageFile` per-path case-insensitive resolve (`7164de7`) | each `Paths=` entry is tried exactly **then** case-insensitively before the next | HP2 `appFindPackageFile` (`Core/Src/UnMisc.cpp`, search `appFindPackageFile`) | Port. WasmFS/FetchFS is case-sensitive and HP2's retail tree mixes case (`Maps/`, `Textures/`, `Sounds/`; check `ls Assets/`). A later path's exact hit beating an earlier case-variant is a wrong-package load → `[appThrowf] Can't find … in file` → abort. |

### 5.2 Core: the loader — "exceptions off → return NULL" sites

| # | Function | Template (file:line) | HP2 (file:line) | Patch (web branch) |
|---|---|---|---|---|
| L1 | `UObject::SafeLoadError` | `Core/Src/UnObj.cpp:1991-2001` | `Core/Src/UnObj.cpp:2155` | `#ifndef __EMSCRIPTEN__ if( LoadFlags & LOAD_Throw ) appThrowf(TEXT("%s"),Error); #else (void)Error; #endif`. **The single most important site**: every loader catch funnels here with `LOAD_Throw`. HP2 extra: `SafeLoadError` starts with Legend's `if( GIsStrict ) LoadFlags = LOAD_NoFail;` (`:2143-2151`) — on web `-STRICT` would convert every soft failure into a fatal `appErrorf`; never pass `STRICT` from the host. |
| L2 | `UObject::GetPackageLinker` — `!InOuter` | `UnObj.cpp:2043-2047` | `UnObj.cpp:2196-2197` | `return NULL;` instead of `appThrowf(PackageResolveFailed)` |
| L3 | `GetPackageLinker` — package not found by name | `UnObj.cpp:2053-2057` | `UnObj.cpp:2203` | `return NULL;` instead of `appThrowf(PackageNotFound)` |
| L4 | `GetPackageLinker` — file not found by filename | `UnObj.cpp:2064-2068` | `UnObj.cpp:2209-2210` | `return NULL;` instead of `appThrowf(FileNotFound)` |
| L5 | `GetPackageLinker` — GUID mismatch (`PackageVersion`, 2.79o) | `UnObj.cpp:2116-2129`: `FString Why = FString::Printf( LocalizeError("PackageVersion"), InOuter->GetName() ); SafeLoadError( LoadFlags, *Why, LocalizeError("FailedLoad"), InOuter->GetName(), *Why ); return NULL;` | `UnObj.cpp:2253-2254` | Port — only reachable in a net join (`CompatibleGuid` non-NULL). One `Localize` per statement (shared static buffer). |
| — | `GetPackageLinker` — `FilenameToPackage` | `UnObj.cpp:2092` (left as throw) | `UnObj.cpp:2234` | leave (unreachable: `CreatePackage` never returns NULL) |
| — | `GetPackageLinker` — Sandbox | template `:2108` throw (left) | HP2 `:2262-2266` already **debugf + `Result=NULL`** outside the try (436-era) | nothing — HP2 is already soft here |
| L6 | `UObject::StaticLoadObject` — ObjectNotFound | `UnObj.cpp:2245-2257`: `{ EndLoad(); return NULL; }` | `UnObj.cpp:2376-2377` | Port. Note the web branch skips `GObjPackageRemap` retry (HP2 retail inis have no `[PackageRemap]`? — verify `grep -i remap Config/System/Default.ini`). |
| L7 | `ULinkerLoad::VerifyImport` — nested import, parent linker NULL | `Core/Src/UnLinker.h:552-564`: `if(!Import.SourceLinker){ if(Cast<UPackage>(LinkerRoot)) Cast<UPackage>(LinkerRoot)->PackageFlags |= PKG_BrokenLinks; return; } #else check(Import.SourceLinker); #endif` | `Core/Src/UnLinker.cpp:400` (`check(Import.SourceLinker);`) | Port verbatim |
| L8 | `ULinkerLoad::VerifyImport` — top-level import, linker NULL | `UnLinker.h:573-585` (before the `ExportHash[iHash]` walk) | insert after `Core/Src/UnLinker.cpp:407` (end of the `else` branch), before `:409` `UBOOL SafeReplace = 0;` | Port verbatim. HP2's `PackageIndex==0` branch still has `try{ GetPackageLinker(…LOAD_Throw…) } catch → appThrowf(Error)` (`:379-393`); with L1 in place `GetPackageLinker` returns NULL instead of throwing, so the catch is never needed — L8 is what makes it safe. |
| — | `VerifyImport` `FailedImportPrivate` / `FailedImport` | template leaves as throws | HP2 `UnLinker.cpp:454`, `:527` (both preceded by `LOAD_Forgiving` soft paths) | leave; if a web boot dies with `[appThrowf] Failed import …`, the fix is content (missing package) or `LOAD_Forgiving`, not a guard. |
| — | `ULinkerLoad::Create` `FailedCreate` (only `Checked`) | leave | HP2 `UnLinker.cpp:583` | leave |
| — | `ULinkerLoad` ctor `OpenFailed`/`LinkerExists`/`BinaryFormat`/`OldVersion` | leave (fatal on web by design; C3 names the file) | HP2 `UnLinker.cpp:168,176,205,213` | leave. **HP2 note:** `:212-213` is a `GWarn->YesNof("OldVersion")` prompt — `FFeedbackContextAnsi::YesNof` on web returns …? verify it returns 1 (non-interactive) or this aborts on any pre-`PACKAGE_MIN_VERSION` file. |
| L9 | `UObject::StaticLoadClass` `LoadClassMismatch` | template **leaves** `:2294` | HP2 `UnObj.cpp:2413` | leave (programming error, not content) |
| L10 | `UObject::LoadPackage` — **template gap** | `UnObj.cpp:2319-2324`: `ULinkerLoad* Linker = GetPackageLinker(…); if(!(LoadFlags & LOAD_Verify)) Linker->LoadAllObjects(); Result = Linker->LinkerRoot;` — with L1-L4, `Linker` can be NULL on web and this NULL-derefs | HP2 `UnObj.cpp:2438-2441`, identical | **Add (HP2 improvement over the template):** `#ifdef __EMSCRIPTEN__ if( !Linker ) { EndLoad(); return NULL; } #endif` right after the `GetPackageLinker` call. HP2 calls `LoadPackage` for things the template never does (verify: `grep -rn 'LoadPackage(' Source/Engine Source/HGame Source/*/Src` — HP2's save/load and `HPDialog`/`HGame` natives may). A NULL-deref shows as `memory access out of bounds`, which is harder to diagnose than the named `appErrorf`. |

### 5.3 Engine: `UGameEngine` (Init / Browse / LoadMap / SetProgress)

| # | Function | Template (file:line) | HP2 (file:line) | ACTION |
|---|---|---|---|---|
| E1 | `UGameEngine::Init` — startup map failure → fall back to Entry (2.6g) | `Engine/Src/UnGame.cpp:202-215`: `#ifdef __EMSCRIPTEN__ { debugf(NAME_Warning, TEXT("Startup map '%s' failed to load (%s) — falling back to the menu"), Parm, *Error); if(!GLevel) GLevel = GEntry; } #else appErrorf(FailedBrowse…) #endif` | `Engine/Src/UnGame.cpp:212-214` | Port. **HP2 semantic check:** HP2's `FURL::DefaultLocalMap` (retail `[URL] LocalMap=`, check `Config/System/Default.ini`) is HP2's start map (likely `Startup.unr`, which plays the intro/front-end). Falling back to `GEntry` (`Entry.unr`) leaves the player in HP2's Entry map — verify Entry has a usable HP2 menu (HP2 front-end is a UWindow `HPConsole`/`FEBook`? — owned by the boot report). If not, fall back to `DefaultLocalMap` instead. |
| E2 | `UGameEngine::Init` — first-token test (`1d3dfa5`) | `UnGame.cpp:~180` uses `appIsGlobalOptionToken(Parm)` | `UnGame.cpp:181-185` `Parm[0]=='-'` only | Port (C8). |
| E3 | `UGameEngine::SetProgress` — NULL Actor (2.12) | `UnGame.cpp:288-293`: `if( Client && Client->Viewports.Num() && Client->Viewports(0)->Actor )` | `UnGame.cpp:303-305` (`if( Client && Client->Viewports.Num() )` then derefs `Actor`) | Port (not web-guarded in template). Reached from every web failure path in E5/E6 before the player pawn exists. |
| E4 | `UGameEngine::Browse` — `<host>?connect` promotion (2.7d) | `UnGame.cpp:509-520`: `if( URL.HasOption(TEXT("connect")) && URL.Host.Len()<=0 && URL.Map.Len()>0 ) { URL.Host = URL.Map; debugf(TEXT("[ufront] connect URL: promoted map '%s' to host"), *URL.Host); }` | insert after `UnGame.cpp:651` (`debugf( TEXT("Browse: %s"), … )`) | Port **only if** HP2 ships multiplayer on web (WebRTC P2P). HP2 retail is single-player; the frontend's `targets/hp2` descriptor decides (`network` section). If `network` is disabled for hp2, skip E4 and E5 and note it. |
| E5 | `UGameEngine::LoadMap` — net join: NULL linker in the PackageMap (2.79o) | `UnGame.cpp:734-750` (loop over `PackageMap->List`, `appFindPackageFile` to pick `PackageVersion` vs `PackageNotFound`, `EndLoad(); SetProgress(...); return NULL;`) | after `UnGame.cpp:903` (the GetPackageLinker loop), before `:905-906` `VerifyLinker` | Port with E4 (same condition). HP2 detail: HP2's loop declares `INT i` inside the first `for` and reuses `i` in the second (MSVC6 scoping, `:896,905`) — gcc rejects this; the template hoisted `INT i;` (`:731`) — do the same. |
| E6 | `UGameEngine::LoadMap` — verify-load returned NULL (2.12) | `UnGame.cpp:757-772`: `ULevel* Verified = LoadObject<ULevel>(…LOAD_Verify|LOAD_Throw|LOAD_NoWarn…); EndLoad(); #ifdef __EMSCRIPTEN__ if(!Verified){ Error = FString(TEXT("Could not load map ")) + URL.Map; SetProgress(LocalizeError(TEXT("UrlFailed"),TEXT("Core")), *URL.Map, 6.0); return NULL; } #else (void)Verified; #endif` | `UnGame.cpp:910-911` (result discarded) | Port: capture the result into `Verified` and add the bail **before** `DissociateViewports`/`ExitLevel` (HP2's order: check that HP2's `LoadMap` tears down the old level *after* `:911`, as v400 does). The `SetProgress` 3rd arg is `6.f` in HP2 style. |
| E7 | `UGameEngine::LoadMap` — real load returned NULL (2.6g) | `UnGame.cpp:871-881`: `#ifdef __EMSCRIPTEN__ if(!GLevel){ Error = …; SetProgress(…); return NULL; } #endif` | after `UnGame.cpp:1066-1067` (`GLevel = LoadObject<ULevel>(… LOAD_NoFail …); unguard;`) and before `:1092 check(GLevel)` | Port verbatim. |
| E8 | `UGameEngine::Init` — Entry load failure | template leaves `appErrorf(FailedBrowse, "Entry")` | HP2 `UnGame.cpp:169-170` | leave (no Entry = broken install; a named fatal is right). |
| E9 | `UGameEngine::SaveGame` / save dir creation (`1546db7`) | `MakeDirectory( *GSys->SavePath, 1 )` (Tree=1) | HP2 `UnGame.cpp:250-251,445-446,470-471,486-487`: `MakeDirectory(SavePath,0)` and **`SaveSlotPath`** (HP2-only) with Tree=0 | Pass `1` at all 8 calls. HP2 has a second-level `SaveSlotPath` (`UnMisc.cpp:2019-2026`, `SAVESLOT=`), i.e. exactly the two-level case that aborted Spatial Fear on the web: `ULinkerSave` → `OpenFailed` → throw → abort mid-game. |
| E10 | savegame path through FURL on Unix (`5674fc7`) | Browse `?load` and hub-pop branches set `FURL.Map` directly from the `PATH_SEPARATOR` path | HP2 `Browse` — HP2 uses `SAVESLOT`/`-LOAD=` (`UnGame.cpp:246-256`) | Verify HP2's load path builds `"../Save/…usa?load"` via `FURL( …, TEXT("..") PATH_SEPARATOR …)`; if so port the fix (on Unix/web `/` is parsed as URL host syntax → loads the default map). |
| E11 | `Received '<pkg>'` debugf (`3ff2ed5`, comment `7c31a53`) | `Engine/Src/UnChan.cpp:1322-1326` (UT99 downloads in `UFileChannel`) | HP2 downloads live in **`Engine/Src/UnDownload.cpp:176`** (`appSprintf( Msg, TEXT("Received '%s'"), Info->Parent->GetName() );`, 436-era `UDownload`) | Only with multiplayer: add `debugf( TEXT("%s"), Msg );` next to it. Not needed for the host (it flushes on `Bringing … up for play`, which HP2 already logs at `UnGame.cpp:1272`). |

### 5.4 Engine: `UnLevel.cpp`, `UnLevTic.cpp`, other

| # | Site | Template | HP2 | ACTION |
|---|---|---|---|---|
| X1 | `Engine/Src/UnLevel.cpp` | **no web patches** (`diff` vs epic = only gcc scoping/`Min<INT>`/`!(a==b)` fixes, lines 310-692) | HP2 `UnLevel.cpp` | nothing web-specific; the gcc fixes are the compile report's. |
| X2 | `ULevel::Tick` tick floor 0.005 → 0.001 (`e2e724b`) | `Engine/Src/UnLevTic.cpp` | HP2 `Engine/Src/UnLevTic.cpp` (search `DeltaSeconds` floor) | Port; together with the launcher's `FrameRateCap` (§4.4.3) it stops the world running fps/200× fast on web. |
| X3 | GC reachability walk iterative (`ec74328`) | `Core/Src/UnObj.cpp` `FArchiveTagUsed` drain | HP2 `UnObj.cpp` `FArchiveTagUsed` | Port. On web the JS/wasm call stack is the limit (`RangeError: Maximum call stack size exceeded` at 5,547 wasm frames on a -O0 build). HP2 levels with many actors + HP2's larger class graph (HGame) make this *more* likely. |
| X4 | `UT_REPLACEMENT_ALLOC` `always_inline` on global `operator new/delete` (`ec74328`) | `Core/Inc/UnGnuG.h`/`Core.h` | HP2 `Core/Inc/Core.h` operator new/delete | Port. Otherwise the `-O0` `wasm-debug` build hands WasmFS's static initialiser the engine allocator before `appInit` → NULL. |
| X5 | `FString::Mid` DWORD sum (`1f744b8`) | `Core/Inc/UnTemplate.h` | HP2 `Core/Inc/UnTemplate.h` `Mid` | Port (UB under clang -O2; general). |
| X6 | `DECLARE_CLASS` no value-init (`a680288`) + layout guard (`bb90b47`) | `Core/Inc/UnObjBas.h` | HP2 433 `DECLARE_CLASS` | Owned by the core report; listed because a wiped UObject header crashes the web build at the first GC. |

### 5.5 Core: POSIX file manager (web compile + case/separator traps)

| # | Site | Template | HP2 | ACTION |
|---|---|---|---|---|
| F1 | `FFileManagerLinux::MakeDirectory` mode | `Core/Inc/FFileManagerLinux.h:287-293`: `mkdir(Path, S_IRUSR \| S_IWUSR \| S_IXUSR)` (+ `PLATFORM_WIN32` no-mode branch, 2.33) | `Core/Inc/FFileManagerLinux.h:263`: `mkdir(Path, __S_IREAD && __S_IWRITE && __S_IEXEC)` — **logical AND = mode 1**, and `__S_IREAD` is a glibc-internal macro | Take the template's line. The `&&` bug makes native save dirs mode `0001` (unwritable: saving fails); on Emscripten's musl `__S_IREAD`/`__S_IWRITE` are not public API — expect a web compile error at `:234,254,263`. Replace with `S_IRUSR\|S_IWUSR` in the two `chmod` calls too (template `:234-238,258-262`, which also adds `#include <sys/stat.h>`, `<cerrno>`). |
| F2 | `FFileManagerLinux::CreateFileReader` | `fopen(Filename,"rb")` **verbatim** — no `\`→`/` conversion, no case folding (only `FindFiles` converts `\`, `:319-323`) | same | Any HP2 path built with `"..\\"` literals fails on WasmFS and Linux. Known HP2 instance: `ALAudio/Src/ALAudioSubsystem.cpp:1136` `TEXT("..\\music\\")` (and the lowercase `music` vs `Assets/Music/`). Grep all HP2 packages for `TEXT("..\\\\` and fix each to `PATH_SEPARATOR` or `/` with the on-disk case. `GSys->SavePath` default `"..\\Save"` (§2.3.4) is the other one. |
| F3 | `FFileManagerLinux::GetGlobalTime/SetGlobalTime` | stubs present (`:266-281`) | absent | Needed only if HP2's `FFileManager` base declares them pure (check `Core/Inc/UnFile.h`); the core merge decides. |

## 6. NSDLDrv web paths

HP2 `Source/NSDLDrv/` is byte-identical to the template (0 diffs). The **433 interface adaptation**
(`DECLARE_CLASS(…,NSDLDrv)` 4th arg, `FTime` for `LastUpdateTime`/`InputUpdateTime`, `bWindowsMouseAvailable = !Capture`
in `SetMouseCapture`) is owned by `template-drivers.md` §1-§2. This section lists the **web** sites that must survive
that adaptation unchanged. Every web branch is guarded `#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)`
(call it **G3**). `__EMSCRIPTEN_PTHREADS__` only exists when `-pthread` is passed at **compile** time, so HP2's global
`add_compile_options(-pthread)` (`Source/CMakeLists.txt:76`, §1.2.10) is what switches all of them on. If it is
missing, every row below silently compiles its *non-web* branch, and the first `SDL_GL_CreateContext` fails with
`InvalidStateError`.

### 6.1 Site table (template line numbers = HP2 line numbers today)

| # | Site (`Source/NSDLDrv/…`) | What it does (template's reason) | ACTION for HP2 |
|---|---|---|---|
| N1 | `Inc/NSDLDrv.h:75-81` (G3) `BYTE* SoftFB; INT SoftFBBytes;` | a worker has no usable SDL renderer (the GLES-backed `SDL_CreateRenderer` → proxied `eglCreateContext` deadlocks), so SoftDrv renders into this buffer | keep |
| N2 | `Src/NSDLViewport.cpp:4-9` (G3) `#include <emscripten/html5.h>`, `<emscripten/html5_webgl.h>` | | keep |
| N3 | `Src/NSDLViewport.cpp:14-38` (G3) `ufrontCanvasDevicePixelSize()` (2.71): `emscripten_get_element_css_size("#canvas")` × `emscripten_get_device_pixel_ratio()`, `lround`, returns 0 for a 0×0 box | the backing store must be the canvas's **device-pixel** size or the browser resamples it (glyphs measured 1.25× wide on a 125 % desktop) | keep verbatim. Consequence for HP2: viewport sizes of 2560-5120 px wide are normal on the web → §7.3 SoftDrv bound |
| N4 | `Src/NSDLViewport.cpp:206-209` (G3) ctor zeroes `SoftFB`/`SoftFBBytes` | | keep |
| N5 | `Src/NSDLViewport.cpp:317-322`, `:366-386`: `INT WebGLESMajor = 2;` then `if( DoOpenGL && Temp.InStr(TEXT("XOPENGL")) != INDEX_NONE ) WebGLESMajor = 3;` (2.9); `DoOpenGL` = the device name contains `OPENGL` (`:366,372`) | the render-device **class path string** picks WebGL2 vs WebGL1 vs no GL | keep. HP2 must keep the class paths `XOpenGLDrv.XOpenGLRenderDevice` (→ WebGL2) and `OpenGLDrv.OpenGLRenderDevice` (→ WebGL1); `SoftDrv.SoftwareRenderDevice` and `NullDrv.NullRenderDevice` contain no `OPENGL` → SoftFB path. Update the comment at `:319` to drop NOpenGLES. |
| N6 | `Src/NSDLViewport.cpp:421-428` (G3): do **not** add `SDL_WINDOW_OPENGL` | SDL's EGL path would be proxied to a main thread that no longer owns the canvas (Emscripten #24792) | keep |
| N7 | `Src/NSDLViewport.cpp:435-441` (`#ifdef __EMSCRIPTEN__`) `SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, WebGLESMajor)` + `SDL_GL_CONTEXT_PROFILE_ES` | harmless under G3 (no GL window), keeps the non-pthread build honest | keep |
| N8 | `Src/NSDLViewport.cpp:503-549` (G3) "hatch (c)": `emscripten_webgl_init_context_attributes(&Attrs); Attrs.majorVersion = WebGLESMajor-1; minorVersion=0; alpha=0; depth=1; stencil=1; antialias=0; explicitSwapControl=1; renderViaOffscreenBackBuffer=1; WebGLCtx = emscripten_webgl_create_context("#canvas",&Attrs); … emscripten_webgl_make_context_current(WebGLCtx); GLCtx = (SDL_GLContext)(intptr_t)WebGLCtx;` + `appErrorf` on failure | **the direct WebGL context on the worker** (on the canvas that `-sOFFSCREENCANVASES_TO_PTHREAD=#canvas` transferred). XOpenGL adopts it via `emscripten_webgl_get_current_context()`; gl4es/UTGLR use whatever context is current | keep verbatim. The `explicitSwapControl`+`renderViaOffscreenBackBuffer` pair needs `-sOFFSCREEN_FRAMEBUFFER` at link (§2.2.17) and gets the 24-bit depth from `offscreen-depth24.sh` (§3.4). The `debugf(NAME_Init, "Created WebGL %i.0 context on the worker (handle %i)")` line is a useful smoke assert (§12). |
| N9 | `Src/NSDLViewport.cpp:551-555` (G3) `emscripten_webgl_make_context_current` instead of `SDL_GL_MakeCurrent` | | keep |
| N10 | `Src/NSDLViewport.cpp:557-570` (G3) SoftFB path: `SDLTexFormat = SDL_PIXELFORMAT_ARGB8888; ColorBytes = 4; Caps = 0;` | no SDL renderer/texture on the worker | keep. HP2's SoftDrv must accept `ColorBytes=4` (retail HP2 SoftDrv supports 32-bit? check `SoftDrv.cpp` `SetRes` for `ColorBytes==4` — v400's does) |
| N11 | `Src/NSDLViewport.cpp:612-631` (2.12 + G3 2.71): adopt `SDL_GetWindowSize` then override with the device-pixel size, **unaligned** on web (`WantX = RealW`) vs `Align(RealW,4)` natively | dynamic resolution: the game resolution is the browser window | keep. HP2 has no "fixed 640×480 menu" assumption? — verify HP2's front-end (`HPConsole`/`FEBook` UWindow pages) scales with `Canvas.ClipX/ClipY`; if a page assumes 640×480 it will be tiny on a 4K canvas (AutoFOVUI-style HUD scaling is a later feature, not a boot blocker). |
| N12 | `Src/NSDLViewport.cpp:690-694` (G3) `emscripten_webgl_destroy_context` in `CloseWindow` | SDL never made the context | keep |
| N13 | `Src/NSDLViewport.cpp:729-760` (G3) `Lock`: (re)size `SoftFB` lazily **here**, `if( SoftFBBytes != Needed )` (2.19f) | the only point where `SizeX/SizeY` are final and the buffer is about to be written; sizing elsewhere was a per-frame heap overrun above ~4 MB | keep; read `SoftDrv/WEB-NOTES.md` §2 |
| N14 | `Src/NSDLViewport.cpp:885-889` (G3) `emscripten_webgl_commit_frame();` in `Unlock` | **the present** under `explicitSwapControl` (`SDL_GL_SwapWindow` would go through EGL emulation). A stub without `-sOFFSCREEN_FRAMEBUFFER` (the ~1 fps D3D11 Chromium bug, `2074fe0`) | keep |
| N15 | `Src/NSDLViewport.cpp:894-931` (G3) SoftFB present: `EM_ASM` → 2D context of `Module['canvas']`, `Module.__ufrontSoftImg` `ImageData`, BGRA→RGBA swizzle 32 bits at a time, `ctx.putImageData(...)` (`:929`); "one declaration per statement — EM_ASM is a macro and the preprocessor splits on top-level commas" | CPU present for SoftDrv/NullDrv | keep |
| N16 | `Src/NSDLViewport.cpp:967-968` (G3) `MakeCurrent` → `emscripten_webgl_make_context_current` | | keep |
| N17 | `Src/NSDLViewport.cpp:1114-1121` (`#ifndef __EMSCRIPTEN__` / `#else (void)OnlyFocus`) `SetMouseCapture` skips the `SDL_GetMouseFocus()` gate | SDL2's Emscripten port defers pointer lock to the next click; a stale focus on the transition frame would drop the lock | keep. Combine with template-drivers' `bWindowsMouseAvailable = !Capture;` addition (HP2 retail WinViewport semantics; HP2 menus read absolute `WindowsMouseX/Y`) |
| N18 | `Src/NSDLViewport.cpp:1276-1290` (G3) `WINDOWEVENT_RESIZED/SIZE_CHANGED`: re-apply the device-pixel size whenever SDL's CSS size differs → `SetClientSize(NewX,NewY,false)` | resize/zoom/DPR change handling | keep |
| N19 | `Src/NSDLClient.cpp:16-60` `StaticConstructor` registers the client config properties **and** assigns compiled defaults mirroring the web ini's `[NSDLDrv.NSDLClient]` (2.19/2.35: "registration only makes an option ADDRESSABLE; it assigns no value … decals were off by accident", `3703f35`) | | keep; HP2's `UClient` adds `NoFractalAnim` and `ParticleDensity` (`Engine/Inc/UnCamera.h:383,394`) — give them compiled defaults too and put them in §9's `[NSDLDrv.NSDLClient]`. Keep the compiled defaults and the web ini in sync. |
| N20 | `Src/NSDLViewport.cpp:212-222` `InitKeyMap()` from the ctor (2.12) | v400/433 never call the v200 `InternalClassInitializer`, so the keymap stayed zero → dead keyboard | keep |
| N21 | `Src/NSDLViewport.cpp:853` `bWindowsMouseAvailable = !(NewBlitFlags & BLIT_Fullscreen);` (`1d3dfa5`: absolute UWindow cursor) | | keep (+ N17 note) |

### 6.2 HP2-specific viewport virtuals the NSDLDrv does not implement (and whether the web needs them)

HP2's `UViewport` adds virtuals with default bodies (`Engine/Inc/UnCamera.h:286-293`):

| Virtual | Default | Who calls it in HP2 | Web consequence / ACTION |
|---|---|---|---|
| `TryRenderDevice(const TCHAR*,INT,INT,INT,UBOOL)` | `{}` | nothing outside WinDrv | none (NSDLDrv's `UNSDLClient::TryRenderDevice(UViewport*,…)` is a different class; no clash) |
| `DrawString(DWORD,UFont*,INT&,INT&,const TCHAR*,const FPlane&)` | `{}` | Editor only (`Editor/Src/UnEdCam.cpp:2314-2445`) | none |
| `CreateNativeFont(const TCHAR*,int)` | `return NULL` | `UConsole::execCreateNativeFont` (`Engine/Src/UnCon.cpp:88-98`) ← `HGame/Classes/Menu/HPConsole.uc:934-937,964-967`, **only for `TRA/KOR/THA/JAP` languages** | INT/Western languages are unaffected. Asian-language web builds would get NULL fonts (GDI TTF fonts in the retail driver) → out of scope; record it in `Config/System/web/README.md`. |
| `LoadSmackerMovie/BlitNextSmackerFrame/ShutdownSmacker` | `false/true/true` | nothing in HP2 source or `.uc` (`grep -rni smacker` → only the declaration) | none |
| `ScreenShot(const TCHAR*)` + `snapBuff/snapU/snapV` | implemented in Engine (`UBOOL ScreenShot`) | HP2 save-game thumbnails? (verify: `grep -rn ScreenShot Source/Engine/Src Source/HGame`) | if save thumbnails read back the framebuffer, the WebGL path needs `glReadPixels` on the worker context — works, but `preserveDrawingBuffer` is not set; do the read inside the frame before `commit_frame`. Verify when saving is tested. |

## 7. Audio (NOpenALDrv / Module.alsoftConfig), WebRTCDrv, SoftDrv, XOpenGLDrv, OpenGLDrv web notes

### 7.1 Audio: NOpenALDrv, openal-soft web backend, `Module.alsoftConfig`

HP2 `Source/NOpenALDrv/` is the template's driver verbatim — written against **v400's** `UAudioSubsystem` and
libxmp/`.umx` music. HP2's `UAudioSubsystem` (`Engine/Inc/UnAudio.h`) is HP2's own (Ogg streaming, `PlayMusic(FString,
FLOAT)`, lip-sync, fades). PLAN.md §2 decides: first `SoundDrv.NullAudioSubsystem`, then a NOpenALDrv that is **HP2's
`ALAudio` ported to OpenAL 1.1/openal-soft**, using the template driver as the pattern. The web-specific pattern
elements to carry over:

| # | Template element | Where | Why (web) | ACTION for HP2's driver |
|---|---|---|---|---|
| A1 | openal-soft from `10-p/openal-soft-web@ufront/web`, `ufront_openal_soft(PLATFORM WEB EAX OFF)` | `Source/cmake/OpenALSoft.cmake`, `Source/CMakeLists.txt:614-615` (HP2 `:367-368`) | Web Audio **AudioWorklet** backend, mixer on its own pthread (hence `PTHREAD_POOL_SIZE=4`), HRTF + EFX available | keep; `EAX ON` only if the port keeps `EAXSet/EAXGet` calls (§3.2.1) — the template's route is EFX `AL_EFFECT_EAXREVERB` resolved at run time (`b35069a`, `d118e23`) |
| A2 | `OPENAL_LIBRARY OpenAL` target (gives `<AL/alext.h>`, `<AL/efx.h>`) | `NOpenALDrv/CMakeLists.txt:49-56` | the fork's headers, not Emscripten's `-lopenal` | keep |
| A3 | libxmp FetchContent + `BUILD_STATIC` save/restore | `NOpenALDrv/CMakeLists.txt:14-47` | `.umx` tracker music | **delete** (HP2 has no `.umx`: 111 files in `Assets/Music/*.ogg`). Add a Vorbis decoder instead: either FetchContent `xiph/ogg` + `xiph/vorbis` (native parity; watch for option-name collisions like libxmp's `BUILD_STATIC` — Vorbis/Ogg define `BUILD_SHARED_LIBS`/`INSTALL_*`, scope them) or Emscripten's `-sUSE_OGG=1 -sUSE_VORBIS=1` ports on web + distro/fetched on native. Record the choice in `meta.json` (`ufront_artifact_dep` if fetched, `buildFlags` if a port). `Thirdparty/Vorbis*` holds Win32 `.lib`s only — unusable. |
| A4 | Music pumped from `Update()` on the engine thread — "no background thread" | `NOpenALDrv.cpp:5-8` | **hard web constraint**: WasmFS JSImpl hooks only work on the thread that got the host's bootstrap message (`wasmfs-shim.js`: a hook on another pthread throws `WasmFS hook fired on the worker before the host handed over the sync channel`) | HP2's `FFileStream` pump thread (`FileStreamingThread`, created only in `Core/Src/UnVcWin32.cpp:665-690,1489`; the Ogg open is `ov_open(fopen(...))` at `UnVcWin32.cpp:866-869`, also Win32-only) must **not** become a pthread on web. Implement the POSIX `FFileStream` pump as a call from `UAudioSubsystem::Update()` (or from the engine tick) and keep `PTHREAD_POOL_SIZE=4`. |
| A5 | Music path | — | FetchFS serves by exact path + case | HP2 `ALAudio/Src/ALAudioSubsystem.cpp:1136-1140` builds `TEXT("..\\music\\") + Song + ".ogg"` and opens it **directly** — HP2's music loader does **not** go through `Paths=`. Rewrite as `TEXT("../Music/")` (on-disk case; §5.5 F2). `Paths=../Music/*.ogg` in the web ini is therefore **not required** for playback; it would only matter if something calls `appFindPackageFile` on a song (nothing does). The frontend manifest must still include `Music/*.ogg` (it does via the asset-tree manifest + `mods.extToFolder` `.ogg→Music` — `ufrontend-target-contract.md`). |
| A6 | `StaticConstructor` registers config keys as `CPF_Config` with compiled defaults: `OutputRate`, `UseHRTF` (default **false**), reverb, etc. | `NOpenALDrv.cpp:112-142` | the launcher's HRTF toggle writes `-ini:System:[NOpenALDrv.NOpenALAudioSubsystem]:UseHRTF=…` (`target.json audioHrtf`) | register the same key names in HP2's driver (`UseHRTF`, `OutputRate`) so the frontend switches work unchanged; HP2-specific keys (`[ALAudio.ALAudioSubsystem]` retail: `UseEAX`, `Use3dHardware`, `UseSpatial`, `MusicVolume`, `SoundVolume`, `Channels`…) keep their HP2 names under the new section. |
| A7 | device attrs `ALC_FREQUENCY, OutputRate, ALC_HRTF_SOFT, UseHRTF?…, ALC_OUTPUT_MODE_SOFT, UseHRTF ? ALC_STEREO_HRTF_SOFT : ALC_ANY_SOFT` (2.61 fix: the wrong key had made HRTF a no-op) | `NOpenALDrv.cpp:185-193` | | copy verbatim |
| A8 | `Module.alsoftConfig` | **not in the engine** — the host writes `window.Module['alsoftConfig'] = alsoftConfigText(buffer)` just before `callMain` (`ufrontend/packages/frontend/src/shell/boot.ts:349-355`) when the target declares `audioBuffer`; text is `[general]\nperiods=N\n\n[web]\nperiod-ms=M\n` (`shell/audioConfig.ts`); the fork's `alc/alconfig.cpp` reads it | latency tuning (default 10 ms × 3) | **engine: nothing to do** (it is library-side). Frontend: give `targets/hp2/target.json` an `audioBuffer {periodMs:10, periods:3}` only once HP2's NOpenALDrv links openal-soft; omit it while the web build runs `SoundDrv.NullAudioSubsystem` (the host then skips the line). |
| A9 | link flags `-sPTHREAD_POOL_SIZE=4`, `-sNO_DISABLE_EXCEPTION_CATCHING` | `Game/CMakeLists.txt` (§2.2.18-19) | openal-soft threads + its C++ exceptions | already present |
| A10 | Launcher web override `AudioDevice=NOpenALDrv.NOpenALAudioSubsystem` | `Launch.cpp:495` (unconditional under `__EMSCRIPTEN__`) | | While HP2's driver is the Null one: guard on `HP2_HAS_NOPENALDRV` and fall back to `SoundDrv.NullAudioSubsystem`; otherwise `InitAudio`'s `LOAD_NoFail` class load → `appErrorf` → abort before frame 1. Note `BUILD_NOPENALDRV` is **forced ON** on web (`Source/CMakeLists.txt:70`): while HP2's NOpenALDrv does not compile, temporarily force it OFF on web too (and drop `-sPTHREAD_POOL_SIZE` pressure only if nothing else spawns threads). |

### 7.2 WebRTCDrv (+ IpDrv web guards)

HP2 `Source/WebRTCDrv/` = template (0 diffs). The `UNetDriver`/`UNetConnection` face it uses exists in HP2 433
(`Engine/Inc/UnNetDrv.h:72-80` `LowLevelDestroy/LowLevelGetNetworkNumber/InitConnect/InitListen/TickFlush/TickDispatch/Exec`;
`Engine/Inc/UnConn.h:149-169` `UNetConnection(UNetDriver*, const FURL&)`, `LowLevelGetRemoteAddress/LowLevelDescribe/LowLevelSend/InitOut`),
so the 433 adaptation is small (`DECLARE_CLASS` 4th arg; `template-drivers.md`).

| # | Site | Code | ACTION |
|---|---|---|---|
| W1 | `WebRTCDrv/WebRTCDrv.cpp:36` | `#define WEBRTC_PEER_PREFIX "ut99"` ("MUST match `targets/ut99/target.json network.peerPrefix`"); used at `:225` `"%s-%s-srv"` | → `"hp2"` (PLAN.md §2: `network.peerPrefix = "hp2"`) |
| W2 | `WebRTCDrv.cpp:~429` | listen-status fallback `SName = TEXT("UT Server");` | → `TEXT("HP2 Server")`, equal to `targets/hp2/target.json network.defaultServerName` |
| W3 | `WebRTCDrv.cpp` `InitListen` reads `[Engine.GameReplicationInfo] ServerName` when the URL has none (2.40, `629992d`) | | keep; HP2 retail `Default.ini` has the section (stock `ServerName=Another UT Server` — change it in the web ini to an HP2 name and mirror as `network.serverName.stock`) |
| W4 | `WebRTCDrv.cpp:48-70` 9 transport imports under `#ifdef __EMSCRIPTEN__` (native inline no-op stubs); `WebRTCLan.cpp:43-56` 9 discovery imports; + `webrtc_set_listen_status` | | keep; names are ABI (§8) |
| W5 | `WebRTCDrv.cpp:445-447` `emscripten_sleep(50)` while a handshake is pending (never in the data phase) | needs `-sASYNCIFY` and `emscripten_sleep` in `ASYNCIFY_IMPORTS` | keep |
| W6 | `WebRTCLan.cpp` synthetic LAN (UdpLink bind → WebRTC discovery), reads `[UBrowser.UBrowserLocalFact] BeaconProduct` / `[IpDrv.UdpBeacon] BeaconProduct` | HP2 ships no UBrowser / server browser | keep compiled (inert; IpDrv's `UdpLink.cpp` calls into it); do not invest. |
| W7 | IpDrv web guards: `IpDrv/Src/UdpLink.cpp:38-40,56-70,138-145,178-182,207-215` (`WebRTCLan_*`), `IpDrv/Src/UnSocket.cpp:226-235` (`#ifndef __EMSCRIPTEN__` around `HOST_NOT_FOUND/TRY_AGAIN/NO_RECOVERY` — duplicate `case` under musl) | | keep through the 433 adaptation. `IpDrv.u` script package: HP2 has none (PLAN Phase B step 6); without `IpDrv.u` the `UdpLink` natives are never reached, but `IpDrv.TcpNetDriver` (C++-only class) still resolves — that is all the web needs (the launcher overrides `NetworkDevice` to WebRTCDrv anyway). |
| W8 | `Launch.cpp:54` `UEngine* GWebRTCEngine` defined by the launcher, `extern` in the driver | | keep in HP2 `Launch.cpp` |
| W9 | Whole feature | — | **Decision needed:** HP2 is single-player. If `targets/hp2/target.json` disables `network`, WebRTCDrv still links (the `NetworkDevice` override needs a class) but E4/E5/E11 (§5.3) and W1-W3 are cosmetic. Keep it linked either way — `UGameEngine::Init` loads `ini:Engine.Engine.NetworkDevice` lazily only on listen/connect, but a missing class there is a `LOAD_NoFail` abort. |

### 7.3 SoftDrv (`Source/SoftDrv/WEB-NOTES.md`)

HP2's SoftDrv is **its own 433 device** (6 source files differ from the template's v400 one) and has no `WEB-NOTES.md`.

| # | Template web lesson | HP2 state | ACTION |
|---|---|---|---|
| S1 | §4 of WEB-NOTES + `SoftDrvPrivate.h:39-53`: `MaximumXScreenSize/MaximumYScreenSize = SOFTDRV_MAX_SCREEN` (default 4096; the `wasm` preset passes 8192) — the static span/coord tables are bounded by it | HP2 `SoftDrv/Src/SoftDrvPrivate.h:17-18`: **`MaximumYScreenSize 1200`, `MaximumXScreenSize 2048`**; `SOFTDRV_MAX_SCREEN` is defined by `SoftDrv/CMakeLists.txt:37-38` but never read | **BLOCKER for `-SOFTWARERENDERER` on web**: the NSDLDrv hands SoftDrv the device-pixel canvas (N3/N11; 2560×1440 at DPR 1, 5120×2160 on a 4K panel). Port the template's `#ifndef SOFTDRV_MAX_SCREEN … #define MaximumYScreenSize SOFTDRV_MAX_SCREEN` block, then check every HP2 SoftDrv table indexed by `SizeX/SizeY` (template 2.12 list: `Setup`, `MMXSetup`, `SpanIndex`, `UCoordTable`, `Photon`) and the `SetRes` clamp + `DrawPoly` skip + `DrawTile` span-fill clamp. |
| S2 | §1: `ASM=0` on web → the portable C rasteriser; `#if !ASM && __INTEL__ #define GIsMMX false` (`SoftDrvPrivate.h:32`) | HP2 `SoftDrv.cpp:44` `SupportsFogMaps = GIsMMX;`, `:101 if(GIsMMX)` | port the `GIsMMX` force-off (native `-m32`); on wasm `__INTEL__` is undefined and `GIsMMX` is the Core global (false) — verify HP2's C fallbacks exist for every `#if ASM` block (compile will say). Template also un-gated fog maps from MMX (`SoftDrv.cpp:45`) — port it or HP2 loses volumetric fog on web. |
| S3 | §2: web draws into `SoftFB` (ARGB8888, 4 bytes) sized in `Lock`; §3 the size changes at runtime (`WINDOWEVENT_RESIZED`) | — | HP2 SoftDrv must handle `ColorBytes=4` and a `SetRes` on every resize without leaking/stale tables |
| S4 | §5: a clean `-O2` run does not clear a memory bug (an overrun faulted only at `-O3`); §6: test across RESOLUTIONS with `ufrontend/packages/e2e/tools/softdrv-matrix.cjs` | | copy `WEB-NOTES.md` into `Source/SoftDrv/` (adapt the file refs), and run the matrix on `wasm-perf` |
| S5 | `e8198af` FogAvg off-by-one, `b40994b` volumetric fog + texture filtering in the C path | HP2 own code | port by analogy if HP2's C path has the same `FogAvg` loop |

### 7.4 XOpenGLDrv (`Source/XOpenGLDrv/WEB_INTEGRATION.md`)

The doc is **historical**: it describes a `WASM_USE_XOPENGL` option and `-sMIN_WEBGL_VERSION=2 -sFULL_ES3`, both gone
since 2.9 made XOpenGL and UTGLR coexist in one binary with `MIN_WEBGL_VERSION=1` (§2.2.4). Still true and relevant:

| # | Fact | ACTION for HP2 |
|---|---|---|
| X1 | the driver forces `OpenGLVersion=ES_WEBGL` under `__EMSCRIPTEN__` in `Init` and adopts the current WebGL2 context via `emscripten_webgl_get_current_context()` (the one NSDLViewport created, N8) | the `hp2-v433-compat` fork branch equals `ut99-v400-compat` today, so this is present; the 433 face (`DrawComplexSurface(…,PolyFlags,cAlpha)`, `DrawTriangles`, `MaxVertices`, `GetOsSurface`) is the fork's work (`renderer-forks.md`) — keep every `__EMSCRIPTEN__` block intact while adding `ENGINE_VERSION==433` gates |
| X2 | S3TC via `WEBGL_compressed_texture_s3tc` (desktop extension name absent on ANGLE); non-4-aligned DXT1 mips software-decoded (vendored gl4es decoder) | none; if HP2 textures carry DXT mips they work on web |
| X3 | the fork's CMake uses `file(GLOB Src/*.cpp)` without `CONFIGURE_DEPENDS` | re-run `cmake --preset wasm` after adding a `.cpp` to the fork |
| X4 | glad resolves GL through `emscripten_webgl_get_proc_address` → needs `-sGL_ENABLE_GET_PROC_ADDRESS=1` (§2.2.5) | present |
| X5 | XOpenGL owns its own FBO, so `offscreen-depth24.sh` does not affect it | none |

### 7.5 OpenGLDrv / UTGLR + gl4es (`Source/OpenGLDrv/WEB-INTEGRATION.md`)

| # | Fact | ACTION for HP2 |
|---|---|---|
| G1 | "No engine / ABI / glue changes are required"; NSDLDrv opens a GL context only when the device name contains `OPENGL` → keep the class path `OpenGLDrv.OpenGLRenderDevice` | keep |
| G2 | gl4es emits client-side arrays → `-sFULL_ES2=1` (§2.2.6); 24-bit depth on FBO 0 via `offscreen-depth24.sh` (UTGLR z-fought in bands on 16-bit, `4356c7b`) | present |
| G3 | web ini: `UseFragmentProgram=0`, `UsePalette=0` (+ the rest of the template's `[OpenGLDrv.OpenGLRenderDevice]` set) | carry the section into HP2's web `Default.ini` (§9) |
| G4 | `UTGLR_ENGINE_UT99DC` guards every engine-specific deviation in the fork | HP2 adds `UTGLR_ENGINE_HP2` (already in `Source/OpenGLDrv/CMakeLists.txt`); the only diff vs template in `OpenGLDrv/` is that CMake file |
| G5 | gl4es built `NOX11 NOEGL STATICLIB DEFAULT_ES=2 NO_INIT_CONSTRUCTOR` (`Source/CMakeLists.txt` HP2 `:307-343`) — `NO_INIT_CONSTRUCTOR` means UTGLR must call `initialize_gl4es()` itself (the fork does under `__EMSCRIPTEN__`) | keep; the branch mismatch (`hp2` clone vs `fix/cubemap-bind-active-unit` default) is §1.4.2 |

## 8. vendor/glue ABI: exports/imports the engine must provide

`vendor/glue/{ue_abi.h (92 lines), webrtc-shim.js (129), wasmfs-shim.js (147)}` are **identical** to the template's
(`diff -q` clean) and to `@ufront/glue` at `ABI_VERSION 1`. CLAUDE.md: never change them. What the engine side must
provide / may assume:

### 8.1 Imports (engine calls, JS provides) — `UE_IMPORT`, `ue_abi.h:60-78`

| Symbol(s) | Provided by | Proxy | Called from (HP2 = template) |
|---|---|---|---|
| `webrtc_init_listen, webrtc_connect, webrtc_accept_connection, webrtc_close, webrtc_destroy, webrtc_get_connection_state, webrtc_check_pending_connect, webrtc_set_listen_status` | `webrtc-shim.js:50-88` → `Module.UE.net.*` | `__proxy: 'sync'` (runs on main, blocks the engine thread) | `WebRTCDrv/WebRTCDrv.cpp:48-70` (`webrtc_check_pending_connect` is declared, not called by the UE1 driver — dead-stripped, fine) |
| `webrtc_send, webrtc_recv` | `webrtc-shim.js:59-63` | **not proxied** — shared-memory packet rings (`901db66`, 2.13.6) | `WebRTCDrv.cpp` data path |
| `webrtc_discover_servers, webrtc_get_server_count, webrtc_is_discovery_done, webrtc_get_server_info/map/gametype/players/maxplayers/ping` | `webrtc-shim.js:92-127` | `sync` | `WebRTCDrv/WebRTCLan.cpp:43-56` (IpDrv `UdpLink` synthetic LAN). Lesson `3a62947`: never poll `webrtc_is_discovery_done` every frame (each is a sync proxy round-trip) |

All forwarders assert `Module.UE.abiVersion === 1` on first use (`webrtc-shim.js:42-44`) — the host (not the engine) injects `Module.UE`.
**Engine rule:** declare these only under `#ifdef __EMSCRIPTEN__`, provide inline no-op stubs natively (as the template does),
and **never rename** them (the frontend's ABI parity test keys on `UE_IMPORT`/`UE_EXPORT`).

### 8.2 Exports (engine provides, JS calls) — `UE_EXPORT`, `ue_abi.h:84-85`

| Symbol | Engine obligation | HP2 ACTION |
|---|---|---|
| `ue_exec_command(const char*) → char*` | `extern "C" EMSCRIPTEN_KEEPALIVE`, in `-sEXPORTED_FUNCTIONS` as `_ue_exec_command`; returns a pointer valid until the next call | keep `Launch.cpp:121-146` as is (§4.1.5). Under `PROXY_TO_PTHREAD` the host posts `{ufrontExec: cmd}` and the worker calls it **from a JS message event** (`wasmfs-shim.js:99-121`), i.e. while the main loop is Asyncify-suspended inside `emscripten_sleep`/`ufront_yield_frame`. So the command must **not** itself reach an Asyncify import (`emscripten_sleep`, a FetchFS on-demand `__syscall_openat`). HP2's `UGameEngine::Exec` `OPEN`/`START` defer through `SetClientTravel` (`Engine/Src/UnGame.cpp:338-352`) → safe. Avoid exposing HP2 console commands that load synchronously (e.g. an HP2 `LOADGAME`-style native that calls `LoadPackage` inline) to the host; verify each command the E2E uses. All helpers used exist in HP2 (`appStrnicmp`/`appStrfind` `Core/Inc/UnFile.h:318,324`, `FStringOutputDevice` `UnTemplate.h:1343`). |
| `webrtc_deferred_open()` | **UE1 engines do not define it and must not export it** (ut2004 only). Exporting an undefined symbol is a hard wasm-ld error | keep it out of `EXPORTED_FUNCTIONS` (as HP2's `Game/CMakeLists.txt` already does) |

### 8.3 Emscripten/runtime exports the host needs (link flags, not ABI header)

| Needed by the host for | Symbol / method | Where set |
|---|---|---|
| FetchFS JSImpl backend creation (on main) | `_wasmfs_create_jsimpl_backend`, `_wasmfs_create_file` (Emscripten), `_ufront_wasmfs_register`, `_ufront_wasmfs_set_pending` (`wasmfs-shim.js:42-60`) | `EXPORTED_FUNCTIONS` (§2.2.12) |
| boot + string marshalling | `_main`, `_malloc`, `_free`, `_close`; `callMain`, `stringToUTF8`, `UTF8ToString`, `HEAPU8`, `stackSave`, `stackAlloc`, `stackRestore`, `lengthBytesUTF8` | `EXPORTED_FUNCTIONS` / `EXPORTED_RUNTIME_METHODS` |
| handing the FS sync channel to the engine worker | `PThread` | `EXPORTED_RUNTIME_METHODS` — omit it and the host's bootstrap is a silent no-op → the worker's first asset read throws |
| worker bootstrap | `$ueWorkerBoot` (`__postset: 'ueWorkerBoot();'`) | `-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['$ueWorkerBoot']` — otherwise dead-stripped silently |
| worker-side glue | `importScripts('ufront-glue.js')` on the worker (`wasmfs-shim.js:136`) | the **published folder** must carry `ufront-glue.js` next to `SDLLaunch.js` (frontend's `verify.mjs REQUIRED_IN_FOLDER`; frontend-owned) |
| async imports | `ASYNCIFY_IMPORTS=['emscripten_sleep','emscripten_set_main_loop','__syscall_openat']` + auto `__asyncjs__*` | §2.2.3 |

### 8.4 JS objects the host injects before `main()` (engine may assume)

`Module.UE = { abiVersion: 1, idb, net, fs }` (`ufrontend/packages/frontend/src/shell/boot.ts` ~`:435-444`), `Module.canvas`
(`#canvas`, transferred to the worker), `Module.arguments` (§4.6), `Module.alsoftConfig` (§7.1 A8), `Module.print`/`printErr`
→ console (the log taps). The engine never reads `Module.UE` directly — only the shims do.

### 8.5 Checklist

- [ ] `vendor/glue/*` byte-identical to `ufrontend/packages/glue/src/shim/*` of the `hp2` branch (`diff`); refresh only with an ABI bump decided in ufrontend.
- [ ] `nm`/`wasm-objdump -x SDLLaunch.wasm | grep -c webrtc_` shows the 19 imports (18 if `check_pending_connect` stripped) and the `ue_exec_command` export.
- [ ] `grep -c '_webrtc_deferred_open' SDLLaunch.js` = 0.

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

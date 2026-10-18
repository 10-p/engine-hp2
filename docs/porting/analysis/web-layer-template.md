Record — kept as history; the current state is in README.md, BUILD.md and the change inventory.

# web-layer-template — the Emscripten/web layer of engine-ut99, end to end, as an HP2 replication checklist

**Key:** `web-layer-template` · **Run:** 2026-09-26 (fresh re-run; supersedes the 2026-09-25 draft) ·
**Template (READ ONLY):** `/home/ion/engines/engine-ut99` · **Port target:** `/home/ion/hp2-ufrontend/HarryPotter2` ·
**Frontend clone:** `/home/ion/hp2-ufrontend/ufrontend` (branch `hp2`) · **Ancestor:** `/home/ion/epic-sources/ut99-v400`

_Status: complete (sections 0-8 written in the first pass, 9-16 completed 2026-09-26)._

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

Today `Config/System/web/` holds only `README-ut99.md` (the template's README copied verbatim, still naming
`UnrealTournament.ini` and `Source/UnrealTournament/CMakeLists.txt`). Three files must be authored. Principle carried over
from the template (`e762cf4`, `a1e810b`): **the preloaded web `Default.ini` is the single source of truth for web config**;
every key is derived from a driver's or class's own property registration, never guessed; the asset-tree copy never wins
(the frontend's `manifestExclude` lists `System/Default.ini`, `System/DefUser.ini` and the derived `System/Game.ini`,
`System/User.ini` so a stale install cannot shadow them).

### 9.1 How the template's web ini is built (what to mirror, what not)

Template `Config/System/web/Default.ini` (705 lines) = the UT99 retail `Default.ini` + these web-specific edits
(`diff` against the parent `Config/System/Default.ini`, which is the Dreamcast set):

| Template web section/key | Purpose | HP2 web ini |
|---|---|---|
| `[Engine.Engine] FrameRateCap=120` (`:21`) | read by the launcher main loop, never by the engine (§4.4.3); written by the frontend through `-ini:System:[Engine.Engine]:FrameRateCap=<n>` (`targets/*/target.json frameRateCap`) | **add** |
| `[Engine.Engine] GameRenderDevice=SoftDrv.SoftwareRenderDevice` | left at the retail value on purpose: `SelectRenderDevice()` step 2 refuses a persisted SoftDrv without the `ufrontUserRenderDevice` marker and falls to the preference order (XOpenGL) (template `Launch.cpp:406-435`) | see 9.2 (HP2 may name XOpenGL directly) |
| `[Engine.Engine] ViewportManager=WinDrv.WindowsClient`, `AudioDevice=Galaxy…`, `NetworkDevice=IpDrv.TcpNetDriver` | **left retail**; the launcher overrides all three under `__EMSCRIPTEN__` before `Engine->Init()` (§4.3.5) | the brief asks for the web values in the ini: write the **real** web values (9.2) — belt and braces; the launcher override stays |
| `[NOpenALDrv.NOpenALAudioSubsystem]` | real audio driver config (`UseHRTF=False` default; the frontend's HRTF switch writes it) | add (keys from HP2's NOpenALDrv port — §7.1 A6) |
| `[WebRTCDrv.WebRTCNetDriver]` (2.12) | net driver rates; without the section a listen host fell to ~30 fps online | add |
| `[OpenGLDrv.OpenGLRenderDevice]` full UTGLR set (`UsePalette=False`, `UseFragmentProgram=False`, `UseAA`, `MaxAnisotropy=16` …) | WebGL1 path; values tuned in 2.34/`a1e810b` | add (copy verbatim; the UTGLR fork's HP2 branch registers the same keys — verify against `utglr-copy@hp2 OpenGLDrv.cpp` `StaticConstructor` / `CPF_Config` list) |
| `[XOpenGLDrv.XOpenGLRenderDevice]` with `OpenGLVersion=ES_WEBGL` + the 12.x visual set and the explanatory comment block | WebGL2 path | add (copy verbatim) |
| `[NOpenGLESDrv.NOpenGLESRenderDevice]` | NOpenGLES | **omit** (HP2 builds no NOpenGLESDrv) |
| `[ufront.AutoFOV]` (read by `Engine/Src/UnCamera.cpp:270-293`) | UT99 engine feature (Hor+ FOV, HUD auto-scale via `HudScale`) | **omit** — HP2's Engine has no AutoFOV code (`grep -rl AutoFOV Source` → nothing). Only add the section together with a port of the feature (not a web requirement). |
| `[NullDrv.NullRenderDevice]` | `-NULLRENDERER` | add |
| `[NSDLDrv.NSDLClient]` (1280×720, `Decals=True`, `Brightness=0.5`, `CaptureMouse=True`, `StartupFullscreen=False`, joystick set) | the client this build actually uses; mirrors `UNSDLClient::StaticConstructor` compiled defaults (`3703f35`) | add + HP2 keys (9.2) |
| `[PackageRemap] UnrealShare=UnrealI` | UT99 content compat | **omit** (HP2 retail has none; its `UnrealShare.u` is its own) |
| `[Engine.GameEngine] ServerActors=… / ServerPackages=…Skins/Botpack` | UT99 | **omit** (§14) |
| `[Engine.Player] ConfiguredInternetSpeed=1000000` (`7cbd017`) | web net speed | add only if multiplayer is exposed; harmless otherwise |
| `[Botpack.*]`, `[UMenu.*]`, `[UBrowser*]`, `[UTMenu.*]`, `[UWeb.WebServer]`, `[UTServerAdmin.*]`, `[IpServer.*]`, `[Galaxy.*]`, `[GlideDrv/MetalDrv/SglDrv/D3DDrv]`, `[XDrv.XClient]`, `[WinDrv.WindowsClient]` | UT99/Windows | **omit** all (§14). (The template keeps many of them only because it started from a retail file.) |

### 9.2 `Config/System/web/Default.ini` for HP2 — section by section

Derived from HP2's retail `Config/System/Default.ini` (509 lines, sections at `:1,14,19,40,66,…`) plus the web edits above.
Each row gives the value and the fact it hinges on.

**`[URL]`** — copy retail `:1-12`, with two on-disk-case fixes:

| Key | Value | Why |
|---|---|---|
| `LocalMap` | `startup.unr` (retail says `Startup.unr`) | the file on disk is **`Assets/Maps/startup.unr`** (lowercase). HP2's `appFindPackageFile` does fall back to a case-insensitive `FindFiles` scan (`Core/Src/UnMisc.cpp:1516-1580`, logs `Case-insensitive search: …`), so the retail value works, but it costs a directory scan per path and depends on FetchFS listing being complete. Write the on-disk name. (Alternatively rename nothing and accept the scan; do NOT rename the asset — the frontend manifest keys on it.) |
| `Map` | `autoplay.unr` (retail) | **no `autoplay.unr` exists** in `Assets/Maps/`. It is only used for a URL with no map part; keep the retail value but **verify** nothing browses an empty map on web (if something does, set `Map=Entry.unr`). |
| `Class` | `Engine.Playerpawn` (retail) | the real player class comes from `DefUser.ini [DefaultPlayer] Class=HGame.harry` |
| `Name=Player`, `MapExt=unr`, `SaveExt=usa`, `Port=7777`, `Protocol=unreal` | retail | the frontend's player-name setting writes `[URL] Name` through `-ini:User:…`? — **verify** in `targets/hp2/target.json` (the ut99 descriptor writes `[DefaultPlayer] Name` in `User.ini`) |

**`[FirstRun]`** — `FirstRun=0` only (template). Drop retail `Reconfig=1`/`ForceSoftware=0`: read only by the Win32
hardware auto-detect in `Engine/Inc/UnEngineWin.h:~30-60` (writes `[optionsettings]`), which the SDL launcher does not run.

**`[Engine.Engine]`**

| Key | Value | Why / source |
|---|---|---|
| `FrameRateCap` | `120` | §4.4.3; HP2's `ULevel::Tick` clamps `DeltaSeconds` to `[0.005,0.10]` (`Engine/Src/UnLevTic.cpp:960`), so above 200 fps the world runs fast — the cap (and X2's floor change) matter for HP2 exactly as for UT99. Default = frontend's `frameRateCap.default`. |
| `GameRenderDevice` | `XOpenGLDrv.XOpenGLRenderDevice` | the brief asks for the web value; it is a hardware default (`IsHardwareDefaultEligible`), so `SelectRenderDevice` step 2 adopts it directly and `NSDLViewport::OpenWindow` sees `OPENGL`+`XOPENGL` → WebGL 2 context (N5). Retail `D3DDrv.D3DRenderDevice` would also work (rejected as not compiled → preference order) but lies about the web. |
| `WindowedRenderDevice` | `SoftDrv.SoftwareRenderDevice` | the `TryRenderDevice` fallback (`NSDLViewport.cpp:654-656`). ⚠ If XOpenGL fails *after* the viewport created a WebGL context, the SoftDrv fallback runs without a `SoftFB` (N10 allocates it only on the non-GL branch) — same as template; accept, and rely on `-SOFTWARERENDERER` for a deliberate CPU run. |
| `RenderDevice` | `SoftDrv.SoftwareRenderDevice` | retail says `GlideDrv.GlideRenderDevice` (never loaded on web); make it a compiled class so nothing ever resolves a missing package |
| `AudioDevice` | `NOpenALDrv.NOpenALAudioSubsystem` **once HP2's NOpenALDrv links**; until then `SoundDrv.NullAudioSubsystem` | matches the launcher override (§7.1 A10). Keep ini and launcher in step. |
| `NetworkDevice` | `WebRTCDrv.WebRTCNetDriver` | brief; also what the launcher forces |
| `DemoRecordingDevice` | `Engine.DemoRecDriver` | retail |
| `ViewportManager` | `NSDLDrv.NSDLClient` | brief; launcher forces it too |
| `Console` | `HGame.HPConsole` | retail `:27` (the UWindow one is commented out) |
| `Language` | `int` | retail. ⚠ also read by the "gk 10/15/01" second cache on the literal file name `default.ini` (§2.3.3) — the web file is preloaded as `/System/Default.ini`; fix the literal or the language becomes garbage |
| `GameEngine`, `EditorEngine`, `DefaultGame=Engine.GameInfo`, `DefaultServerGame=Engine.GameInfo`, `Render=Render.Render`, `Input=Engine.Input`, `Canvas=Engine.Canvas` | retail | |
| `EASplashWaitTime`, `WBSplashWaitTime` | omit | Win32 launcher splash only (`Source/Launch/Src/Launch.cpp`) |
| `CdPath` | **do not add** (template has `CdPath=D:`) | HP2 `appInit` (`UnMisc.cpp:1980-1990`) clears `GCdPath` only if `FileSize("..\\Textures\\Palettes.utx")>=0` — a backslash path that never exists on WasmFS — so `CdPath=D:` would make every `appFindPackageFile` miss do a second `D:/System/…` pass. |

**`[Core.System]`** — the `Paths` list has two HP2-specific constraints found in `appInit`:

```ini
[Core.System]
PurgeCacheDays=30
SavePath=../Save
CachePath=../Cache
CacheExt=.uxx
Paths=../System/*.u
Paths=../Maps/*.unr
Paths=../Textures/*.utx
Paths=../Sounds/*.uax
Paths=../Music/*.umx
Paths=../Save/*.usa
Suppress=DevLoad
Suppress=DevSave
Suppress=DevNetTraffic
Suppress=DevGarbage
Suppress=DevKill
Suppress=DevReplace
Suppress=DevSound
Suppress=DevCompile
Suppress=DevBind
Suppress=DevBsp
Suppress=ScriptWarning
Suppress=Localization
```

| Fact | Where | Rule for the web ini |
|---|---|---|
| `GSys->Paths[5]=*(GSys->SavePath * TEXT("*.usa"));` — hard-coded **index 5** | `Core/Src/UnMisc.cpp:2006` (branch taken when `appUserDir()` is non-empty — native POSIX may be; web returns `""`) | keep **exactly six `Paths=` lines in retail order**, the `.usa` one sixth. Do not drop the useless `../Music/*.umx` line (HP2 has no `.umx`; dropping it moves the save entry to index 4 and makes `Paths[5]` out of range). |
| `GConfig->SetString( TEXT("Core.System"), TEXT("paths"), *(GSys->SaveSlotPath * TEXT("*.usa")) ); GConfig->Flush(0);` at the end of `appInit` | `UnMisc.cpp:2062-2065` | `FConfigSection` is `TMultiMap<FString,FString>` (`Core/Inc/FConfigCacheIni.h:14`), `FString==` is case-insensitive (`UnTemplate.h:1186-1193`) and `TMap::Find` returns the **last-inserted** pair of a key (chains are built head-first, `UnTemplate.h:1485-1507`, `Find` `:1570-1576`). So this rewrites the **last** `Paths=` entry of `Game.ini` to the save-slot path and flushes `Game.ini` on every boot. ⇒ the `.usa` entry **must be the last `Paths=` line** (it is, above). If anyone appends a `Paths=` after it (e.g. `../Music/*.ogg`), boot 1 silently replaces *that* line in the persisted `Game.ini`, and because the frontend persists and restores `/System/Game.ini` (IndexedDB `hp2-user-data`), the damage survives reloads. |
| retail `Paths=../save/*.usa` (lowercase) | `Config/System/Default.ini:50` | write `../Save/*.usa` — the directory is `Save/` (and the engine creates `..\\Save` itself, see next row) |
| `SavePath` is **overwritten in code**: `GSys->SavePath = TEXT("..\\Save");` when `appUserDir()` is empty | `UnMisc.cpp:2008-2011`, then `SetString(SavePath, SaveSlotPath)` `:2062` | the ini value is irrelevant on web; the **source** must change (`TEXT("..") PATH_SEPARATOR TEXT("Save")`), otherwise `MakeDirectory("..\\Save")` creates a file literally named `..\Save` inside `/System` and every save fails (§2.3.4, §5.3 E9, §5.5 F2). |
| `Paths=../Music/*.ogg`? | brief's question | **Not needed.** HP2 music does not go through `Paths=`: `ALAudio/Src/ALAudioSubsystem.cpp:1136-1140` builds `"..\\music\\" + Song + ".ogg"` and opens it directly (§7.1 A5). The port rewrites that to `../Music/<Song>.ogg` (on-disk case, `/`). Adding an `.ogg` line would also trip the `paths` rewrite above unless placed before the `.usa` line, and would let `appFindPackageFile` offer `.ogg` files to the package linker. Leave it out. |
| `Suppress=` | retail `:52-65` | keep retail's list (it suppresses `ScriptWarning` and `Localization`, which HP2 emits in bulk). For first web boots consider **not** suppressing `DevLoad` (the loader trace is the fastest way to find the next missing package) — do it via `-ini:` or a debug copy, not in the shipped file. |

**`[Engine.GameEngine]`** — `CacheSizeMegs=4`, `UseSound=True`. **No `ServerActors`/`ServerPackages`** (retail `:69-74`
lists the UT99 ones; none exist in HP2; the launcher also empties `ServerActors`, §4.3.7).

**`[NSDLDrv.NSDLClient]`** — template section verbatim (`WindowedViewportX=1280`, `WindowedViewportY=720`,
`WindowedColorBits=32`, `FullscreenViewportX=1280`, `FullscreenViewportY=720`, `FullscreenColorBits=32`,
`MipFactor=1.0`, `Brightness=0.5`, `CaptureMouse=True`, `CurvedSurfaces=True`, `TextureDetail=High`, `SkinDetail=High`,
`ScreenFlashes=True`, `NoLighting=False`, `MinDesiredFrameRate=20.0`, `Decals=True`, `NoDynamicLights=False`,
`DefaultDisplay=0`, `StartupFullscreen=False`, `UseJoystick=True`, `DeadZoneXYZ=0.1`, `DeadZoneRUV=0.1`, `ScaleXYZ=200.0`,
`ScaleRUV=500.0`, `InvertY=False`, `InvertV=False`) **plus HP2's two extra `UClient` keys**, registered in
`Engine/Src/UnCamMgr.cpp:64-65`: `NoFractalAnim=False`, `ParticleDensity=1` (retail WinDrv value `Default.ini:102`).
Notes: the window size is only the *initial* size — on web NSDLViewport immediately adopts the canvas's device-pixel size
(N3/N11). HP2 retail `Brightness=0.4` (WinDrv) vs template `0.5`: pick HP2's `0.4` if the game's art was tuned for it
(verify visually). Keep `UNSDLClient::StaticConstructor`'s compiled defaults equal to this section (N19). ⚠ HP2's `UClient`
bools are adjacent native `BITFIELD`s: port the `c506ba7` bool-bitmask restore (§13) or `CaptureMouse=False`-style
overrides of a compiled `=1` default silently fail.

**`[XOpenGLDrv.XOpenGLRenderDevice]`, `[OpenGLDrv.OpenGLRenderDevice]`, `[NullDrv.NullRenderDevice]`** — template sections
verbatim (template web `Default.ini:280-330` OpenGLDrv, `:575-624` XOpenGLDrv, `:671-677` NullDrv; `[NSDLDrv.NSDLClient]` is `:679-705`); `[SoftDrv.SoftwareRenderDevice]`: HP2's own SoftDrv
registers its own key set — derive from `Source/SoftDrv/Src/SoftDrv.cpp` `StaticConstructor` (retail HP2 section
`Default.ini:213-221` is the starting point: `Translucency`, `VolumetricLighting`, `ShinySurfaces`, `Coronas`,
`HighDetailActors`, `HighResTextureSmooth`, `LowResTextureSmooth`, `FastTranslucency`).

**`[ALAudio.ALAudioSubsystem]`** — keep `Channels=32` even on web: HP2's `appInit` reads it to size the `FFileStream`
channel table (`UnMisc.cpp:1993-1996`) regardless of the audio driver. The other ALAudio keys are dead on web.

**`[NOpenALDrv.NOpenALAudioSubsystem]`** — only once HP2's NOpenALDrv exists; keys = its `StaticConstructor` registrations.
Must include `UseHRTF=False` and `OutputRate=44100` so the frontend's `audioHrtf` switch (`-ini:System:[NOpenALDrv.NOpenALAudioSubsystem]:UseHRTF=…`)
works unchanged. HP2-scale volumes (`MusicVolume=0.5`, `SoundVolume=0.9` floats, retail `ALAudio`) if the port keeps HP2's
float semantics — do not copy the template's byte-scale `MusicVolume=160`.

**`[WebRTCDrv.WebRTCNetDriver]`** — template block verbatim (`AllowDownloads=True`, `ConnectionTimeout=15.0`,
`InitialConnectTimeout=500.0`, `AckTimeout=1.0`, `KeepAliveTime=0.2`, `MaxClientRate=1000000`, `SimLatency=0`,
`RelevantTimeout=5.0`, `SpawnPrioritySeconds=1.0`, `ServerTravelPause=4.0`, `NetServerMaxTickRate=120`,
`LanServerMaxTickRate=120`) **plus** `DownloadManagers=Engine.ChannelDownload` — HP2's `UNetDriver` has the 436-era
`DownloadManagers` array (`Engine/Src/UnNetDrv.cpp:137`); do **not** list `IpDrv.HTTPDownload` (no HTTP client on web; HP2
retail ships no IpDrv package at all — `Assets/System/` has no `IpDrv.*`). Also `[IpDrv.TcpNetDriver]` (same values)
because the WebRTC auto-switch starts from it. Keep `[Engine.DemoRecDriver]` retail (`DemoSpectatorClass=HGame.harry`).

**`[Engine.GameReplicationInfo]`** — `ServerName=HP2 Server`, `ShortName=HP2 Server` (retail says `Another UT Server`;
WebRTCDrv's `InitListen` reads it — W3; keep equal to `targets/hp2/target.json network.serverName.stock`).

**`[Engine.GameInfo]`** — retail `bLowGore/bVeryLowGore` + template's `bWorldLog=False`, `bLocalLog=False` (both exist in
HP2 `Engine/Classes/GameInfo.uc:110-111`; the launcher also forces them).

**`[Editor.EditorEngine]`** — keep retail `EditPackages` list (`:327-337`); harmless, and `BUILD_EDITOR` is ON.

**`[HGame.baseConsole] bDebugMode=true`** (retail `:506-507`) — **decide**: retail ships it `true`, which enables HP2's
debug console paths in `HPConsole.uc` (`ConsoleKey` handling gated by `bDebugMode`, `Insert` → `sshot`). For the web build
keep retail parity (`true`) unless the owner wants the debug console hidden.

**Omit**: `[optionsettings]` and the ~30 `[<vendor>.<device-id>]` GPU-quirk sections (`:392-505`, consumed only by
`UnEngineWin.h` hardware detection and `D3DDrv`), every UT99/Windows section listed in 9.1.

### 9.3 `Config/System/web/DefUser.ini` for HP2

Start from HP2's retail `Config/System/DefUser.ini` (246 lines: `[DefaultPlayer] Class=HGame.harry`, `[Engine.Input]`
with HP2's broom/spell/duel `Button b…` aliases, `[Engine.PlayerPawn]`, `[Engine.HUD]`, `[HGame.Harry]`) — **not** from the
template's (Botpack classes, `SoldierSkins`, `ChallengeHUD`, `ChallengeBotInfo`, weapon priorities: all UT99).
Web-specific review items:

| Key | Retail | Web consideration |
|---|---|---|
| `Escape=quit` | `DefUser.ini [Engine.Input]` | `HPConsole.KeyEvent` consumes `IK_Escape` (`Source/HGame/Classes/Menu/HPConsole.uc:776-779`, `MenuBook.EscFromConsole(); return true;`) so the binding normally never fires. If it ever does on web, `quit` → `appRequestExit(0)` → the page navigates back to the launcher (C5). Browsers also deliver Escape when leaving pointer lock. **Verify** in-game; if a stray Escape can reach the binding (e.g. while a cut-scene console state is active), blank it (`Escape=`) in the web copy. |
| `F12`/`EndFullscreen`-style or `Alt`-combos | — | harmless |
| `Insert` (screenshot in `HPConsole`) | — | writes `Shot*.bmp` to `/System` (not persisted; fine) |

`User.ini` is **derived** from it at boot (`GUserIni = appUserDir()+"User.ini"`, `UnMisc.cpp:1933-1935`) → `/System/User.ini`.

### 9.4 `Config/System/web/README.md` for HP2

Replace `README-ut99.md` with an HP2 README (rename the file; the template's content is 14 lines): what the two files are,
that `Game/CMakeLists.txt` preloads them at `@/System/…`, that `appInit( TEXT("Game") … )` derives `Game.ini`/`User.ini`,
the two `Paths=` rules from 9.2 (six entries; `.usa` last), the `default.ini` lowercase-literal note, the "no Asian native
fonts on web" limitation (§6.2 `CreateNativeFont`), and "edit this folder, never the asset tree".

### 9.5 Build-side hooks already in place

`Game/CMakeLists.txt:203-213` preloads `Default.ini` and `DefUser.ini` and puts both in `LINK_DEPENDS` (§2.3/§2.4); a
README is not preloaded. Nothing else to wire. (Optional hardening: turn the `if(EXISTS)` into a `FATAL_ERROR`, §2.3.2.)

### 9.6 Where the derived inis live and how they persist

| File | Engine path (web) | Created by | Persisted by the frontend as |
|---|---|---|---|
| `Game.ini` | `/System/Game.ini` (`appUserDir()=""`, package `Game`) | `appInit` → `FConfigCacheIni::Init(GIni…)` copies `Default.ini` on first boot, `Flush(0)` at the end of `appInit` (`UnMisc.cpp:2065`) and on every `SaveConfig`/`FLUSHCONFIG` | IndexedDB `hp2-user-data` key `/System/Game.ini` — only if `targets/hp2/target.json manifestExclude` lists `System/Game.ini` (that list is what `UserData.watch` harvests, `boot.ts` ~`:745-755`) |
| `User.ini` | `/System/User.ini` | same, from `DefUser.ini` | `/System/User.ini` (same rule) |
| `Game.log` | `/System/Game.log` (`FOutputDeviceFile`, needs `appPackage()`=`Game`) | launcher `Log` device | not persisted (`*.log` excluded) |
| saves | `/Save/Save<n>.usa`, `/Save/Game<n>.usa`, with `SAVESLOT=<k>`: `/Save/Slot<k>/…`, `/Save/Slot<k>/cache/*`, `*_pa.*` persistent-actor files (`UnMisc.cpp:2108-2135`) | engine | `userFiles` globs in the descriptor must cover `Save/*.usa` **and** `Save/Slot*/**` and the `cache/` + `_pa` files (the ut99 descriptor has only `Save/*.usa`) — owned by `ufrontend-target-contract.md` §1 |

⚠ Returning players: a persisted `Game.ini` **shadows** every later change to the preloaded `Default.ini` (the template's
2.12 comment: "a UserData-persisted UnrealTournament.ini shadows it -> clear site storage"). During HP2 bring-up, clear
the `hp2-user-data` store (or the site data) after changing the web ini, or new keys never reach an existing profile.

### 9.7 Checklist

- [ ] `Config/System/web/Default.ini` authored per 9.2 (six `Paths=`, `.usa` last, on-disk case, no `CdPath`, `ViewportManager=NSDLDrv.NSDLClient`, `NetworkDevice=WebRTCDrv.WebRTCNetDriver`, `GameRenderDevice=XOpenGLDrv.XOpenGLRenderDevice`, `AudioDevice` = Null until NOpenALDrv links, `FrameRateCap=120`, `[NSDLDrv.NSDLClient]` incl. `NoFractalAnim`/`ParticleDensity`, `[WebRTCDrv.WebRTCNetDriver]` with `DownloadManagers=Engine.ChannelDownload`, `[ALAudio.ALAudioSubsystem] Channels=32`).
- [ ] `Config/System/web/DefUser.ini` = HP2 retail `DefUser.ini` (review `Escape=quit`).
- [ ] `README-ut99.md` → `README.md` (HP2 text).
- [ ] Source prerequisites the inis depend on: `default.ini` literal (§2.3.3), `appUserDir()` POSIX (§2.3.4), `SavePath` separator (§2.3.4/E9/F2), `appPackage()`/`GPackage`=`Game` (§4.5.1), `-ini:` overrides (§4.6.2).
- [ ] Clear `hp2-user-data` whenever the web ini changes during bring-up (9.6).

## 10. Artifact contract (SDLLaunch.{js,wasm,data}, meta.json, targetId `hp2`)

The artifact is the **only** thing the HP2 repo hands to ufrontend. Its shape is frozen across all five engines; HP2 must
produce it byte-compatible with what `ufrontend/packages/build` (branch `hp2`) validates.

### 10.1 Files (per build tree)

| File | Produced by | Required? | Consumer / check |
|---|---|---|---|
| `build-wasm-perf/SDLLaunch/SDLLaunch.js` | emcc link (`OUTPUT_NAME SDLLaunch SUFFIX .js`, `Game/CMakeLists.txt:225-228`), then rewritten in place by `offscreen-depth24.sh` | **yes** | `ufront-build.mjs:200` dies without it; loaded by the host as a classic `<script>` (`s.src = 'SDLLaunch.js'`) — `loaderStyle` must be `global-module` (no `-sMODULARIZE`, no `-sEXPORT_ES6`) |
| `…/SDLLaunch.wasm` | emcc | **yes** | `:200`; checksum; wasm local-count limit (§11.1) |
| `…/SDLLaunch.data` | emcc `--preload-file` of the two web inis | yes in practice (without it the engine aborts `MisingIni`) | optional to the validator (`checksums.data` only compared when the file exists, `ufront-build.mjs:224`) |
| `…/meta.json` | `emit-artifact-meta.mjs` POST_BUILD (after the depth patch) | **yes** | `ufront-build.mjs:209-227` `validateArtifactMeta`; `publish/engine.mjs:103-150` provenance checks |
| `…/SDLLaunch.js.symbols` | `--emit-symbol-map` (perf preset only) | no | archived to `ufrontend/.ufront/symbols/hp2-<gitSha>.js.symbols`, **never** published (`ufront-build.mjs:300-307`) |
| `build-wasm/SDLLaunch/SDLLaunch.{js,wasm,data}` | the `wasm` (dev, `-O2 -g`, ASSERTIONS=1) preset | optional | shipped renamed as `SDLLaunch-debug.*` (the "debug engine" link, `ufront-build.mjs:309-320`; `build-inputs.mjs:120-124` resolves `<repo>/build-wasm/SDLLaunch`); the host rewrites the stem via `Module.locateFile` (`boot.ts` ~`:1185-1195`) |

Served folder (`REQUIRED_IN_FOLDER`, `packages/build/src/publish/verify.mjs:140-150`): `index.html`, `meta.json`,
`SDLLaunch.js`, `SDLLaunch.wasm`, `ufront-glue.js`, `ufront-cache-worker.js`, `ufront-target.json`, `theme.css`,
`file-manifest.json`. Only the first four come from HP2; `ufront-glue.js` is **mandatory for the engine** because the
worker does `importScripts('ufront-glue.js')` (`vendor/glue/wasmfs-shim.js:136`).

### 10.2 `meta.json` — the exact document HP2 must emit

What HP2's (adapted) emitter should write, with every field's validator:

```json
{
  "targetId": "hp2",
  "engineFamily": "UE1",
  "buildConfig": "wasm-integrated",
  "abiVersion": 1,
  "loaderStyle": "global-module",
  "provenance": { "repo": "HarryPotter2", "gitSha": "<git -C Source/Game/cmake rev-parse --short HEAD>", "builtAt": "2026-…Z" },
  "toolchain": {
    "emscripten": "<emscripten-version.txt>",
    "base": "Harry Potter and the Chamber of Secrets (UE1 build 433), templated on engine-ut99",
    "openal-soft": "10-p/openal-soft-web@ufront/web",
    "xopengl": "local:/home/ion/hp2-ufrontend/XOpenGLDrv",
    "utglr": "local:/home/ion/hp2-ufrontend/utglr-copy",
    "gl4es": "local:/home/ion/hp2-ufrontend/gl4es"
  },
  "buildFlags": ["-sASYNCIFY", "-sFULL_ES2", "-sUSE_SDL=2", "-sALLOW_MEMORY_GROWTH", "-sFORCE_FILESYSTEM", "-sWASMFS",
                 "-pthread", "-sPROXY_TO_PTHREAD", "-sOFFSCREENCANVAS_SUPPORT", "-sOFFSCREENCANVASES_TO_PTHREAD=#canvas",
                 "-sOFFSCREEN_FRAMEBUFFER"],
  "exports": { "symbolContract": 1 },
  "checksums": { "wasm": "sha256-…", "js": "sha256-…", "data": "sha256-…" }
}
```

| Field | Validated by | Rule |
|---|---|---|
| `targetId` | `REQUIRED` only (`glue/src/artifact/meta.js:24`) — **never compared to the requested target** (`publish/engine.mjs:158-160`) | must be `hp2` anyway: human-facing, and CI/telemetry key on it. Emitter fallback `'ut99'` → `'hp2'` (§3.3). |
| `engineFamily` | enum `['UE1','UE2']` (`meta.js:13`) | `UE1` |
| `buildConfig` | enum (`meta.js:14`) | `wasm-integrated` |
| `abiVersion` | `=== glue ABI_VERSION` (1) (`meta.js` + `ufront-build.mjs:225`) | from `vendor/glue/ue_abi.h`; never hand-edit |
| `loaderStyle` | enum + `HOST_SUPPORTED_LOADERS=['global-module']` (`meta.js:15-21`) | `global-module` |
| `checksums.{wasm,js,data}` | re-hashed by `ufront-build.mjs:218-224` and by `engineArtifactCurrent` (`engine.mjs:103-121`) | the emitter must run **after** `offscreen-depth24.sh` (it does, §2.6) |
| `provenance.repo` | `checkEngineIdentity` compares the engine **checkout's basename** with `target.json publish.engine.repo` (`engine.mjs:162-180`) — the meta field itself is informational | `HarryPotter2` (equal to the descriptor's `publish.engine.repo`, `ufrontend-target-contract.md` §1.3) |
| `provenance.gitSha` | `engineArtifactCurrent` (skip test) + `verifyEngineProvenance` (post-build) compare it to the engine HEAD (`engine.mjs:103-150`) | resolved at **build** time by the emitter via `git -C <emitter dir>`; HP2 is a git repo (`main` at `c73e7e7` on 2026-09-26). ⚠ `ufront-publish` refuses a **dirty** engine tree (`engine.mjs:104`) — the analysis reports being written into `docs/porting/analysis/` make the tree dirty; commit them before a publish run. |
| `provenance.builtAt` | must be newer than the publish run start − 60 s (`engine.mjs:138-150`) | automatic |
| `toolchain`, `buildFlags`, `exports` | not validated | keep honest (drop `libxmp`, add Vorbis if fetched, add `-sOFFSCREEN_FRAMEBUFFER`) |

### 10.3 Which preset produces what (and what publish runs)

`ufront-publish` builds engines itself: `PRESETS = { perf: 'wasm-perf', debug: 'wasm' }` (`publish/engine.mjs:17-20`),
configure = `cmake --preset <p>` run in `<repo>/<publish.engine.presetDir>` (HP2: `Source`), build =
`cmake --build <repo>/build-<p> -j N`, with `EMSDK` injected. Artifact dir = `<repo>/build-<p>/<engineArtifactSubpath||SDLLaunch>`
(`engine.mjs:35-38`). HP2 presets already use `binaryDir ${sourceDir}/../build-wasm{,-perf,-debug}`
(`Source/CMakePresets.json:30,47,55`) → `HarryPotter2/build-wasm-perf/SDLLaunch` ✔. `wasm-debug` (`-O0`) is never shipped
(and is the build most likely to break the local-count limit, §11.1).

⚠ HP2's presets pin `FETCHCONTENT_SOURCE_DIR_*` to the local forks, so a publish from this workspace builds the local fork
working trees (whatever is checked out/dirty there), recorded as `local:` in `toolchain`. That is honest; just be aware
that fork edits reach a publish without a fork commit.

### 10.4 Stale-artifact traps (all three already guarded by `LINK_DEPENDS`)

1. web ini edited → `.data` must be regenerated → `LINK_DEPENDS` on both inis (`Game/CMakeLists.txt:211-213`).
2. shim edited → relink → `LINK_DEPENDS` on both `.js` libraries (`:218-220`).
3. emitter edited → `meta.json` regenerated → `LINK_DEPENDS` on the `.mjs` (`:273-274`; the template learned this the hard way, `cc7c1e3`).
4. (not guarded) a **commit with no source change** → nothing relinks → `meta.json` keeps the old `gitSha` → `ufront-publish` reports "artifact is at X, engine HEAD is Y" and tells you to `rm -f <artifact>/SDLLaunch.{js,wasm}` (`ufront-publish.mjs:34-48`). Expected behaviour; the publish tool handles it.

### 10.5 The frontend descriptor side the artifact must agree with

Owned by `ufrontend-target-contract.md` (`targets/hp2/target.json`); listed here because each item is an **engine-side
string** that must match:

| Descriptor field | Must equal (engine side) | Engine source of truth |
|---|---|---|
| `id` = `hp2`, `engineFamily` = `UE1`, `abiVersion` = 1 | `meta.json` `targetId`/`engineFamily`/`abiVersion` | emitter |
| `publish.engine.repo` = `HarryPotter2`, `branch` = `main`, `presetDir` = `Source` | repo dir basename; `Source/CMakePresets.json` | — |
| `videoDrivers[].args` = `-XOPENGLRENDERER`, `-UTGLRRENDERER`, `-SOFTWARERENDERER`, `-NULLRENDERER` | `GRenderSwitches[]` in HP2 `Launch.cpp` (§4.3.1, **no** `-NOPENGLESRENDERER`) | launcher |
| `frameRateCap` `{iniPath:"System/Game.ini", iniToken:"System", section:"Engine.Engine", key:"FrameRateCap", default:120}` | `[Engine.Engine] FrameRateCap` read by the launcher; `System` token → `Game.ini` needs `appInit(TEXT("Game"))` | §4.5.3, §9.2 |
| `audioHrtf` `{iniPath:"System/Game.ini", section:"NOpenALDrv.NOpenALAudioSubsystem", key:"UseHRTF"}` | HP2 NOpenALDrv registers `UseHRTF` (§7.1 A6); omit while audio is Null | driver |
| `audioBuffer` | only once openal-soft is linked (§7.1 A8) | — |
| `manifestExclude` ⊇ `System/Default.ini`, `System/DefUser.ini`, `System/Game.ini`, `System/User.ini`, `*.log`, `*.dll`, `*.exe`, `*.usa` | the preloaded + derived file names | §9.6 |
| `userFiles` ⊇ `Save/*.usa`, `Save/Slot*/**` (+ `cache/`, `*_pa.*`) | HP2 save layout (`UnMisc.cpp:1834-1840, 2002-2036, 2108-2135`) | §9.6 |
| `network.peerPrefix` = `hp2`, `network.defaultServerName` = `HP2 Server` | `WEBRTC_PEER_PREFIX`, the `SName` fallback in `WebRTCDrv.cpp` (W1/W2) | driver |
| `telemetry` log patterns (crash reason, travel URL) | exact engine log lines (`Bringing … up for play`, `appError called`, `[appThrowf] …`, `Critical: …`) → need the stdout mirror (C1) and C2 | Core |

### 10.6 Checklist

- [ ] Emitter: `targetId` fallback `hp2`, `provenance.repo` `HarryPotter2`, `toolchain.base` HP2 text, `buildFlags` += `-sOFFSCREEN_FRAMEBUFFER`, header comment de-UT99'd (§3.3).
- [ ] `Game/CMakeLists.txt`: drop the `libxmp` `ufront_artifact_dep` once NOpenALDrv no longer fetches it; add Vorbis if fetched (§2.7).
- [ ] After the first `cmake --build --preset wasm-perf`: `ls build-wasm-perf/SDLLaunch` shows `SDLLaunch.{js,wasm,data}`, `meta.json`, `SDLLaunch.js.symbols`; `node -e` validate: `targetId=hp2`, `checksums.data` present.
- [ ] `cd ufrontend && yarn build --target hp2` passes the meta + wasm-limit gates (§11.1) — the earliest end-to-end artifact check without a browser.

## 11. Wasm limits, required exports, pthread / proxy-to-pthread / OffscreenCanvas, frontend capability probes

### 11.1 Wasm limits (`ufrontend/packages/build/src/wasm-limits.mjs`)

| Item | Value / behaviour | Source |
|---|---|---|
| Limit | `MAX_WASM_FUNCTION_LOCALS = 50000` (V8 `kV8MaxWasmFunctionLocals`; SpiderMonkey's is higher, so V8 binds) | `wasm-limits.mjs:28` |
| Check | walks the code section, sums each body's local-decl groups, names functions from the `name` section (index fallback for stripped builds) | `localCensus()` `:131-160` |
| Gate | `ufront-build.mjs:232-245`: any function **over** 50000 → build dies ("engine wasm will NOT compile in Chromium … Build that translation unit optimised"); **near** (> 80 % = 40000) → warning per function | |
| Why it exists | a Chromium `CompileError: … local count too large` rejects the **whole module** before any engine output; ut2004's `-O0` debug build had one function at 4,800 locals → 91,016 after `-sASYNCIFY` instrumentation (2.19c) | `wasm-limits.mjs:1-24` |

**Measured on the template (2026-09-26, same emsdk `5.0.7` HP2 will use, `~/emsdk/upstream/emscripten/emscripten-version.txt`)**
— run read-only with the ufrontend checker against `engine-ut99`'s current artifacts:

| Artifact | Functions | Worst function (locals) | over / near |
|---|---|---|---|
| `build-wasm/SDLLaunch/SDLLaunch.wasm` (`-O2 -g`, 60 MB) | 12,130 | `emscripten_webgl1_get_proc_address` 165; `vorbis_decode_packet` 124; `UOpenGLRenderDevice::GetGL1Procs()` 118; `URender::OccludeBsp` 90 | 0 / 0 |
| `build-wasm-perf/SDLLaunch/SDLLaunch.wasm` (`-O3 -g0`, 9.1 MB) | 8,179 | `#6597` 129 | 0 / 0 |

So at `-O2`/`-O3` the margin is ~300×; the limit is only a real risk for the **`wasm-debug` (`-O0`) preset**, which is
never shipped (§10.3) but is what one reaches for when debugging. HP2-specific candidates for a blow-up at `-O0` +
Asyncify (very large single functions): `UEditorEngine::Exec` (`Source/Editor/Src/UnEdSrv.cpp`, linked because
`BUILD_EDITOR` is forced ON), the script compiler (`Source/Editor/Src/UnScrCom.cpp` `CompileStatement`/`CompileExpr`),
HP2's `UGameEngine::Browse`/`LoadMap`, and any HGame native with giant `switch`es. **ACTION:**
- [ ] After the first `wasm-debug` link, run the census (the script used for the table above:
  `node -e "import('/home/ion/hp2-ufrontend/ufrontend/packages/build/src/wasm-limits.mjs').then(m=>{const r=m.localCensus(require('fs').readFileSync(process.argv[1]));console.log(r.slice(0,15))})" build-wasm-debug/SDLLaunch/SDLLaunch.wasm`).
- [ ] If a function is over/near, compile **that translation unit** at `-O1`+ in the debug preset
  (`set_source_files_properties(<file> PROPERTIES COMPILE_OPTIONS -O1)` under `if(HP2_WEB_DEBUG)`), as the ut2004 fix did —
  do not raise the global optimisation level of `wasm-debug`.
- [ ] Never ship `wasm-debug`; the debug engine the frontend offers is `build-wasm` (`-O2 -g`).

### 11.2 Required exports / imports (consolidated from §2.2 and §8)

| Kind | Symbols | Defined by | Failure if missing |
|---|---|---|---|
| `EXPORTED_FUNCTIONS` | `_main`, `_malloc`, `_free`, `_close` | Emscripten/libc | link error / host marshalling fails |
| | `_ue_exec_command` | HP2 `Launch.cpp` (`extern "C" EMSCRIPTEN_KEEPALIVE`) | wasm-ld "undefined symbol" at link (hard error) |
| | `_wasmfs_create_jsimpl_backend`, `_wasmfs_create_file` | Emscripten WasmFS | FetchFS cannot mount → first asset read fails |
| | `_ufront_wasmfs_register`, `_ufront_wasmfs_set_pending` | `vendor/glue/wasmfs-shim.js` | same |
| **not** exported | `_webrtc_deferred_open` | (UE2 only) | exporting it is a hard wasm-ld error |
| `EXPORTED_RUNTIME_METHODS` | `callMain` (host runs main with `noInitialRun: true`, `boot.ts` ~`:1154`, `:356`), `stringToUTF8`, `UTF8ToString`, `HEAPU8` (the host's **threaded-build detector**: `HEAPU8.buffer instanceof SharedArrayBuffer`, `boot.ts:526-530`), `stackSave`, `stackAlloc`, `stackRestore`, `lengthBytesUTF8`, `PThread` | Emscripten | `PThread` missing on a threaded build → `[SHELL] threaded engine but Module.PThread is missing …` and death on the first asset read (`boot.ts:531-537`) |
| JS library functions kept alive | `$ueWorkerBoot` via `DEFAULT_LIBRARY_FUNCS_TO_INCLUDE` | `wasmfs-shim.js` | silently dead-stripped → no worker FS, no `ufrontExec` routing |
| C imports (JS provides) | 19 `webrtc_*` (`ue_abi.h:60-78`) | `webrtc-shim.js` | unresolved import at instantiate (if the driver references them) |
| Async imports | `ASYNCIFY_IMPORTS=['emscripten_sleep','emscripten_set_main_loop','__syscall_openat']` + implicit `__asyncjs__ufront_yield_frame` | link flag | an unwind through a non-listed import = `unreachable` |
| Engine console commands the host sends | `flushconfig` (`UserData.engineFlush`, `boot.ts:762-764`, via `ueExecResult`, 4 s timeout); E2E specs send `open <map>`, `exit` | **HP2 lacks `FLUSHCONFIG`** (`grep FLUSHCONFIG Source/Engine/Src/UnEngine.cpp` → nothing) | the host's persist-now call gets no handler: settings still persist when the engine itself calls `SaveConfig`/`Flush`, but the explicit flush before harvest is a no-op → **port `6525113`** (21 lines at the top of `UEngine::Exec`, §13) |

Verification after link: `wasm-objdump -x -j Export SDLLaunch.wasm` lists the export set; `-j Import` lists the
`webrtc_*` + `__asyncjs__*`; `grep -c ueWorkerBoot SDLLaunch.js` ≥ 1; `grep -c _webrtc_deferred_open SDLLaunch.js` = 0.

### 11.3 Threads: pthread / `PROXY_TO_PTHREAD` / OffscreenCanvas — the requirement chain

```
compile: -pthread (global add_compile_options)      → __EMSCRIPTEN_PTHREADS__ defined → NSDLDrv G3 branches,
                                                       appRequestExit MAIN_THREAD_EM_ASM, XOpenGL resolver
link:    -pthread -sPROXY_TO_PTHREAD                → main() runs on a worker; the page's main thread only hosts JS
         -sOFFSCREENCANVAS_SUPPORT
         "-sOFFSCREENCANVASES_TO_PTHREAD=#canvas"   → #canvas transferred to that worker before main()
         -sOFFSCREEN_FRAMEBUFFER                     → emscripten_webgl_commit_frame is real (not a stub)
         -sPTHREAD_POOL_SIZE=4                       → main + openal-soft mixer + event thread + 1 spare, pre-spawned
runtime: NSDLViewport creates the WebGL ctx itself on the worker (explicitSwapControl + renderViaOffscreenBackBuffer),
         presents with emscripten_webgl_commit_frame() in Unlock; SoftDrv presents with a 2D context putImageData
host:    COOP: same-origin + COEP: require-corp       → crossOriginIsolated → SharedArrayBuffer → shared wasm memory
```

HP2 rules that follow:
- **No new long-lived threads** in HP2 code on web: WasmFS JSImpl hooks only work on the thread that received the host's
  channel (`wasmfs-shim.js` throws "WasmFS hook fired on the worker before the host handed over the sync channel"). HP2's
  `FFileStream` (Win32 `CreateThread`, `Core/Src/UnVcWin32.cpp:665-690,1489`) must be pumped from the engine thread (§7.1 A4).
  Any thread that must exist raises `PTHREAD_POOL_SIZE` (a thread created with an empty pool starts only after the main
  thread yields — i.e. late).
- **No blocking of the page's main thread from the engine**: `webrtc_*` sync proxies run on main; `emscripten_sleep(50)`
  only during a handshake (W5). Never poll a proxied import every frame (`3a62947`).
- **No `emscripten_set_main_loop`** (§4.4.4) and no `SDL_GL_*`/`SDL_CreateRenderer` on the worker (N6/N1).
- **`ue_exec_command` runs from a worker message event** while the main loop is suspended in an Asyncify import: a console
  command must not itself hit an Asyncify import (§8.2).

### 11.4 What the frontend capability probes check, and which build choice each one protects

`ufrontend/packages/frontend/src/shell/capabilities.ts` (430 lines): main-thread probes `:173-224`, worker probes
(`WORKER_PROBE_SOURCE`, run in a real worker like the engine) `:230-273`. The per-target spec is `target.json capabilities`
(`required` blocks the launch with a notice; `recommended` warns). The ut99 descriptor's spec — which HP2 should copy
verbatim because the HP2 link is identical — is:

| Capability id | Probe (file:line) | Engine requirement it protects | HP2 spec |
|---|---|---|---|
| `secureContext` | `isSecureContext` `:174` | OPFS, SAB (both need a secure context) | required |
| `crossOriginIsolated` | `crossOriginIsolated===true` `:175` | shared memory for `-pthread` | required |
| `sharedArrayBuffer` | `:176` | `-pthread` | required |
| `webAssembly` | `:177` | — | required |
| `wasmThreads` | `new WebAssembly.Memory({shared:true})` `:178-186` | `-pthread` | required |
| `wasmFeatures` | validates tiny modules for bulk-memory, sign-extension, sat-float-to-int, BigInt `:150-171` | Emscripten 5.x defaults (+ `-pthread` implies bulk-memory/atomics) | required |
| `workers` | `typeof Worker` | `PROXY_TO_PTHREAD`, cache worker | required |
| `atomicsWait` (worker) | `Atomics.wait` returns `'not-equal'` `:253-254` | FetchFS sync reads block the engine worker | required |
| `offscreenCanvas` | `OffscreenCanvas` + `transferControlToOffscreen` `:192-198` | `OFFSCREENCANVASES_TO_PTHREAD=#canvas` | required |
| `offscreenWebgl` (worker) | `new OffscreenCanvas(1,1).getContext('webgl')` `:243-256` | UTGLR/gl4es (WebGL1) and the minimum for any GL renderer | required |
| `offscreenWebgl2` (worker) | same with `webgl2` `:257` | XOpenGL (default renderer) | recommended (fallback: `-UTGLRRENDERER` / `-SOFTWARERENDERER`) |
| `gpuAcceleration` (worker) | WebGL with `failIfMajorPerformanceCaveat` `:258` | playable frame rate | recommended |
| `opfsSyncAccess` (worker) | `navigator.storage.getDirectory()` + `createSyncAccessHandle` `:259-266` | the package cache (OPFS) | required |
| `indexedDb` | open/delete a probe DB `:199-221` | `hp2-user-data` (inis, saves) | recommended (HP2 note: **saves** live there — consider making it *required* for HP2, since a single-player game without persistent saves is broken; a descriptor-only decision) |
| `webAudio` | `AudioContext` `:222` | openal-soft Web Audio backend | recommended |
| `webrtc` | `RTCPeerConnection` + `WebSocket` `:223` | WebRTCDrv | recommended (or drop from HP2's list if `network` is disabled) |
| `pointerLock` | `requestPointerLock` `:224` | mouse look | recommended |

Gaps worth knowing (not HP2 actions unless they bite): there is **no wasm SIMD probe**, while openal-soft is compiled
`-msimd128 -msse4.1` (`Source/cmake/OpenALSoft.cmake`), so a browser without wasm SIMD fails at compile time rather than at
the capability screen (all current evergreen browsers have SIMD; Safari since 16.4). No probe for `AudioWorklet`
specifically.

Serving requirement (not a probe): the host must send `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp` on every response — `yarn serve` and the Azure config already do for the other
targets; a root-published HP2 site needs the same headers (`ufrontend-target-contract.md` §6/§10). Any ad-hoc test server
(e.g. the smoke in §12) must send them too.

### 11.5 Checklist

- [ ] `-pthread` at compile **and** link; `PROXY_TO_PTHREAD`; `OFFSCREENCANVAS_SUPPORT`; quoted `OFFSCREENCANVASES_TO_PTHREAD=#canvas`; `OFFSCREEN_FRAMEBUFFER`; `PTHREAD_POOL_SIZE=4` — all present in HP2 `Game/CMakeLists.txt` (§2.2) — keep.
- [ ] Export set per 11.2 verified with `wasm-objdump` after the first link.
- [ ] Port `FLUSHCONFIG` (`6525113`).
- [ ] Census the first `wasm-debug` link (11.1).
- [ ] `targets/hp2/target.json capabilities` = ut99's list (decide `indexedDb` required; drop `webrtc` if no network).

## 12. Smoke test (`tools/web-smoke.mjs`) and how to adapt it

HP2's `tools/web-smoke.mjs` is **byte-identical** to the template's (127 lines; `diff` clean). It dates from the squashed
2.7b import (`d5a6bfd`; `git log -- tools/web-smoke.mjs` shows no later commit) — i.e. from **before** 2.13.1 (WasmFS) and
2.13.2 (pthreads + `PROXY_TO_PTHREAD`), and it was never updated for them.

### 12.1 What the template smoke does

| Step | Code (template `tools/web-smoke.mjs`) |
|---|---|
| Args | `DIR = argv[2] \|\| 'build-wasm/SDLLaunch'`, `SECS = argv[3] \|\| 20`, `PORT = 8792` (`:13-15`) |
| Playwright | `createRequire('/home/ion/ufrontend/packages/e2e/')` → `chromium` (`:17-18`) — resolved from the **upstream** ufrontend working copy |
| Host page | writes `__smoke.html` **into the artifact dir** (`:26-36`): `<canvas id=canvas>`, a bare global `Module` with `print`/`printErr` captured into `window.__log`, `onAbort`, `onRuntimeInitialized`, no `arguments`, no `Module.UE` (no FetchFS, no WebRTC) |
| Server | `node:http`, MIME map for html/js/wasm/data, **no COOP/COEP headers** (`:44-56`) |
| Browser | `chromium.launch({headless:true})`, `goto('/')`, wait `SECS` (`:58-63`) |
| Probe | walks `FS.readdir('/')` with `FS.isDir(mode)`, reads the first `*.log` it finds (`:65-90`) |
| Verdict | PASS = `runtimeInitialized && engineRan`, where `engineRan` = a `.log` exists **or** any captured line matches `/(Log\|Init\|Warning\|…\|Package\|Core\|appThrowf\|MissingIni\|Detected)/i` **or** `/appThrowf\|PackageNotFound\|MissingIni\|Package/` (`:103-125`) |
| Intended boundary | "the engine reaches package loading and stops there, BEFORE any WebGL — so this smoke needs no GPU" (`:5-6`) |

### 12.2 Measured: the template smoke is stale (fails on today's template artifact)

Run 2026-09-26 against a **scratch copy** of `engine-ut99/build-wasm/SDLLaunch/SDLLaunch.{js,wasm,data}` (built 2026-09-24 at
`7c31a53`; the engine tree was not touched), with Playwright resolved from the hp2 ufrontend clone:

```
runtimeInitialized: false
[ERR] still waiting on run dependencies: … dependency: loading-workers
[PAGEERROR] Failed to execute 'postMessage' on 'Worker': SharedArrayBuffer transfer requires self.crossOriginIsolated.
[ABORT] Assertion failed: native function `_wasmfs_readdir_start` called before runtime initialization
[smoke] FAIL — runtimeInitialized=false engineExecuted=true
```

Two defects: (1) the threaded build needs **cross-origin isolation** — without `COOP: same-origin` + `COEP: require-corp` the
pthread pool cannot be started; (2) the verdict logic is loose: `engineExecuted=true` was reported although `main()` never
ran (the regex matched `Init` inside "initialization" of the Emscripten assertion text). Also (3) `FS.isDir` does not
exist under `-sWASMFS` (`TypeError: FS.isDir is not a function`), so the FS walk stops at the first entry.

With only the two headers added (one-line change), the same artifact passes and shows the real boundary:

```
[OUT] Init: Name subsystem initialized
[OUT] Init: Version: 400
[OUT] Init: Base directory: /System/
[OUT] Log: Bound to Editor.so … Bound to Render.so          (the UNREAL_STATIC package table)
[OUT] Init: Object subsystem initialized
[OUT] Log: Cd Path: D:/                                      (← the template ini's CdPath=D:, see §9.2)
[OUT] Init: Detected: Emscripten
[OUT] Log: ufront 2.9: default render device XOpenGLDrv.XOpenGLRenderDevice (preference order)
[OUT] Init: Unreal engine initialized
[OUT] Init: NSDLClient: 1280x720, Brightness=0.50, Decals=1, …
[OUT] Log: LoadMap: Entry
[OUT] Critical: appError called:
[OUT] Critical: <?int?Engine.Errors.FailedBrowse?>          (no Entry.unr: no FetchFS — the expected boundary, §5.3 E8)
[OUT] Log: appRequestExit(1)
[smoke] PASS — runtimeInitialized=true engineExecuted=true
```

This is the right tier-1 bar for HP2 too: toolchain + threaded runtime + preloaded inis + Core/Engine init + launcher
render selection + NSDLClient config, **no GPU, no assets, no frontend**.

### 12.3 Adapted `tools/web-smoke.mjs` for HP2 (the edits, in order)

| # | Change | Why |
|---|---|---|
| 1 | `createRequire('/home/ion/hp2-ufrontend/ufrontend/packages/e2e/')` (or `process.env.UFRONT_E2E_DIR`) | the upstream `/home/ion/ufrontend` is READ ONLY for this port and may be on another branch; the hp2 clone has `node_modules/playwright` + `~/.cache/ms-playwright/chromium-1223` |
| 2 | send `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp` on **every** response | 12.2 defect 1 |
| 3 | write the host page to a **temp dir** and serve the artifact read-only from `DIR` (or serve `/__smoke.html` from memory) | do not leave `__smoke.html` inside `build-wasm/SDLLaunch` (harmless for `ufront-build`, which copies only the named files, but untidy); also lets the smoke run on a read-only artifact |
| 4 | FS walk: replace `FS.isDir(mode)` with `(mode & 0o170000) === 0o040000` | 12.2 defect 3 (WasmFS) |
| 5 | Verdict (strict): PASS iff `runtimeInitialized` **and** a line `Init: Name subsystem initialized` (`Core/Src/UnName.cpp:157`) **and** `Init: Object subsystem initialized` (`Core/Src/UnObj.cpp:1350`) **and** one of the expected boundaries: `LoadMap: Entry` (`Engine/Src/UnGame.cpp:842`) followed by `FailedBrowse`, or a named `[appThrowf] …` / `Critical: …` **after** `Unreal engine initialized` (`Engine/Src/UnEngine.cpp:123`); FAIL on `[PAGEERROR]`, on an abort **before** `Object subsystem initialized`, on `MisingIni`/`MissingIni`, and on `Localization: No localization` for `Core.General.*` **plus** a garbage language (see 7) | 12.2 defect 2; make each expected HP2 prerequisite observable |
| 6 | HP2 asserts (web-layer prerequisites made visible): `Init: Base directory: /System/` (chdir before `appInit`, §4.5.2); `Log: ufront 2.9: default render device XOpenGLDrv.XOpenGLRenderDevice` **or** `using the persisted hardware render device XOpenGLDrv…` (web ini `GameRenderDevice`, §9.2); `Init: NSDLClient: 1280x720 …` (client config reached); **no** `Log: Cd Path:` line (no `CdPath` in the HP2 web ini) | each line proves one §4/§9 action landed |
| 7 | Language check: after boot, `ueExec`-free check via the log — HP2's "gk 10/15/01" block (§2.3.3) must not produce a garbage language; assert no line matches `/<\?[^?]*\?/` except for known `Engine.Errors.FailedBrowse` at the boundary (with `Language=int` and `Core.int` absent from the FS this is noisy — so the precise check is: `Localization: No localization: … (int)` shows `(int)`, not a garbage token) | catches the `default.ini` lowercase-literal bug at tier 1 |
| 8 | Optional `-SOFTWARERENDERER`/`-NULLRENDERER` via `Module.arguments` (argv[4]) | the tier-1 page has no FetchFS so no level ever loads; renderer choice only shows in the selection log line |
| 9 | Exit code 0/1 + print the first 200 captured lines (not 30) on FAIL | debugging |

Expected HP2 tier-1 output shape (to be confirmed on the first link): `Name subsystem initialized` → `Base directory: /System/`
→ `Bound to …` lines for HP2's static set (Core, Engine, Editor, Render, Fire, SoftDrv, NullDrv, SoundDrv, XOpenGLDrv,
OpenGLDrv, NSDLDrv, IpDrv, WebRTCDrv, [NOpenALDrv]; HGame has no C++ package — retail ships no `HGame.dll`, its natives live in Engine) → `Object subsystem initialized` →
render selection → `Unreal engine initialized` → `LoadMap: Entry` → `FailedBrowse` → `appRequestExit(1)`.
**Must not** contain `Bound to UWeb.so` or `Bound to NOpenGLESDrv.so` (§14).

### 12.4 Tier 2 — the real boot (frontend + FetchFS + GPU), and tier 3 — manual

| Tier | What | How | Pass bar |
|---|---|---|---|
| 1 | bare loader smoke (12.3) | `node tools/web-smoke.mjs build-wasm/SDLLaunch 20` (also run on `build-wasm-perf/SDLLaunch`) | 12.3 row 5-6 |
| 1b | artifact gates | `cd /home/ion/hp2-ufrontend/ufrontend && yarn build --target hp2` | `✓ artifact meta.json valid`, no wasm-limit error (§10.2, §11.1) |
| 2 | engine boots to the menu in the real host | `packages/e2e/hp2-wasm-boot.spec.js` + `lib/engines/hp2.profile.js` (spec designed in `ufrontend-target-contract.md` §5.3; `yarn e2e --target hp2 hp2-wasm-boot.spec.js`) | milestones `Name subsystem initialized` → `Game engine initialized` (`UnGame.cpp:242`) → `Created WebGL 2.0 context on the worker` (N8) → `Bringing … up for play` (`UnGame.cpp:1272`, the host's per-level flush tap) → `Possessed PlayerPawn`; zero `Aborted`/`RuntimeError`/`[appThrowf]` |
| 2b | SoftDrv GPU-free | same spec, `#render-device` = Software | menu reached; exercises S1 (device-pixel canvas vs SoftDrv screen bound) |
| 2c | persistence | `registerConfigPersistenceTests`, `registerSaveGameTests` | `Game.ini`/`User.ini` round-trip via `hp2-user-data`; a save survives reload (§9.6, E9, F1) — needs `FLUSHCONFIG` (§11.2) |
| 3 | manual | `yarn serve`, Chrome, `?map=PrivetDr` | play the first level; watch the console for the §5 named errors |

⚠ Consistency item with `ufrontend-target-contract.md` §5.3: its `hp2.profile.js` sets `logPath: '/System/this.program.log'`
(copied from ut99, where `appPackage()` = `this.program`). If HP2's launcher sets `GPackage`/`appPackage()` to `Game` as
§4.5.1 recommends, the log is **`/System/Game.log`** — the profile must follow whichever the launcher does. Decide once
(§16).

### 12.5 Checklist

- [ ] Apply 12.3 rows 1-5 (minimum: headers + Playwright path + WasmFS dir test + strict verdict) to `HarryPotter2/tools/web-smoke.mjs`.
- [ ] Add the HP2 asserts (row 6-7) as the corresponding §4/§9 actions land.
- [ ] Record the first real HP2 tier-1 output in `BUILD.md` as the reference boundary.
- [ ] Tier 2 spec per `ufrontend-target-contract.md` §5.3, with `logPath` agreed (12.4).

## 13. Web-era commits (2.7b..2.79) and anything undocumented

`git -C /home/ion/engines/engine-ut99 log --oneline` has **108** commits; everything before `d5a6bfd` ("Web + native port of
UT99 on ut99dc (squashed)") is the Dreamcast base, so the web era is `d5a6bfd` plus the 90 commits `d5a6bfd..7c31a53` (ufront steps
2.7b … 2.79). The squash hides 2.7b-2.12 history; the individual commits start at `cf3dfcb`. Each web-relevant commit is
mapped below to the section of this report that carries it, and to an HP2 action. **Bold** rows are the ones *not* covered
by §1-§12 (i.e. what this pass caught).

### 13.1 Web architecture and build

| Commit | Subject (short) | Covered in | HP2 action |
|---|---|---|---|
| `d5a6bfd` | squashed 2.7b-2.12 web+native port | §1-§8 | the baseline everything else builds on |
| `cf3dfcb` | fetch XOpenGLDrv + UTGLR via FetchContent | §1.4 | done (HP2 fetches the `hp2` fork branches) |
| `1102437` | libxmp's `BUILD_STATIC` poisoned native reconfigure | §7.1 A3 | moot once libxmp is removed; keep the save/restore idiom for any new FetchContent dep (Vorbis/Ogg) |
| `e762cf4` | boot config from tracked `Config/System/web` | §2.3, §9 | author the HP2 set (§9) |
| `582f8b4`/`6927d26` | 2.13.1 WasmFS + JSImpl backend | §2.2.10-12, §8 | link flags present; no engine code |
| `53cbaf1`/`3ee8dc8` | 2.13.2 engine on a worker (pthreads + "hatch (c)" direct WebGL) | §2.2.15-16, §6 N8-N16, §11.3 | NSDLDrv G3 branches must survive the 433 adaptation |
| `cc7c1e3` | descriptor names the real flags; emitter in `LINK_DEPENDS` | §2.4.3, §3.3 | done; add `-sOFFSCREEN_FRAMEBUFFER` to `buildFlags` |
| `1577859` | shim forwards the bootstrap message verbatim | §8 | vendored shim identical — nothing |
| `901db66` | `webrtc_send/recv` unproxied (shared rings) | §8.1 | nothing (shim) |
| `a4500ee`, `520e08f`, `c64a2ca`, `88184e5`, `d240d7b`, `65fd0d8` | pacing: unclamped yield, `-YIELDMODE` added then deleted, present-yield fix, `-PRESENTSYNC` deleted | §4.4.4 | port the **final** `Launch.cpp` loop only (timer yield via `ufront_yield_frame`, `emscripten_sleep` when budget remains); none of the reverted switches |
| `2074fe0` | `emscripten_webgl_commit_frame` was a stub → `-sOFFSCREEN_FRAMEBUFFER` | §2.2.17, N14 | present |
| `4356c7b` | 24-bit depth for the emulated default framebuffer (`offscreen-depth24.sh`) | §3.4 | present |
| `c500e7c` | provenance stamped at build time | §3.3 | present |
| `8c61ef5` | symbol map from the shipping link | §2.2.22 | present |
| `90ae806`, `371c64b` | `meta.json` deps as `repo@branch`; openal-soft from `10-p/openal-soft-web` | §3.1-§3.2 | present |
| `8384371` | openal-soft replaces Emscripten's OpenAL; HRTF off by default | §7.1 | pattern for HP2's NOpenALDrv |
| `2c36918` | ship our own openal-soft beside the native Linux binary | §1.1.5 | native only; present (`HP2_BUNDLED_OPENAL`) |
| `7d3ed39` | Editor is a runtime dependency — build it natively too | §1.1.3 | keep `BUILD_EDITOR` ON; verify HP2 content imports `Editor.*` |
| `6079353` | ignore `build-wasm-debug/` | — | HP2 `.gitignore` has `/build-*/` — covered |
| **`e7f442d`, `a930e9b`, `0d623ca`, `6b3704b`, `cf85daa`** | **CI: linux/windows/web jobs, artifact naming by `ENGINE_VERSION`, 7-day retention, rolling `latest`** | — | **HP2 has no `.github/workflows/`.** If CI is wanted, copy the template's `build.yml` web job with three HP2 changes: (1) `setup-emsdk` `version: 5.0.7` (same as local); (2) keep the **`embuilder build sdl2`** step — a fresh emsdk has an empty ports cache and the first compile fails with `'SDL2/SDL.h' file not found` (template `build.yml:218-223`; undocumented in BUILD.md); (3) HP2's `wasm` preset hard-codes `FETCHCONTENT_SOURCE_DIR_*`/`GL4ES_SOURCE_DIR` to sibling checkouts that do not exist on a runner → add a CI preset (or `-D…=` overrides to empty) that fetches `10-p/XOpenGLDrv@hp2-v433-compat`, `10-p/utglr-copy@hp2`, the chosen gl4es ref. Artifact name `hp2-433-web`; `retention-days: 7`; the provenance step (`meta.json gitSha` prefix of `$GITHUB_SHA`). |

### 13.2 Launcher / config

| Commit | Subject | Covered | HP2 action |
|---|---|---|---|
| `e2e724b` | world ran fast above the tick floor: floor 0.005→0.001 + frame cap on web too | §4.4.3, §5.4 X2 | port (HP2 `UnLevTic.cpp:960` has `Clamp(DeltaSeconds,0.005f,0.10f)`) |
| `35fb897` | frame-rate cap lives in the ini only (`-FRAMERATECAP=` removed) | §4.4.3, §9.2 | `FrameRateCap=120` in the web ini; no switch |
| `2c25d55` | implement `-ini:<File>:[<Section>]:<Key>=<Value>` | §4.6.2, C8 | port `appApplyIniOverrides` |
| `1d3dfa5` | three defects: (1) **`appUnwindf` latched `GIsCriticalError` forever**, (2) `appIsGlobalOptionToken`, (3) absolute UWindow cursor (`bWindowsMouseAvailable`) | (2) §4.6.1/E2, (3) N21 | **(1) is new here**: every `unguard` calls `appUnwindf`, which set `GIsCriticalError=1`; after the first *caught* load failure `UObject::~UObject` skips cleanup and `FOutputDeviceFile::Serialize` (`if( !GIsCriticalError \|\| Entry )`) **stops logging**. On web, catching is off, so the latch matters only natively — but native is where HP2 is debugged first. HP2 has the latch: `appUnwindf` sets `GIsCriticalError = 1;` at `Core/Src/UnMisc.cpp:2291` — port the fix (owned by the core report; listed because a gagged log hides every web-relevant failure on the native reference build). |
| **`6525113`** | **`FLUSHCONFIG` console command (2.28)** | §11.2 | **port**: the host calls `ueExecResult('flushconfig', 4000)` before harvesting `Game.ini`/`User.ini` (`boot.ts:762-764`); HP2's `UEngine::Exec` has no handler |
| `3703f35`, `5e4d8cf` | NSDLClient compiled defaults; register via `StaticConstructor` not the v200 hook | N19, N20 | keep; add HP2 `NoFractalAnim`/`ParticleDensity` |
| `a1e810b` | complete the web render/audio/client option sets | §9.1 | method: derive every key from registrations |
| `7cbd017` | `ConfiguredInternetSpeed=1000000` on web | §9.1 | only with multiplayer |
| `6612d2b` | silence UT99's first-run multiplayer prompts (`[UMenu.UMenuNetworkClientWindow] bShownWindow`, `[UBrowser.UBrowserIRCSetupClient] bHasReadWarning`) | — | **do not copy** (UT99 UMenu/UBrowser; §14) |
| `67976cb`, `52641f8`, `1fb5f26`, `144954c` | AutoFOV + AutoFOVUI (engine-level Hor+), switches removed | §9.1 | optional feature port, not a web requirement; HP2 has no `HudScale` HUD model — AutoFOVUI would not apply as-is |
| `29a71a7`, `d31fdfd` | first-person weapon kept at 4:3 on wide screens; viewmodel placement published for tests | — | UT99 weapon/viewmodel — **do not copy** (HP2 is third-person; §14) |

### 13.3 Core / Engine fixes found by the web build

| Commit | Subject | Covered | HP2 action |
|---|---|---|---|
| `7164de7` | `Paths=` per-entry case-insensitive resolve | C9 | port (HP2 has the old two-pass version, `UnMisc.cpp:1438-1590`) |
| `ec74328` | GC reachability walk iterative; `always_inline` global new/delete | X3, X4 | port |
| `a680288`, `bb90b47` | no value-init in `InternalConstructor`; native layout guard | X6 | port (core report) |
| `0f620a2` | pad `ABrush`'s trailing `bColored` (multi-keyframe movers) | — | **v400 layout fix — do NOT copy the pad**; HP2 433's `EngineClasses.h` has its own layout. Run the layout guard (`bb90b47`) and a per-*property* offset check on HP2 (the commit shows the size guard alone misses this shape) — core report |
| `c506ba7` | **native bools: restore the bit mask as well as the offset in `UStruct::Link`** | — | **port** (HP2 `Core/Src/UnClass.cpp` lacks it; `grep BitMask` shows only the Epic uses). Directly affects the web: HP2 `UClient`'s adjacent native `BITFIELD`s (`CaptureMouse` … `NoFractalAnim`, `Engine/Inc/UnCamera.h:377-383`) and the NSDLClient/NOpenALDrv compiled `= 1` defaults — an ini `False` would clear the wrong bit |
| `1f744b8` | `FString::Mid` signed overflow | X5 | port |
| `1546db7` | create the whole save path; name the failing file | E9, C3 | port (HP2's two-level `Save/Slot<n>/`) |
| `5674fc7` | savegame load on Unix: `PATH_SEPARATOR` vs FURL syntax | E10 | verify/port for HP2's `?load` URL (`UnGame.cpp:719`) |
| `63595e5` | `ServerTickClient` C++17 sequencing (`PriorityActors[ConsiderCount] = … + ConsiderCount++`) | — | **present in HP2 three times**: `Engine/Src/UnLevTic.cpp:655, 686, 703` (`PriorityActors[ConsiderCount] = PriorityList + ConsiderCount++;`). Only reached by a listen server's `ServerTickClient` (so inert for single-player), but it is a live wild-pointer read the moment HP2 hosts over WebRTC → split all three (`PriorityActors[ConsiderCount] = PriorityList + ConsiderCount; ConsiderCount++;`) |
| `a860dca` | remove the Dreamcast actor-constructor undo | — | n/a (HP2 is not ut99dc-derived) |
| `a57d578` | 2.79o: another package version than the server's fails the join instead of aborting | L5, E5 | only with multiplayer ("the 436-era join checks") |
| `3ff2ed5`, `7c31a53` | log the `Received` line; comment | E11 | only with multiplayer |
| `8ee7cbd` | pad `AInternetLink` for `ATcpLink` | — | **do not copy** (IpDrv script layout; HP2 ships no `IpDrv.u`) |
| `d20f4f6`, `5c6d021` | IpDrv: `INVALID_SOCKET`, non-blocking connect on BSD sockets | — | native IpDrv only; take with the IpDrv merge |
| `4e78c02` | UCC static build links the headless package set whole-archive | — | native/UCC report |

### 13.4 Drivers

| Commit(s) | Area | Covered | HP2 action |
|---|---|---|---|
| `b548e28` | NSDL device-pixel backing store | N3/N11/N18 | keep |
| `e8198af`, `b40994b` | SoftDrv FogAvg off-by-one; volumetric fog + filtering in the C path | S5 | port by analogy |
| `b35069a`, `d118e23`, `a2f0ec7` | NOpenALDrv EFX at run time; Galaxy reverb added then removed | §7.1 A1 | pattern only (EFX `AL_EFFECT_EAXREVERB`, resolved at run time) |
| `9b65027`, `af15c15`, `9ed2d8b` | NOpenGLESDrv | — | **do not copy** (no NOpenGLESDrv in HP2) |
| `ff8aa65`, `8af0d15`, `d6abe98`, `b97c9f1`, `3a62947`, `6c52347`, `cbf0e4e`, `14b7bc8`, `33f70ec`, `a41f4e6`, `b8ca13f`, `629992d` | WebRTC transport + synthetic LAN/server browser fixes | §7.2 | HP2's `WebRTCDrv/` is already the post-fix template copy; `ff8aa65` also added `Source/UWeb/CMakeLists.txt` and a `Launch.cpp` hunk for the server browser — **UWeb part: do not copy** |

### 13.5 Undocumented gotchas surfaced by this pass (not in BUILD.md / CLAUDE.md of the template)

1. **The template's `tools/web-smoke.mjs` does not work on a threaded build** (no COOP/COEP; measured, §12.2).
2. **CI needs `embuilder build sdl2`** before the first configure on a fresh emsdk (only in `build.yml:218-223`).
3. **`CdPath=D:` in the template web ini** makes `appInit` log `Cd Path: D:/` and doubles every `appFindPackageFile` miss (measured in the §12.2 run) — harmless for UT99 because hits come first, but pure overhead; do not carry it to HP2.
4. The template emitter's `buildFlags` omits `-sOFFSCREEN_FRAMEBUFFER` (descriptive drift since `2074fe0`).
5. The template's own `meta.json` records `gl4es: ptitSeb/gl4es@master`, while HP2 defaults to `10-p/gl4es@fix/cubemap-bind-active-unit` and clones `gl4es@hp2` (§1.4.2) — three different gl4es refs in play.

## 14. UT99-specific things that must NOT be copied

Scan of HP2's template-copied web-layer files for UT99 residue (`grep -i 'botpack|UnrealTournament|ut99|UWeb|IpServer|UBrowser|NOpenGLES|umx|libxmp|Galaxy|UT Server|UT_HAS|UMenu'`):
`WebRTCDrv/WebRTCDrv.cpp` 35 hits, `Game/Src/Launch.cpp` 27, `NOpenALDrv/CMakeLists.txt` 24, `WebRTCLan.cpp` 10,
`NSDLViewport.cpp` 6, `emit-artifact-meta.mjs` 5, `Game/CMakeLists.txt` 5, plus `tools/dev/*`, `tools/web-smoke.mjs`,
`Config/System/web/README-ut99.md`. Most hits are comments; the table lists the ones that are **behaviour** or **data**.

### 14.1 Do NOT copy / remove from the HP2 copy

| # | UT99-specific item | Where (template → HP2 copy) | Why it is UT99-only | HP2 action |
|---|---|---|---|---|
| U1 | **Botpack** (game classes, bots, `Botpack.TMale1`, `DeathMatchPlus`, `ChallengeHUD`, `ChallengeBotInfo`, weapon priorities) | template web `Default.ini`/`DefUser.ini` sections; `targets/ut99` `defaults.game` | UT99 game code | author HP2 inis from HP2 retail (§9); nothing in C++ |
| U2 | **Editor for bot skins** — the 2.39 essay "Editor is a runtime dependency" (`Source/CMakeLists.txt:24-38`) | HP2 `Source/CMakeLists.txt:35,65` keeps `BUILD_EDITOR` ON | the *reason* is UT99's skin/bot code touching `Editor.*`; for HP2 the need is unproven | keep ON (cheap) but **re-justify for HP2**: `strings Assets/Maps/*.unr Assets/System/*.u \| grep -c 'Editor\.'`; if zero, HP2 may drop Editor from the web link (saves wasm size and the §11.1 `-O0` risk from `UnEdSrv.cpp`/`UnScrCom.cpp`). Decision + evidence into `BUILD.md`. |
| U3 | **UWeb** (`INSTALL_TARGETS … UWeb`, `Source/UWeb/`, `ff8aa65`) | template `Source/CMakeLists.txt:476` — **not** in HP2 ✔ | UT99 ships `UWeb.u`; HP2 does not | keep it out |
| U4 | **IpServer / UdpServerUplink `DoUplink=False`** | `Launch.cpp:525` (HP2 copy, verbatim) | writes a section for a package HP2 does not have | delete the line; keep `bWorldLog`/`bLocalLog` (HP2's `GameInfo.uc:110-111` has them) |
| U5 | **`ServerActors`/`ServerPackages` lists** (UdpBeacon, UdpServerQuery, 3× uplinks, WebServer; Soldier/Commando/… skins, Botpack) | template web ini; HP2 retail ini `:69-74` | UT99 net/admin actors | omit from the HP2 web ini; keep the launcher's generic `ServerActors.Empty()` (§4.3.7) as defence |
| U6 | **NOpenGLESDrv** (`-NOPENGLESRENDERER` switch `Launch.cpp:277`, preference entry `:293`, the `[NOpenGLESDrv…]` ini section, NSDLViewport comments `:319`) | HP2 copy | HP2 decided not to build it | remove switch + preference entry (§4.3.1-4.3.2); no ini section |
| U7 | **libxmp / `.umx` tracker music, Galaxy reverb** | `NOpenALDrv/CMakeLists.txt:14-47`, `NOpenALDrv.cpp`, `META_DEPS libxmp` | HP2 music is 111 × Ogg Vorbis streamed by name | new HP2 NOpenALDrv from `ALAudio` (§7.1 A3-A5); drop the libxmp dep line (§2.7) |
| U8 | **`[PackageRemap] UnrealShare=UnrealI`** | template web ini | UT99/Unreal content compatibility | omit |
| U9 | **`CdPath=D:`** | template web ini `[Engine.Engine]` | retail UT99 CD path; on web it only adds a second failing search pass (§12.2 run printed `Cd Path: D:/`) | omit |
| U10 | **UBrowser / UMenu / UTMenu / UTServerAdmin / UBrowserUT… sections**, the 2.50 "don't nag" flags (`6612d2b`) | template web ini | UT99 menus and server browser | omit |
| U11 | **WebRTCLan synthetic LAN answering UT99's `UBrowser` REPORTQUERY / `\info\` pings** (`WebRTCLan.cpp:18-21,172-178,320-335,409`) | HP2 copy | UT99's LAN browser protocol; HP2 has no server browser | keep compiled (IpDrv's `UdpLink.cpp` calls it; removing it means editing IpDrv too) but do not invest; `BeaconProduct()` falls back to `[IpDrv.UdpBeacon]` → omit both sections, the function copes with empty |
| U12 | **`WEBRTC_PEER_PREFIX "ut99"`, `SName = "UT Server"`** | `WebRTCDrv.cpp:36,429` | UT99 identity strings | → `"hp2"`, `"HP2 Server"` (W1/W2) — must match `targets/hp2/target.json network` |
| U13 | **the "436 join checks"** — 2.79o package-version mismatch fails the join (`a57d578`: `GetPackageLinker` GUID branch + `LoadMap` NULL-linker bail-out), the `Received` log line (`3ff2ed5`) and the `?connect` host promotion (2.7d) | template `UnObj.cpp:2116-2129`, `UnGame.cpp:509-520,734-750`, `UnChan.cpp:1322-1326` | multiplayer joins against UT99/436 servers with mismatched mods | **only if HP2 exposes WebRTC multiplayer** (L5, E4, E5, E11). HP2 is single-player: skip them unless `network` is enabled in the descriptor. (The *single-player* loader sites L1-L4, L6-L8, L10, E1, E3, E6, E7 are **not** in this category — HP2 needs them.) |
| U14 | **`UT_HAS_NOPENALDRV` macro** | HP2 `Launch.cpp` tests it, HP2 CMake defines `HP2_HAS_NOPENALDRV` | rename mismatch = dead code | rename one side (§2.1.3) |
| U15 | **`appInit( TEXT("UnrealTournament") … )`, `THIS_PACKAGE`** | `Launch.cpp:883,152` | UT99 ini/package naming | `TEXT("Game")`, define `GPackage` (§4.5.1, §4.5.3) |
| U16 | **AutoFOV / AutoFOVUI (`[ufront.AutoFOV]`), 4:3 viewmodel (`29a71a7`), viewmodel placement publishing (`d31fdfd`)** | template Engine/Render | UT99 first-person weapon + `HudScale` HUD | not a web requirement; skip (HP2 is third-person; its HUD is HGame UWindow/canvas code) |
| U17 | **`ConfiguredInternetSpeed=1000000`, `[WebRTCDrv.WebRTCNetDriver]` 120-tick values** | template web ini | tuned for UT99 DM online play | harmless; keep the WebRTC section (the driver needs *a* section), netspeed only with multiplayer |
| U18 | **`ABrush` `bColored` pad (`0f620a2`), `AInternetLink` pad (`8ee7cbd`)** | template `EngineClasses.h`, IpDrv | v400/v436 script layouts | never copy layout pads across engine versions; measure HP2's with the layout guard |
| U19 | **ut99 test/profile data** — `logPath '/System/this.program.log'`, DM test maps, `registerAutoFovTests`, `registerMoverKeyframeTests`, net specs | `ufrontend/packages/e2e` | UT99 | HP2 profile per `ufrontend-target-contract.md` §5.3 (with the `logPath` decision, §12.4) |
| U20 | **`tools/dev/mkrun-ut99.sh`, `shot.sh`, `run-template.sh`** | HP2 `tools/dev/` (copied) | UT99 run helpers (paths, maps) | adapt or delete (not web-layer; native run helpers) |
| U21 | **`README-ut99.md`** in `Config/System/web/` | HP2 | UT99 text | replace (§9.4) |
| U22 | **Dreamcast** (`BUILD_DCUTIL/PVRDRV/AICADRV`, `a860dca`) | template CMake/Core | ut99dc base | already absent in HP2 ✔ |

### 14.2 Looks UT99-specific but IS needed by HP2 (do not strip by mistake)

| Item | Why HP2 needs it |
|---|---|
| `IpDrv` (built + linked on web) | WebRTCDrv's auto-switch and `UNetDriver` plumbing start from `IpDrv.TcpNetDriver`; costs little (§1.4.9) |
| `WebRTCDrv` + the `webrtc_*` ABI | the `NetworkDevice` override needs a class; `LOAD_NoFail` would abort without it (W9) |
| `GWebRTCEngine`, `ue_exec_command` | ABI entry for **every** host command (incl. `flushconfig`), not just networking |
| `bWorldLog/bLocalLog=False` | HP2's `GameInfo.uc` has them; avoids `appCreateProc` |
| `ServerActors.Empty()` in the launcher | generic safety — HP2's retail ini lists six UT99 actors |
| `FrameRateCap` + tick-floor fix | HP2's `ULevel::Tick` has the same 0.005 floor |
| `SoftDrv` web fixes (screen bound, SoftFB, MMX gating) | HP2's own SoftDrv has the same static tables, smaller (§7.3 S1) |
| the `vendor/glue` shims verbatim | ABI 1 is shared by all targets |

## 15. Numbered replication checklist (short form, build order)

Build order: each phase ends at a verifiable milestone. `§` = where the detail and file:line live. "HP2 file" is the file
to edit in `/home/ion/hp2-ufrontend/HarryPotter2` unless stated.

### Phase W0 — workspace and toolchain (before the first web configure)

1. `mv` the three forks from `/home/ion/hp2-ufrontend/ufrontend/{XOpenGLDrv,utglr-copy,gl4es}` to `/home/ion/hp2-ufrontend/` (the presets resolve `${sourceDir}/../../X`); never `git add` them to the ufrontend clone. (§1.4.2)
2. Reconcile gl4es: clone branch (`hp2`) vs `GL4ES_GIT_TAG` default (`fix/cubemap-bind-active-unit`, `Source/CMakeLists.txt:313`); record in `BUILD.md`. (§1.4.2, §1.4.4)
3. `source ~/emsdk/emsdk_env.sh` (emsdk **5.0.7**, same as the template artifacts and CI). (§1.3.2, §11.1)
4. On a fresh emsdk: `embuilder build sdl2` before the first configure. (§13.1)
5. Restore the template's 2.54 comment on the duplicate `-O` / `NDEBUG` provenance in `Source/CMakeLists.txt` near `:194`. (§1.3.1)

### Phase W1 — link succeeds (configure + compile + link of `SDLLaunch`)

6. Author `Config/System/web/Default.ini` (§9.2): six `Paths=` in retail order, `../Save/*.usa` **last**; `LocalMap=startup.unr`; no `CdPath`; `ViewportManager=NSDLDrv.NSDLClient`; `NetworkDevice=WebRTCDrv.WebRTCNetDriver`; `GameRenderDevice=XOpenGLDrv.XOpenGLRenderDevice`; `WindowedRenderDevice` and `RenderDevice` = `SoftDrv.SoftwareRenderDevice`; `AudioDevice=SoundDrv.NullAudioSubsystem` (until NOpenALDrv links); `FrameRateCap=120`; `[NSDLDrv.NSDLClient]` incl. `NoFractalAnim`/`ParticleDensity`; `[XOpenGLDrv…]`/`[OpenGLDrv…]`/`[NullDrv…]`/HP2 `[SoftDrv…]`; `[WebRTCDrv.WebRTCNetDriver]` + `DownloadManagers=Engine.ChannelDownload`; `[IpDrv.TcpNetDriver]`; `[ALAudio.ALAudioSubsystem] Channels=32`; `[Engine.GameReplicationInfo] ServerName=HP2 Server`; no UT99 sections (§14).
7. Author `Config/System/web/DefUser.ini` from HP2 retail `DefUser.ini`; review `Escape=quit`. (§9.3)
8. Replace `Config/System/web/README-ut99.md` with an HP2 `README.md`. (§9.4)
9. (Optional) `Game/CMakeLists.txt:203-208`: missing web ini → `FATAL_ERROR`. (§2.3.2)
10. Web link while audio is not ported: temporarily `set(BUILD_NOPENALDRV OFF)` on web (`Source/CMakeLists.txt:70`) and guard the launcher's `AudioDevice` override on `HP2_HAS_NOPENALDRV` → `SoundDrv.NullAudioSubsystem`. (§7.1 A10)
11. `Launch.cpp` (HP2 `Source/Game/Src/Launch.cpp`, still byte-identical to the template) — compile/link blockers: `appSeconds()` is `FTime` in HP2 (main loop + pacing ring, §4.1.4, §4.4.1); define `extern "C" TCHAR GPackage[64]` instead of `THIS_PACKAGE` and set it/`appPackage()` to `Game` (§4.5.1); `appInit( TEXT("Game") … )` (§4.5.3); rename `UT_HAS_NOPENALDRV`↔`HP2_HAS_NOPENALDRV` (§2.1.3).
12. `Launch.cpp` UT99 removals: `NOPENGLESRENDERER` switch + preference entry; `IpServer.UdpServerUplink DoUplink` line. Keep everything else in §4 verbatim (EM_ASYNC_JS yield, pacing ring, `ue_exec_command`, `GWebRTCEngine`, render selection, web subsystem overrides, `ServerActors.Empty()`, `chdir("/System")` before `appInit`). (§4.3, §14 U4/U6)
13. Core POSIX: define `appUserDir()` in HP2 `Core/Src/UnUnix.cpp`, returning `""` on web. (§2.3.4, C7)
14. Core POSIX: `FFileManagerLinux::MakeDirectory`/`chmod` modes (`__S_IREAD && …` → `S_IRUSR|S_IWUSR|S_IXUSR`, `<sys/stat.h>`). (§5.5 F1)
15. NSDLDrv 433 adaptation (owned by `template-drivers.md`) keeping **every** G3 web site N1-N21 intact. (§6)
16. WebRTCDrv 433 adaptation (`DECLARE_CLASS` 4th arg) + `WEBRTC_PEER_PREFIX "hp2"`, `"HP2 Server"`. (§7.2 W1-W2)
17. Link flags: already correct in `Game/CMakeLists.txt` (§2.2) — do not touch; confirm `-pthread` at compile (§1.2.10).
18. Emitter: `targetId` fallback `hp2`, `provenance.repo` `HarryPotter2`, `toolchain.base`, `buildFlags` += `-sOFFSCREEN_FRAMEBUFFER`. (§3.3, §10.6)
19. **Milestone W1:** `cmake --preset wasm && cmake --build --preset wasm -j$(nproc)` (in `Source/`) → `build-wasm/SDLLaunch/{SDLLaunch.js,.wasm,.data,meta.json}`; `offscreen-depth24.sh` reports its 3 rewrites; export/import check with `wasm-objdump` (§11.2); `grep -c _webrtc_deferred_open SDLLaunch.js` = 0.

### Phase W2 — the engine runs in a browser up to the no-assets boundary (tier-1 smoke)

20. Adapt `tools/web-smoke.mjs` (COOP/COEP headers, hp2 Playwright path, WasmFS dir test, strict verdict). (§12.3)
21. `Core/Inc/FOutputDeviceFile.h`: stdout mirror (C1) — the "GLog to console" every host tap depends on.
22. `Core/Src/UnMisc.cpp` `appThrowf`: `[appThrowf]` printf under `__EMSCRIPTEN__` (C2); `UnLinker.cpp:168,892` name the file in `OpenFailed` (C3).
23. `Core/Src/UnUnix.cpp` `appRequestExit` web branch with `MAIN_THREAD_EM_ASM` (C4/C5).
24. `appInit` fixes: the `"default.ini"` literal + uninitialised `DTemp` (§2.3.3); `SavePath` separator `TEXT("..") PATH_SEPARATOR TEXT("Save")` (§2.3.4); port `appApplyIniOverrides` and call it after `GConfig->Init` (C8/§4.6.2); `appIsGlobalOptionToken` + `UGameEngine::Init` first-token test (C8/E2).
25. **Milestone W2:** `node tools/web-smoke.mjs build-wasm/SDLLaunch 20` → `Name subsystem initialized`, `Base directory: /System/`, `Object subsystem initialized`, render selection line = XOpenGL, `NSDLClient: …`, `LoadMap: Entry` → `FailedBrowse` (expected), no `Cd Path:`, no `Bound to UWeb.so`/`NOpenGLESDrv.so`. Same on `build-wasm-perf`. (§12.3)

### Phase W3 — loader robustness with exceptions off (before the first FetchFS boot)

26. Loader sites L1 (`SafeLoadError`), L2-L4 (`GetPackageLinker` → NULL), L6 (`StaticLoadObject` ObjectNotFound), L7-L8 (`VerifyImport` NULL linker), L10 (`LoadPackage` NULL linker — HP2 improvement), all `#ifdef __EMSCRIPTEN__` with the §5 guard shape. (§5.2)
27. Engine sites E1 (startup-map fallback — decide Entry vs `DefaultLocalMap`), E3 (`SetProgress` NULL Actor), E6 (verify-load NULL), E7 (real load NULL), E9 (`MakeDirectory(…,1)` at all 8 calls), E10 (savegame URL on Unix). (§5.3)
28. Core/Engine general fixes the web build depends on: C9 per-path case-insensitive `appFindPackageFile`; X2 tick floor 0.005→0.001; X3 iterative GC; X4 `always_inline` new/delete; X5 `FString::Mid`; X6 `DECLARE_CLASS` + layout guard; `c506ba7` native-bool bitmask; `1d3dfa5`(1) `appUnwindf` latch; `63595e5` split the three `PriorityActors[ConsiderCount] = … ConsiderCount++` (HP2 `UnLevTic.cpp:655,686,703`). (§5.4, §13)
29. Port `FLUSHCONFIG` into HP2 `UEngine::Exec` (`6525113`). (§11.2)
30. `ALAudio`-derived paths: `"..\\music\\"` → `../Music/` (and every other `TEXT("..\\…")` literal in HP2 packages). (§5.5 F2, §7.1 A5)

### Phase W4 — first real boot in the host (tier 2)

31. ufrontend clone (`hp2` branch): `targets/hp2/*` per `ufrontend-target-contract.md` §12 — `capabilities` = ut99's list (decide `indexedDb` required), `videoDrivers` without NOpenGLES, `frameRateCap.iniPath = System/Game.ini`, `manifestExclude` incl. `System/{Default,DefUser,Game,User}.ini`, `userFiles` covering `Save/**`. (§10.5, §11.4)
32. `yarn build --target hp2` → meta + wasm-limit gates pass. (§10, §11.1)
33. E2E `hp2-wasm-boot.spec.js` + `hp2.profile.js` with the agreed `logPath` (`/System/Game.log` if step 11 sets the package to `Game`). (§12.4)
34. **Milestone W4:** `Created WebGL 2.0 context on the worker` → `Bringing … up for play` → `Possessed PlayerPawn`, no `Aborted`/`[appThrowf]`; then `-UTGLRRENDERER` (WebGL 1) and `-SOFTWARERENDERER` (needs §7.3 S1 screen bound) and `-NULLRENDERER`.

### Phase W5 — audio, persistence, polish

35. HP2 NOpenALDrv from `ALAudio` on openal-soft (EFX or `EAX ON`), Vorbis decoder, stream pump on the engine thread, `UseHRTF`/`OutputRate` keys; flip `BUILD_NOPENALDRV` back ON on web; web ini `AudioDevice=NOpenALDrv…` + section; descriptor `audioHrtf`/`audioBuffer`; `meta.json` deps (drop libxmp). (§7.1, §2.7)
36. SoftDrv: `SOFTDRV_MAX_SCREEN` bound + table audit, MMX gating, fog; copy/adapt `WEB-NOTES.md`; run `softdrv-matrix.cjs` on `wasm-perf`. (§7.3)
37. Persistence E2E: config round-trip (needs step 29), save/load across reload (needs steps 24, 27 E9/E10, 31 `userFiles`). (§9.6, §12.4)
38. `wasm-debug` census (only if that preset is used). (§11.1)
39. (Optional) CI web job adapted from the template's `build.yml` (CI preset without local fork overrides, `embuilder build sdl2`, provenance check, 7-day retention). (§13.1)
40. Multiplayer (only if the owner enables `network` for hp2): L5, E4, E5, E11, `ConfiguredInternetSpeed`, `peerPrefix`, net E2E. (§14 U13)

## 16. Open questions / verify list

| # | Question | Why it matters | How to settle it |
|---|---|---|---|
| Q1 | Does HP2's SDL launcher set `GPackage`/`appPackage()` to `Game` (log `/System/Game.log`) or keep the template's `this.program` (log `/System/this.program.log`, as the ut99 E2E profile assumes)? | `LocalizeError()` default package, the log path the E2E profile reads, `ufrontend-target-contract.md` §5.3 `logPath` | decide in the launcher step (§15 #11); recommendation: `Game` (retail `Game.int` exists, localisation lookups then work); update the profile to `/System/Game.log` |
| Q2 | Startup-map failure fallback (E1): `GEntry` (`Entry.unr`) or `FURL::DefaultLocalMap` (`startup.unr`)? | a player left in an empty Entry level has no menu if HP2's front-end lives in `startup.unr` | boot natively with a bogus `LocalMap`, see what Entry shows; or read HGame's `FEBook`/`HPConsole` bring-up |
| Q3 | Is `Editor` a runtime dependency for HP2 content? | `BUILD_EDITOR` is forced ON (costs wasm size and is the `-O0` local-count risk) | `strings Assets/Maps/*.unr Assets/System/*.u \| grep -c 'Editor\.'` + a native boot with Editor unlinked |
| Q4 | Does HP2 enable WebRTC multiplayer? | decides L5/E4/E5/E11, `webrtc` capability, `ConfiguredInternetSpeed`, net E2E | owner decision; record in `targets/hp2/target.json` (`network`) and PLAN.md |
| Q5 | `[URL] Map=autoplay.unr` — no such file in `Assets/Maps/` | a URL without a map part would fail to load | grep HGame/Engine for empty-map browses (`FURL(NULL)`, `?`-only URLs); if any, set `Map=Entry.unr` in the web ini |
| Q6 | Keep retail `[HGame.baseConsole] bDebugMode=true` on web? | enables HP2 debug console paths (`HPConsole.uc` `ConsoleKey`) | owner decision |
| Q7 | Can the `Escape=quit` binding ever fire (i.e. `HPConsole` not consuming `IK_Escape`)? | it would call `appRequestExit(0)` → page returns to the launcher | test Escape in every console state (cut-scene, menu book, gameplay, dialog) on native first |
| Q8 | What does `FFeedbackContextAnsi::YesNof` return non-interactively? | `ULinkerLoad` `OldVersion` prompt (`UnLinker.cpp:212-213`) — a 0 aborts the load | read HP2 `Core/Inc/FFeedbackContextAnsi.h`; make it return 1 on web if it blocks/returns 0 |
| Q9 | Does HP2 433's `UNREAL_STATIC` registration create every `UClass` before `InitEngine`? | `IsPackageClassAvailable` uses `StaticFindObject` on web; lazy registration → "no known render device" | first tier-1 smoke (§12.3) prints the selection line; if it says "no known render device", fix registration order |
| Q10 | HP2 `FFileStream` on POSIX: pump from `UAudioSubsystem::Update()` (no thread) or a pthread? | WasmFS hooks work only on the engine thread; pool size | audio port design (§7.1 A4); default: no thread, pool stays 4 |
| Q11 | EAX: keep HP2's `EAXSet/EAXGet` calls (`EAX ON` in `ufront_openal_soft`) or map to EFX reverb? | openal-soft build option + runtime behaviour | audio port design (§3.2.1) |
| Q12 | Vorbis decoder: Emscripten ports (`-sUSE_OGG=1 -sUSE_VORBIS=1`) or FetchContent `xiph/ogg`+`xiph/vorbis` (native parity)? | `meta.json` provenance, native parity | audio port design; either works on web |
| Q13 | `indexedDb` required for HP2 (saves) instead of recommended? | a single-player game without persistent saves | owner decision in the descriptor |
| Q14 | Web `Brightness` 0.4 (HP2 retail) or 0.5 (template)? | visual parity | compare screenshots natively |
| Q15 | Confirm the `paths` rewrite lands on the **last** `Paths=` entry | §9.2's ordering rule rests on `TMap::Find` returning the last-inserted pair | after the first boot, `ueExecResult('flushconfig')` then read `/System/Game.ini` from the page (`FS.readFile`) — the sixth `Paths=` must read `../Save/*.usa` (or the slot path), the first five untouched |
| Q16 | Does HP2's own SoftDrv accept `ColorBytes=4` and a 5120-px-wide viewport? | `-SOFTWARERENDERER` on web (N10, S1) | read `Source/SoftDrv/Src/SoftDrv.cpp` `SetRes`; run the SoftDrv E2E at DPR 2 |
| Q17 | Does HP2's engine parse `-benchmark` / `SECONDS=`? | the host's `?benchmark=` URL option | `grep -rn 'BENCHMARK\|SECONDS=' Source/Engine/Src`; inert if absent (not a blocker) |
| Q18 | Save-game thumbnails: does HP2 read the framebuffer back (`UViewport::ScreenShot`, `snapBuff`)? | WebGL readback timing (before `commit_frame`) | `grep -rn ScreenShot Source/Engine/Src Source/HGame`; test a save on web |
| Q19 | Which gl4es ref is canonical for HP2 (`hp2` clone vs `fix/cubemap-bind-active-unit` default vs template's `ptitSeb@master`)? | UTGLR/WebGL1 behaviour, `meta.json` provenance | owner decision; make the CMake default = the clone's branch |
| Q20 | Does `-fpack-struct=4` give HP2 433's native classes the script sizes? | a mismatch corrupts objects at the first GC | port the `bb90b47` guard; read its warnings on the first native and web boots |
| Q21 | Is the `.github` CI wanted for HP2? | the template has linux/windows/web jobs; HP2 has none | owner decision (§13.1) |

## Status: complete

project(Game CXX)

set(SRC_FILES "Src/Launch.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNREAL_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

# Select the appropriate viewport / window driver
if(USE_SDL)
  # SDL / NSDLDrv path (POSIX/Windows with SDL2).
  # Under Emscripten SDL2 comes from the -sUSE_SDL=2 port (link flags below) and NSDLDrv from the
  # --whole-archive static set; linking NSDLDrv here as well double-defines its symbols, which wasm-ld rejects.
  if(NOT EMSCRIPTEN)
    target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY} NSDLDrv)
  endif()
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})
else()
  message(FATAL_ERROR "This port builds only the SDL2 viewport driver (USE_SDL=ON).")
endif()


if(BUILD_STATIC)
  set(LINK_PACKAGES ${INSTALL_TARGETS})
  list(REMOVE_ITEM LINK_PACKAGES ${PROJECT_NAME})
  target_link_libraries(${PROJECT_NAME} -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive)
else()
  target_link_libraries(${PROJECT_NAME} Engine Core)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE UNREAL_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

# UE_HAS_NOPENALDRV: NOpenALDrv is compiled in, so the launcher's web driver set names it as the AudioDevice
# (without it, SoundDrv's silent subsystem).
if(BUILD_NOPENALDRV)
  target_compile_definitions(${PROJECT_NAME} PRIVATE UE_HAS_NOPENALDRV=1)
endif()

# OpenAL Soft is the one library the Windows build loads rather than links (cmake/OpenALMingw.cmake): copy it
# beside the .exe, so the output directory is the runnable set.
if(OPENAL_MINGW_RUNTIME_DLL)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENAL_MINGW_RUNTIME_DLL}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${OPENAL_MINGW_RUNTIME_DLL_NAME}"
    COMMENT "Copying ${OPENAL_MINGW_RUNTIME_DLL_NAME} (OpenAL Soft) next to the executable")
endif()

# Native Linux: the bundled openal-soft (cmake/OpenALSoft.cmake NATIVE) is DT_NEEDED by NOpenALDrv, so the
# binary finds it through $ORIGIN and the build copies it there under its soname. A run tree needs
# libopenal.so.1 beside the binary; without it the loader stops before main.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT EMSCRIPTEN AND NOT ANDROID AND TARGET OpenAL)
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:OpenAL>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_SONAME_FILE_NAME:OpenAL>"
    COMMENT "Copying $<TARGET_SONAME_FILE_NAME:OpenAL> (openal-soft, the ufront fork) next to the executable")
endif()

# Web: a bare loader (SDLLaunch.{js,wasm,data}), no --shell-file. The host page supplies the heavy glue
# (FetchFS/IDB/WebRTC) at run time; only the thin shims in vendor/glue are linked in.
if(EMSCRIPTEN)
  target_link_options(${PROJECT_NAME} PRIVATE
    -sASYNCIFY
    -sASYNCIFY_STACK_SIZE=262144
    # emscripten_sleep is the main loop's per-frame sleep; __syscall_openat is listed for FetchFS's on-demand open.
    "-sASYNCIFY_IMPORTS=['emscripten_sleep','emscripten_set_main_loop','__syscall_openat']"
    # WebGL 1 and 2: NSDLViewport asks for WebGL 2 for every GL device and lets UTGLR fall back to WebGL 1.
    # GL_ENABLE_GET_PROC_ADDRESS lets glad (XOpenGL) and gl4es (UTGLR) resolve entry points. FULL_ES2 is
    # required for UTGLR: gl4es emits client-side vertex arrays, which WebGL rejects ("no ARRAY_BUFFER
    # bound"), and FULL_ES2 emulates them with temporary VBOs (XOpenGL uses real VBOs and is unaffected).
    -sMAX_WEBGL_VERSION=2
    -sMIN_WEBGL_VERSION=1
    -sGL_ENABLE_GET_PROC_ADDRESS=1
    -sFULL_ES2=1
    -sUSE_SDL=2
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=268435456
    -sSTACK_SIZE=2097152
    -sFORCE_FILESYSTEM
    # WasmFS with a JS-implemented backend: vendor/glue/wasmfs-shim.js forwards its file ops to the host's
    # Module.UE.fs.backend, which loads assets on demand and keeps their bytes in JS typed arrays rather than in
    # linear memory. The host builds the backend through the exported functions below.
    # -sFORCE_FILESYSTEM (above) stays required: WasmFS omits the JS FS.* API without it, and the host glue
    # uses that API too.
    -sWASMFS
    "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_close','_ue_exec_command','_wasmfs_create_jsimpl_backend','_wasmfs_create_file','_ufront_wasmfs_register','_ufront_wasmfs_set_pending']"
    # PThread: the host hands the FS sync channel to the worker that runs main(). Runtime methods are on
    # Module only when exported; without it the engine dies on its first asset read.
    "-sEXPORTED_RUNTIME_METHODS=['callMain','stringToUTF8','UTF8ToString','HEAPU8','stackSave','stackAlloc','stackRestore','lengthBytesUTF8','PThread']"
    # The worker-side bootstrap in wasmfs-shim.js is a __postset, emitted only when its $symbol is referenced.
    # Nothing references it, so without this it is dropped silently and the engine dies on its first asset read.
    "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['$ueWorkerBoot']"
    # The engine runs on a worker (a threaded-only artifact): synchronous restore-on-read from OPFS needs
    # FileSystemSyncAccessHandle, which exists only on workers, and Atomics.wait cannot block the browser main
    # thread. -sOFFSCREENCANVASES_TO_PTHREAD transfers #canvas to that worker, which makes NSDLViewport.cpp's
    # direct emscripten_webgl_create_context legal there. SDL2 keeps window and input; GL-context creation and
    # the present bypass SDL, whose EGL path is proxied to a main thread that cannot see the transferred canvas
    # (Emscripten #24792). XOpenGL adopts the current context; UTGLR points gl4es at Emscripten's resolver.
    -pthread
    -sPROXY_TO_PTHREAD
    -sOFFSCREENCANVAS_SUPPORT
    # Quoted: CMake treats an unquoted '#' as a comment, which truncates this to
    # "OFFSCREENCANVASES_TO_PTHREAD=" and fails the link with "string index out of range".
    "-sOFFSCREENCANVASES_TO_PTHREAD=#canvas"
    # NSDLViewport creates the context with explicitSwapControl=1 + renderViaOffscreenBackBuffer=1 and presents
    # each frame with emscripten_webgl_commit_frame(); Emscripten implements both only under
    # -sOFFSCREEN_FRAMEBUFFER. Without it the commit is a no-op stub, which leaves presentation to Chromium's
    # implicit end-of-task propagation for a transferred OffscreenCanvas, and that does not present on
    # Windows/D3D11. The cost is one blit per frame.
    -sOFFSCREEN_FRAMEBUFFER
    # main() runs on one of these; openal-soft adds two (its web backend's mixer thread and the event thread
    # every context starts). A thread created with the pool empty starts only once the main thread has spun up
    # a worker, which the audio ring would absorb as an underrun.
    -sPTHREAD_POOL_SIZE=4
    # openal-soft reports every AL error by throwing and catching it, so catching must be on (the first AL
    # error would otherwise abort). Only its objects are built with -fexceptions.
    -sNO_DISABLE_EXCEPTION_CATCHING
    # The thin shims: net-shim.js defines the ABI's ue_net_* imports (forwarding to the host's
    # Module.UE.net.*), wasmfs-shim.js the WasmFS backend and its worker bootstrap.
    "SHELL:--js-library ${CMAKE_SOURCE_DIR}/../vendor/glue/net-shim.js"
    "SHELL:--js-library ${CMAKE_SOURCE_DIR}/../vendor/glue/wasmfs-shim.js"
    # ufront_backbuffer_samples: a multisampled emulated back buffer for OpenGLDrv/UTGLR, which draws into
    # framebuffer 0.
    "SHELL:--js-library ${CMAKE_CURRENT_SOURCE_DIR}/msaa-backbuffer.js"
  )
  if(UE_WEB_PERF)
    # --emit-symbol-map writes a side file (SDLLaunch.js.symbols) and leaves the wasm unchanged; with it a
    # shipped build's crash offset resolves to a function name offline. DWARF cannot ship (it limits binaryen's
    # optimizations). The map names every private engine symbol, so it must never reach the published tree.
    target_link_options(${PROJECT_NAME} PRIVATE -O3 -g0 -sASSERTIONS=0 --emit-symbol-map)
  else()
    target_link_options(${PROJECT_NAME} PRIVATE -g -sASSERTIONS=1)
  endif()
  # Preload the boot config so appInit has it before FetchFS streams the rest. HP2 433's appInit
  # generates Game.ini/User.ini from Default.ini + DefUser.ini, so both must exist at boot.
  # They come from the tracked web set (Config/System/web), not from the asset dir: a game install's own inis
  # carry none of the web-only settings (the [WebRTCDrv.WebRTCNetDriver] section, the GL render device).
  # Config/System/*.ini (no subdir) are the game's shipped inis. The derived Game.ini/User.ini are excluded
  # from the asset manifest, so a stale install copy cannot shadow what appInit derives from these.
  foreach(_ini Default.ini DefUser.ini)
    if(EXISTS "${CMAKE_SOURCE_DIR}/../Config/System/web/${_ini}")
      set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS
        " --preload-file ${CMAKE_SOURCE_DIR}/../Config/System/web/${_ini}@/System/${_ini}")
    endif()
  endforeach()
  # The preloaded inis are inputs to the link: without LINK_DEPENDS, editing the web config leaves a
  # stale .data (cmake sees no source change and skips the relink).
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_SOURCE_DIR}/../Config/System/web/Default.ini"
    "${CMAKE_SOURCE_DIR}/../Config/System/web/DefUser.ini")
  # The --js-library shims are link inputs too, which CMake does not infer from a link option: without this,
  # editing a shim leaves a stale .js while the build reports success.
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_SOURCE_DIR}/../vendor/glue/net-shim.js"
    "${CMAKE_SOURCE_DIR}/../vendor/glue/wasmfs-shim.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/msaa-backbuffer.js")
  # Bare loader: SDLLaunch.js (+ .wasm/.data), no .html, into build-<cfg>/SDLLaunch/. `SDLLaunch` is the
  # loader basename the host loads for every engine.
  set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME SDLLaunch
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/SDLLaunch)

  # 24-bit depth for the emulated default framebuffer: Emscripten allocates -sOFFSCREEN_FRAMEBUFFER's depth as
  # GL_DEPTH_COMPONENT16 whatever the context attributes say, which z-fights in bands. offscreen-depth24.sh
  # rewrites the emitted loader to D24S8 and asserts the rewrite count, so a toolchain change fails the build.
  # POST_BUILD: it re-runs on every relink, and the publish step forces the relink (it deletes
  # SDLLaunch.{js,wasm} first).
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/offscreen-depth24.sh" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDLLaunch.js"
    COMMENT "offscreen-depth24: D24S8 for the emulated default framebuffer")

  # Emit the artifact descriptor meta.json. abiVersion is read from the vendored ABI header; provenance and
  # toolchain are best-effort. The consumer validates it (schema, ABI version, checksums, loaderStyle).
  file(STRINGS ${CMAKE_SOURCE_DIR}/../vendor/glue/ue_abi.h _ue_abi_line REGEX "define[ \t]+UE_ABI_VERSION")
  string(REGEX MATCH "([0-9]+)" UE_ABI_VERSION "${_ue_abi_line}")
  execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR}/.. rev-parse --short HEAD
    OUTPUT_VARIABLE META_GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  # The Emscripten version, from the toolchain file's root: a preset build has only $EMSDK set, so emcc may
  # not be on PATH.
  set(META_EMCC "unknown")
  if(EXISTS "${EMSCRIPTEN_ROOT_PATH}/emscripten-version.txt")
    file(READ "${EMSCRIPTEN_ROOT_PATH}/emscripten-version.txt" _emcc_ver)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" META_EMCC "${_emcc_ver}")
  endif()
  # Every fetched library as repo@branch|tag, from the cache variables the fetch itself read
  # (the web build always has NOpenALDrv, hence openal-soft and xiph ogg/vorbis; HP2 has no .umx, no libxmp).
  include(${CMAKE_SOURCE_DIR}/cmake/ArtifactDeps.cmake)
  set(META_DEPS)
  if(BUILD_NOPENALDRV)
    ufront_artifact_dep(META_DEPS openal-soft OpenALSoft "${OPENALSOFT_GIT_REPOSITORY}" "${OPENALSOFT_GIT_TAG}")
    ufront_artifact_dep(META_DEPS ogg ogg "${HP2_OGG_GIT_REPOSITORY}" "${HP2_OGG_GIT_TAG}")
    ufront_artifact_dep(META_DEPS vorbis vorbis "${HP2_VORBIS_GIT_REPOSITORY}" "${HP2_VORBIS_GIT_TAG}")
  endif()
  if(BUILD_XOPENGLDRV)
    ufront_artifact_dep(META_DEPS xopengl XOpenGLDrv "${XOPENGL_GIT_REPOSITORY}" "${XOPENGL_GIT_TAG}")
  endif()
  if(BUILD_UTGLR)
    ufront_artifact_dep(META_DEPS utglr utglr "${UTGLR_GIT_REPOSITORY}" "${UTGLR_GIT_TAG}")
    ufront_artifact_dep(META_DEPS gl4es GL4ES "${GL4ES_GIT_REPOSITORY}" "${GL4ES_GIT_TAG}" "${GL4ES_SOURCE_DIR}")
  endif()
  find_program(NODE_EXECUTABLE node REQUIRED)
  # The emitter is an input to the artifact, which CMake cannot infer from a POST_BUILD COMMAND: LINK_DEPENDS
  # makes an edit to it relink, which re-runs POST_BUILD; otherwise a stale meta.json ships.
  set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/emit-artifact-meta.mjs")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/emit-artifact-meta.mjs
      --out=$<TARGET_FILE_DIR:${PROJECT_NAME}>
      --target=${HP2_TARGET_ID} --abi=${UE_ABI_VERSION}
      --gitSha=${META_GIT_SHA} --emcc=${META_EMCC}
      ${META_DEPS}
    COMMENT "Emitting artifact meta.json (docs/05 §4)"
    VERBATIM
  )
endif()

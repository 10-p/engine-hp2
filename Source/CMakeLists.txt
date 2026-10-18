cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# HarryPotter2 (engine-hp2) — Harry Potter and the Chamber of Secrets, Unreal Engine 1 build 433.
# One static binary per preset (UNREAL_STATIC + --whole-archive over the packages' static libraries), the
# hardware render drivers fetched from their own repos, three web build modes.
project(HP2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)

# The frontend target this engine is built for (unreal-frontend-arch-private targets/<id>): meta.json's targetId,
# and the WebRTC peer prefix, which the hp2 targets set equal to their id (network.peerPrefix).
set(HP2_TARGET_ID "hp2-retail-exp")

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
  endif()
endif()

# ---------------------------------------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------------------------------------

# Web build mode (EMSCRIPTEN only) — see the three-mode block further down. UE_WEB_PERF is read by
# Game/CMakeLists.txt for the link flags; UE_WEB_DEBUG selects -O0 over the -O2 dev default.
option(UE_WEB_PERF "Emscripten: shipping build — link at -O3 -g0, assertions off" OFF)
option(UE_WEB_DEBUG "Emscripten: -O0 instead of the -O2 dev default (nothing inlined, every local live)" OFF)
option(USE_SDL "Use NSDLDrv (SDL2 window/input) — the only viewport driver this port builds" ON)
option(BUILD_GAME "Build the launcher (executable `Game`; web artifact SDLLaunch.js)" ON)
# The Editor package is built for the game too: UE1 maps and meshes can reference Editor classes, and their
# load fails without its native half. It is the no-GUI NullEdHook variant, linking only Engine+Core.
option(BUILD_EDITOR "Build the Editor package (a RUNTIME dependency — see above)" ON)
option(BUILD_UTGLR "Build OpenGLDrv (UTGLR, Dohnal's legacy-GL device)" OFF)
option(BUILD_NULLSOUNDDRV "Build SoundDrv (the null/silent audio subsystem)" ON)
option(BUILD_NOPENALDRV "Build NOpenALDrv (OpenAL SFX + Ogg Vorbis music; web + native)" OFF)
option(UE_BUNDLED_OPENAL "Native Linux: build openal-soft (the ufront fork) and ship libopenal.so.1 beside the binary, instead of linking whatever libopenal the system has" ON)
option(BUILD_STATIC "Link everything into a single binary" OFF)
# A 64-bit engine: OFF demotes an x86_64 compiler to -m32. The C++ side does not read it: PLATFORM_64BITS comes
# from the compiler (Core/Inc/Core.h), so it cannot disagree with the code being compiled.
option(UE_ALLOW_64BIT "Build x86_64 as a 64-bit engine instead of demoting to -m32" OFF)

# Hardware render devices: orthogonal per-renderer toggles. XOpenGLDrv (OldUnreal, WebGL 2/GLES3) is the web
# default; OpenGLDrv/UTGLR runs on the web through gl4es (WebGL 2, or WebGL 1 where that is all there is).
if(EMSCRIPTEN)
  set(_HP2_DEF_XOPENGL ON)
  set(_HP2_DEF_GL4ES ON)
else()
  set(_HP2_DEF_XOPENGL OFF)
  set(_HP2_DEF_GL4ES OFF)
endif()
option(BUILD_XOPENGLDRV "Build XOpenGLDrv (OldUnreal GL -> WebGL2/GLES3)" ${_HP2_DEF_XOPENGL})
option(BUILD_SOFTDRV "Build SoftDrv (the v400 software renderer with HP2's render face; -SOFTWARERENDERER)" ON)
option(BUILD_NULLDRV "Build NullDrv (GPU-free no-op render device; -NULLRENDERER)" ON)
option(USE_GL4ES "Build OpenGLDrv (UTGLR) against gl4es (desktop GL -> GLES2) for the WebGL path" ${_HP2_DEF_GL4ES})
if(USE_GL4ES)
  set(BUILD_UTGLR ON)
endif()

# ---------------------------------------------------------------------------------------------------------
# Emscripten / WebAssembly — force the web-safe configuration (the wasm presets need no extra -D flags).
# ---------------------------------------------------------------------------------------------------------
if(EMSCRIPTEN)
  set(BUILD_STATIC ON)            # single binary via UNREAL_STATIC + --whole-archive
  set(USE_SDL ON)                 # NSDLDrv (SDL2 window/GL context -> WebGL); provided by -sUSE_SDL=2
  set(BUILD_EDITOR ON)
  set(BUILD_XOPENGLDRV ON)        # forced with set(): option() only supplies a default, and an old build
  set(USE_GL4ES ON)               # tree keeps its cached value
  set(BUILD_UTGLR ON)
  set(BUILD_NULLSOUNDDRV ON)
  set(BUILD_NOPENALDRV ON)        # HP2's ALAudio on openal-soft (web audio backend) + EA-XA + Ogg Vorbis
                                  # music; the launcher sets AudioDevice=NOpenALDrv.NOpenALAudioSubsystem.
  add_definitions(-DASM=0)        # Core/Render/SoftDrv use their portable (non-x86) code paths
  # -pthread must be a COMPILE option, not just a link option: it defines __EMSCRIPTEN_PTHREADS__, which the
  # threaded code paths (NSDLViewport's direct WebGL context creation, XOpenGL's entry-point resolver,
  # appRequestExit's MAIN_THREAD_EM_ASM) are guarded on, and it gives every object — including the
  # FetchContent'd gl4es, XOpenGL and utglr trees — the atomics/bulk-memory features a shared-memory link expects.
  add_compile_options(-pthread)
  # The SDL2 port must be requested at compile time too: Core/NSDLDrv include <SDL2/SDL.h>, which exists in the
  # Emscripten cache only once the port is installed (a fresh emsdk otherwise fails with "'SDL2/SDL.h' file
  # not found").
  add_compile_options(-sUSE_SDL=2)
  # UE1 assumes Win32 /Zp4 struct packing — the reflected C++ class sizes must equal the UnrealScript
  # property sizes. wasm32 does not match by default; force it for ALL sources so every class layout is uniform.
  add_compile_options(-fpack-struct=4 -Wno-address-of-packed-member)
  # clang rejects passing a non-POD FString through variadic debugf/appErrorf as an ERROR; GCC tolerates it.
  add_compile_options(-Wno-error=non-pod-varargs -Wno-non-pod-varargs)
  # Exceptions: the engine's objects use Emscripten's default (-fignore-exceptions); recoverable load failures
  # resolve to NULL site by site, and errors abort. openal-soft is the one exception (its objects are
  # -fexceptions, cmake/OpenALSoft.cmake) and the link enables catching.
endif()

# Android: the playable client as one shared library, libHP2.so, loaded by the Gradle project's SDLActivity
# (Android/). SDL2 is fetched and built shared beside it (the Dependencies section), openal-soft, libogg and
# libvorbis link in statically, and the render devices are those that run on a GLES context: XOpenGLDrv
# (GLES 3), SoftDrv and NullDrv; OpenGLDrv/UTGLR needs desktop GL.
if(ANDROID AND USE_SDL)
  set(BUILD_STATIC ON)
  set(BUILD_EDITOR ON)            # a runtime dependency (see the option)
  set(BUILD_XOPENGLDRV ON)
  set(BUILD_UTGLR OFF)
  set(USE_GL4ES OFF)
  set(BUILD_SOFTDRV ON)
  set(BUILD_NULLDRV ON)
  set(BUILD_NULLSOUNDDRV ON)
  set(BUILD_NOPENALDRV ON)
  set(UE_ANDROID_CLIENT ON)
  # The app's own platform paths (the game folder from the bridge, the log on logcat).
  add_definitions(-DUE_ANDROID_APP=1)
  # The static packages become part of a shared library.
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
if(ANDROID)
  # The x86_64 ABI (the emulator) is a 64-bit engine, as native64 is; the -m32 demotion below is for x86 hosts.
  set(UE_ALLOW_64BIT ON)
  # The portable code paths of Core, Render and SoftDrv, as on the web.
  add_definitions(-DASM=0)
  # clang rejects a non-POD FString passed through variadic debugf/appErrorf; GCC tolerates it.
  add_compile_options(-Wno-error=non-pod-varargs -Wno-non-pod-varargs)
  # UE1 calls methods through a null pointer and tests `this` inside them; clang deletes those tests.
  add_compile_options(-fno-delete-null-pointer-checks)
endif()

if(BUILD_STATIC)
  if(MSVC)
    message(FATAL_ERROR "Static mode is not supported with MSVC")
  endif()
  set(LIB_TYPE STATIC)
  add_definitions(-DUNREAL_STATIC)
  message(STATUS "Static mode enabled")
else()
  set(LIB_TYPE SHARED)
endif()

# ---------------------------------------------------------------------------------------------------------
# Target architecture / platform
# ---------------------------------------------------------------------------------------------------------
include(TargetArch)
target_architecture(TARGET_ARCH)

set(TARGET_IS_X86 FALSE)
set(TARGET_IS_ARM FALSE)
set(TARGET_IS_64BIT FALSE)

if(TARGET_ARCH MATCHES "arm")
  set(TARGET_IS_ARM TRUE)
  if(TARGET_ARCH MATCHES "64")
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "arm64")
  else()
    set(TARGET_ARCH "arm")
  endif()
elseif((TARGET_ARCH MATCHES "86") OR (TARGET_ARCH STREQUAL "ia64"))
  set(TARGET_IS_X86 TRUE)
  if(TARGET_ARCH MATCHES "64")
    set(TARGET_IS_64BIT TRUE)
    set(TARGET_ARCH "x86_64")
  else()
    set(TARGET_ARCH "x86")
  endif()
endif()

message(STATUS "Target arch: ${TARGET_ARCH}")
message(STATUS "64-bit: ${TARGET_IS_64BIT}")

# UE1 needs a signed char (UnGnuG.h static_asserts it), and ARM's default is unsigned.
if(TARGET_IS_ARM)
  add_compile_options(-fsigned-char)
endif()

# A 64-bit engine runs the 32-bit .u packages unchanged (the script mirror contract, Core/Inc/UnObjBas.h). An
# x86_64 compiler without UE_ALLOW_64BIT is demoted to -m32; 64-bit MSVC has never been built and is refused.
if(TARGET_IS_64BIT)
  if(MSVC)
    message(FATAL_ERROR "64-bit MSVC builds are not supported. Use -A Win32.")
  elseif(TARGET_IS_X86 AND NOT UE_ALLOW_64BIT AND NOT ANDROID)
    # (Android takes the width of its ABI: its x86_64 is a 64-bit engine like arm64.)
    message(STATUS "Building x86 binary with x86_64 compiler (pass -DUE_ALLOW_64BIT=ON for x86_64)")
    set(TARGET_IS_64BIT FALSE)
    set(TARGET_ARCH "x86")
    add_compile_options(-m32)
  else()
    message(STATUS "Building a 64-bit engine for ${TARGET_ARCH}")
  endif()
endif()

set(TARGET_IS_WINDOWS FALSE)
set(TARGET_IS_UNIX FALSE)
if(WIN32)
  set(TARGET_IS_WINDOWS TRUE)
else()
  set(TARGET_IS_UNIX TRUE)
endif()
message(STATUS "Windows target: ${TARGET_IS_WINDOWS}")

if(TARGET_IS_X86)
  add_definitions(-DPLATFORM_X86)
endif()
add_definitions(-DPLATFORM_LITTLE_ENDIAN)

if(TARGET_IS_WINDOWS)
  add_definitions(-DPLATFORM_WIN32)
  add_definitions(-DWIN32)
  add_definitions(-DWINDOWS_IGNORE_PACKING_MISMATCH)
  if(MINGW)
    # Link GCC's runtime statically: without this the .exe imports libgcc_s_dw2-1.dll and libstdc++-6.dll,
    # which live in the toolchain, not in a Windows install.
    add_link_options(-static-libgcc -static-libstdc++)
    # A posix-thread-model MinGW (MSYS2, Arch/Fedora/Ubuntu's -posix packages) also pulls in libwinpthread-1.dll through
    # libstdc++; link it statically too, or the exe does not start outside the build machine (wine: exit 53).
    add_link_options(-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
    # UE1 assumes Win32 /Zp4 struct packing; MinGW follows the MSVC ABI (double/int64 aligned to 8), so without
    # this every class with a double comes out too long. Applied to all sources: packing must be uniform. Not on
    # x86_64, where it would repack the Win64 API's structs too: there the script-mirrored headers pack
    # themselves (#pragma pack(push,4), Core/Inc/UnObjBas.h).
    if(NOT TARGET_IS_64BIT)
      add_compile_options(-fpack-struct=4)
    endif()
    # Windows puts in libraries what glibc keeps in libc: sockets are ws2_32, CoCreateGuid is ole32.
    link_libraries(ws2_32 ole32)
  endif()
else()
  add_definitions(-DPLATFORM_POSIX)
endif()

if(USE_SDL)
  add_definitions(-DPLATFORM_SDL)
endif()

# ---------------------------------------------------------------------------------------------------------
# Common compiler options
# ---------------------------------------------------------------------------------------------------------
if(MSVC)
  message(STATUS "Building ${CMAKE_BUILD_TYPE} with MSVC")
  add_definitions(-DPLATFORM_MSVC)
  add_compile_options(/nologo /fp:precise /analyze- /MP /MD /Gd /Gy /GS /Gm- /EHsc /Zi /Zc:wchar_t /Zc:inline /Zp4)
  add_compile_options(/wd4291 /wd4297)
  set(MSVC_LIBDIR "Win32")
else()
  message(STATUS "Building ${CMAKE_BUILD_TYPE} with GCC or Clang")
  if(EMSCRIPTEN)
    # The three web build modes:
    #   preset        UE_WEB_PERF  UE_WEB_DEBUG  compile   what it is for
    #   wasm-perf     ON            -              -O2       shipping: link-time -O3 -g0, no assertions
    #   wasm          OFF           OFF            -O2 -g    the dev default: named stacks, DWARF, assertions
    #   wasm-debug    OFF           ON             -O0 -g    last resort: every local live, nothing inlined
    if(UE_WEB_DEBUG)
      add_compile_options(-O0 -g)
    elseif(UE_WEB_PERF)
      add_compile_options(-O2)
    else()
      add_compile_options(-O2 -g)
    endif()
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Og -g)
    add_link_options(-g)
  else()
    add_compile_options(-O2)
  endif()
  add_compile_options(-ffunction-sections -fno-strict-aliasing)
  add_compile_options(-Wno-write-strings -Wno-terminate -Wno-address-of-packed-member)
  # GCC: keep the stores a destructor makes to its own object. UE1 destroys native config arrays and strings
  # twice (UObject::ExitProperties, then the C++ destructor), which is safe only because ~FArray leaves
  # Data/ArrayNum/ArrayMax zeroed; GCC's lifetime dead-store elimination may delete those stores.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fno-lifetime-dse)
  endif()
endif()

# ---------------------------------------------------------------------------------------------------------
# Dependencies: SDL2
# ---------------------------------------------------------------------------------------------------------
if(USE_SDL)
  if(MSVC)
    if(NOT DEFINED SDL2_INCLUDE_DIR)
      set(SDL2_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../Thirdparty/SDL2/include")
    endif()
    if(NOT DEFINED SDL2_LIBRARY)
      set(SDL2_LIBRARY "${CMAKE_SOURCE_DIR}/../Thirdparty/SDL2/lib/${MSVC_LIBDIR}/SDL2.lib")
    endif()
  else()
    if(NOT DEFINED SDL2_INCLUDE_DIR)
      set(SDL2_INCLUDE_DIR )
    endif()
    if(NOT DEFINED SDL2_LIBRARY)
      if(EMSCRIPTEN)
        # The -sUSE_SDL=2 PORT supplies SDL2 (and picks libSDL2-mt.a under -pthread). An explicit -lSDL2 on
        # top resolves to the NON-threaded archive and the link dies on a shared-memory mismatch.
        set(SDL2_LIBRARY )
      elseif(ANDROID)
        # SDL2 from its source, built shared (libSDL2.so: SDLActivity loads it before the game library and its
        # JNI lives in it). The Gradle project passes FETCHCONTENT_SOURCE_DIR_SDL2, the checkout whose Java it
        # compiles, so the C and Java sides are one version. The target carries its include directories.
        include(FetchContent)
        set(SDL2_GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git" CACHE STRING "SDL2 git repo")
        set(SDL2_GIT_TAG "release-2.32.x" CACHE STRING "SDL2 git tag/branch/commit")
        message(STATUS "SDL2: fetching ${SDL2_GIT_REPOSITORY}@${SDL2_GIT_TAG}")
        FetchContent_Declare(SDL2 GIT_REPOSITORY "${SDL2_GIT_REPOSITORY}" GIT_TAG "${SDL2_GIT_TAG}" GIT_SHALLOW TRUE)
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL2)
        set(SDL2_LIBRARY SDL2)
      elseif(MINGW AND CMAKE_CROSSCOMPILING)
        include(SDL2Mingw)
      else()
        set(SDL2_LIBRARY "-lSDL2")
      endif()
    endif()
  endif()
endif()

# ---------------------------------------------------------------------------------------------------------
# Render driver sources — fetched from their own repos (the 10-p forks), never vendored here. The engine
# keeps only each driver's build-glue CMakeLists.txt (XOpenGLDrv/, OpenGLDrv/). The branch named here is
# fetched; set FETCHCONTENT_SOURCE_DIR_XOPENGLDRV / _UTGLR to a local clone to build an unpushed change.
# ---------------------------------------------------------------------------------------------------------
if(BUILD_XOPENGLDRV)
  set(XOPENGL_GIT_REPOSITORY "https://github.com/10-p/XOpenGLDrv.git" CACHE STRING "XOpenGLDrv git repo (OldUnreal fork)")
  set(XOPENGL_GIT_TAG "ufront" CACHE STRING "XOpenGLDrv git tag/branch/commit")
  include(FetchContent)
  message(STATUS "XOpenGLDrv: fetching ${XOPENGL_GIT_REPOSITORY}@${XOPENGL_GIT_TAG}")
  FetchContent_Declare(XOpenGLDrv
    GIT_REPOSITORY "${XOPENGL_GIT_REPOSITORY}" GIT_TAG "${XOPENGL_GIT_TAG}" GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(XOpenGLDrv)
  set(XOPENGL_SOURCE_DIR "${xopengldrv_SOURCE_DIR}")
  message(STATUS "XOpenGLDrv: source = ${XOPENGL_SOURCE_DIR}")
endif()

if(BUILD_UTGLR)
  set(UTGLR_GIT_REPOSITORY "https://github.com/10-p/utglr-copy.git" CACHE STRING "OpenGLDrv/UTGLR git repo (Dohnal drop + our port)")
  set(UTGLR_GIT_TAG "main" CACHE STRING "OpenGLDrv/UTGLR git tag/branch/commit")
  include(FetchContent)
  message(STATUS "OpenGLDrv/UTGLR: fetching ${UTGLR_GIT_REPOSITORY}@${UTGLR_GIT_TAG}")
  FetchContent_Declare(utglr
    GIT_REPOSITORY "${UTGLR_GIT_REPOSITORY}" GIT_TAG "${UTGLR_GIT_TAG}" GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(utglr)
  set(UTGLR_SOURCE_DIR "${utglr_SOURCE_DIR}")
  message(STATUS "OpenGLDrv/UTGLR: source = ${UTGLR_SOURCE_DIR}")
endif()

# ---------------------------------------------------------------------------------------------------------
# Subprojects
# ---------------------------------------------------------------------------------------------------------
set(INSTALL_TARGETS Core Engine Render IpDrv Fire)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_subdirectory(Core)
add_subdirectory(Engine)
add_subdirectory(Render)
add_subdirectory(IpDrv)
add_subdirectory(Fire)
# ucc — the command-line tool (commandlets such as `make`), on Linux and Windows. There is no web or Android
# ucc.
if(NOT EMSCRIPTEN AND NOT UE_ANDROID_CLIENT)
  add_subdirectory(UCC)
endif()

# The WebRTC net driver is a web-only package (its transport is the glue's ue_net_* imports, linked in via
# vendor/glue/net-shim.js, not BSD sockets). The launcher makes it the web's NetworkDevice.
if(EMSCRIPTEN)
  add_subdirectory(WebRTCDrv)
  list(APPEND INSTALL_TARGETS WebRTCDrv)
endif()

if(USE_SDL)
  add_subdirectory(NSDLDrv)
  list(APPEND INSTALL_TARGETS NSDLDrv)
else()
  message(FATAL_ERROR "This port builds only the SDL2 viewport driver (USE_SDL=ON).")
endif()

if(BUILD_UTGLR)
  # gl4es (desktop GL -> GLES2) for the OpenGLDrv/UTGLR WebGL path. Fetched when USE_GL4ES (native gl4es
  # validation) or EMSCRIPTEN (the WebGL target). Source configurable via GL4ES_SOURCE_DIR.
  if(USE_GL4ES OR EMSCRIPTEN)
    set(GL4ES_SOURCE_DIR "" CACHE PATH "Local gl4es checkout (vanilla or fork). Empty => fetch from git.")
    set(GL4ES_GIT_REPOSITORY "https://github.com/10-p/gl4es.git" CACHE STRING "gl4es git repo (the 10-p fork)")
    set(GL4ES_GIT_TAG "ufront" CACHE STRING "gl4es git tag/branch/commit to fetch")
    include(FetchContent)
    if(GL4ES_SOURCE_DIR)
      message(STATUS "gl4es: using local source ${GL4ES_SOURCE_DIR}")
      FetchContent_Declare(GL4ES SOURCE_DIR "${GL4ES_SOURCE_DIR}")
    else()
      message(STATUS "gl4es: fetching ${GL4ES_GIT_REPOSITORY}@${GL4ES_GIT_TAG}")
      FetchContent_Declare(GL4ES GIT_REPOSITORY "${GL4ES_GIT_REPOSITORY}" GIT_TAG "${GL4ES_GIT_TAG}" GIT_SHALLOW TRUE)
    endif()
    # Static lib, GLES2 target, no X11/EGL (NSDLDrv owns the context), explicit init.
    set(NOX11 ON CACHE BOOL "" FORCE)
    set(NOEGL ON CACHE BOOL "" FORCE)
    set(STATICLIB ON CACHE BOOL "" FORCE)
    set(DEFAULT_ES 2 CACHE STRING "" FORCE)
    set(NO_INIT_CONSTRUCTOR ON CACHE BOOL "" FORCE)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)  # gl4es' old cmake_minimum_required vs CMake 4.x
    FetchContent_MakeAvailable(GL4ES)
    unset(CMAKE_POLICY_VERSION_MINIMUM)
    if(NOT TARGET GL)
      message(FATAL_ERROR "gl4es: expected static target 'GL' was not created.")
    endif()
    set_target_properties(GL PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    if(NOT TARGET gl4es::GL)
      add_library(gl4es::GL ALIAS GL)
    endif()
  endif()
  add_subdirectory(OpenGLDrv)
  list(APPEND INSTALL_TARGETS OpenGLDrv)
endif()

if(BUILD_XOPENGLDRV)
  add_subdirectory(XOpenGLDrv)
  list(APPEND INSTALL_TARGETS XOpenGLDrv)
endif()

if(BUILD_SOFTDRV)
  add_subdirectory(SoftDrv)
  list(APPEND INSTALL_TARGETS SoftDrv)
endif()

if(BUILD_NULLDRV)
  add_subdirectory(NullDrv)
  list(APPEND INSTALL_TARGETS NullDrv)
endif()

if(BUILD_NULLSOUNDDRV)
  add_subdirectory(SoundDrv)
  list(APPEND INSTALL_TARGETS SoundDrv)
endif()

if(BUILD_NOPENALDRV)
  if(EMSCRIPTEN)
    include(OpenALSoft)
    ufront_openal_soft(PLATFORM WEB EAX OFF)
  elseif(ANDROID)
    # Android links the same fork statically into the game library, playing through OpenSL ES.
    include(OpenALSoft)
    ufront_openal_soft(PLATFORM ANDROID EAX OFF)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND UE_BUNDLED_OPENAL)
    include(OpenALSoft)
    ufront_openal_soft(PLATFORM NATIVE EAX OFF)
  endif()
  add_subdirectory(NOpenALDrv)
  list(APPEND INSTALL_TARGETS NOpenALDrv)
endif()

if(BUILD_EDITOR)
  add_subdirectory(Editor)
  list(APPEND INSTALL_TARGETS Editor)
endif()

if(BUILD_GAME)
  add_subdirectory(Game)
  list(APPEND INSTALL_TARGETS Game)
endif()

# The Android bridge: a library of its own that GameActivity loads first to set the game folder, HOME and the
# working directory before SDLActivity starts SDL_main (Android/app/src/main/cpp/android_bridge.cpp).
if(UE_ANDROID_CLIENT)
  add_library(hp2_android_bridge SHARED "${CMAKE_SOURCE_DIR}/../Android/app/src/main/cpp/android_bridge.cpp")
  set_target_properties(hp2_android_bridge PROPERTIES PREFIX "lib")
  target_link_libraries(hp2_android_bridge log)
endif()

# assemble all modules in one dir
set(INSTALL_FILES )
set(INSTALL_PROGRAMS )
foreach(TGT ${INSTALL_TARGETS})
  get_target_property(TGT_TYPE ${TGT} TYPE)
  if(TGT_TYPE STREQUAL "EXECUTABLE")
    list(APPEND INSTALL_PROGRAMS "$<TARGET_FILE:${TGT}>")
  else()
    list(APPEND INSTALL_FILES "$<TARGET_FILE:${TGT}>")
  endif()
endforeach()
if(INSTALL_PROGRAMS)
  install(PROGRAMS ${INSTALL_PROGRAMS} DESTINATION "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
endif()
if(INSTALL_FILES)
  install(FILES ${INSTALL_FILES} DESTINATION "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")
endif()

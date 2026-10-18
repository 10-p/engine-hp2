cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# HarryPotter2 (engine-hp2) — Harry Potter and the Chamber of Secrets, Unreal Engine 1 build 433.
# One static binary per preset (UNREAL_STATIC + --whole-archive over the packages' static libraries), the
# hardware render drivers fetched from their own repos, three web build modes.
project(HP2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
  endif()
endif()

# ---------------------------------------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------------------------------------

# Web build mode (EMSCRIPTEN only) — see the three-mode block further down. HP2_WEB_PERF is read by
# Game/CMakeLists.txt for the link flags; HP2_WEB_DEBUG selects -O0 over the -O2 dev default.
option(HP2_WEB_PERF "Emscripten: shipping build — link at -O3 -g0, assertions off" OFF)
option(HP2_WEB_DEBUG "Emscripten: -O0 instead of the -O2 dev default (nothing inlined, every local live)" OFF)
option(USE_SDL "Use NSDLDrv (SDL2 window/input) — the only viewport driver this port builds" ON)
option(BUILD_GAME "Build the launcher (executable `Game`; web artifact SDLLaunch.js)" ON)
# The Editor package is built for the game too: UE1 maps and meshes can reference Editor classes, and their
# load fails without its native half. It is the no-GUI NullEdHook variant, linking only Engine+Core.
option(BUILD_EDITOR "Build the Editor package (a RUNTIME dependency — see above)" ON)
option(BUILD_NOPENGLDRV "Build OpenGLDrv (UTGLR, Dohnal's legacy-GL device)" OFF)
option(BUILD_NULLSOUNDDRV "Build SoundDrv (the null/silent audio subsystem)" ON)
option(BUILD_NOPENALDRV "Build NOpenALDrv (OpenAL SFX + Ogg Vorbis music; web + native)" OFF)
option(HP2_BUNDLED_OPENAL "Native Linux: build openal-soft (the ufront fork) and ship libopenal.so.1 beside the binary, instead of linking whatever libopenal the system has" ON)
option(BUILD_STATIC "Link everything into a single binary" OFF)

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
option(BUILD_SOFTDRV "Build SoftDrv (HP2's own CPU software renderer; -SOFTWARERENDERER)" ON)
option(BUILD_NULLDRV "Build NullDrv (GPU-free no-op render device; -NULLRENDERER)" ON)
option(USE_GL4ES "Build OpenGLDrv (UTGLR) against gl4es (desktop GL -> GLES2) for the WebGL path" ${_HP2_DEF_GL4ES})
if(USE_GL4ES)
  set(BUILD_NOPENGLDRV ON)
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
  set(BUILD_NOPENGLDRV ON)
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

# UE1 is 32-bit by construction (pointer-sized fields inside reflected structs). Demote an x86_64 compiler.
if(TARGET_IS_64BIT)
  if(TARGET_IS_X86 AND NOT MSVC)
    message(STATUS "Building x86 binary with x86_64 compiler")
    set(TARGET_IS_64BIT FALSE)
    set(TARGET_ARCH "x86")
    add_compile_options(-m32)
  else()
    message(FATAL_ERROR "64-bit platforms are not supported. If you're building on Windows with MSVC, try -A Win32.")
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

if(TARGET_IS_64BIT)
  add_definitions(-DPLATFORM_64BIT)
else()
  add_definitions(-DPLATFORM_32BIT)
endif()
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
    # this every class with a double comes out too long. Applied to all sources: packing must be uniform.
    add_compile_options(-fpack-struct=4 -Wno-address-of-packed-member)
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
    #   preset        HP2_WEB_PERF  HP2_WEB_DEBUG  compile   what it is for
    #   wasm-perf     ON            -              -O2       shipping: link-time -O3 -g0, no assertions
    #   wasm          OFF           OFF            -O2 -g    the dev default: named stacks, DWARF, assertions
    #   wasm-debug    OFF           ON             -O0 -g    last resort: every local live, nothing inlined
    if(HP2_WEB_DEBUG)
      add_compile_options(-O0 -g)
    elseif(HP2_WEB_PERF)
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
  add_compile_options(-Wno-write-strings -Wno-terminate)
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
  set(XOPENGL_GIT_TAG "hp2-v433-compat" CACHE STRING "XOpenGLDrv git tag/branch/commit")
  include(FetchContent)
  message(STATUS "XOpenGLDrv: fetching ${XOPENGL_GIT_REPOSITORY}@${XOPENGL_GIT_TAG}")
  FetchContent_Declare(XOpenGLDrv
    GIT_REPOSITORY "${XOPENGL_GIT_REPOSITORY}" GIT_TAG "${XOPENGL_GIT_TAG}" GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(XOpenGLDrv)
  set(XOPENGL_SOURCE_DIR "${xopengldrv_SOURCE_DIR}")
  message(STATUS "XOpenGLDrv: source = ${XOPENGL_SOURCE_DIR}")
endif()

if(BUILD_NOPENGLDRV)
  set(UTGLR_GIT_REPOSITORY "https://github.com/10-p/utglr-copy.git" CACHE STRING "OpenGLDrv/UTGLR git repo (Dohnal drop + our port)")
  set(UTGLR_GIT_TAG "hp2" CACHE STRING "OpenGLDrv/UTGLR git tag/branch/commit")
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
# ucc — the command-line tool (commandlets such as `make`). Native Linux only: there is no Windows or web ucc.
if(NOT EMSCRIPTEN AND NOT MINGW)
  add_subdirectory(UCC)
endif()

# The WebRTC net driver is a web-only package (its transport is the glue's webrtc_* ABI, linked in via
# vendor/glue/webrtc-shim.js, not BSD sockets). The launcher makes it the web's NetworkDevice.
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

if(BUILD_NOPENGLDRV)
  # gl4es (desktop GL -> GLES2) for the OpenGLDrv/UTGLR WebGL path. Fetched when USE_GL4ES (native gl4es
  # validation) or EMSCRIPTEN (the WebGL target). Source configurable via GL4ES_SOURCE_DIR.
  if(USE_GL4ES OR EMSCRIPTEN)
    set(GL4ES_SOURCE_DIR "" CACHE PATH "Local gl4es checkout (vanilla or fork). Empty => fetch from git.")
    set(GL4ES_GIT_REPOSITORY "https://github.com/10-p/gl4es.git" CACHE STRING "gl4es git repo (the 10-p fork)")
    set(GL4ES_GIT_TAG "fix/cubemap-bind-active-unit" CACHE STRING "gl4es git tag/branch/commit to fetch")
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
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND HP2_BUNDLED_OPENAL)
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

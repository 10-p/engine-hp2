project(NOpenALDrv C CXX)

# HP2 433 (PLAN Phase E): HP2's retail ALAudio ported to openal-soft on the template's pattern (see the header of
# NOpenALDrv.cpp). SFX: OpenAL; EA-XA sounds through the in-tree EAXADecoder.cpp; music: Ogg Vorbis files opened
# by path through libvorbisfile. No libxmp (HP2 has no .umx), no coda, no MpegLib.
set(SRC_FILES
  "NOpenALDrv.cpp"
  "EAXADecoder.cpp"
)

# ---------------------------------------------------------------------------------------------------------------
# libogg + libvorbis(file): FetchContent'd static libraries on every target, the upstream release tags (§11.2
# A5 / M5-3; cmake/ArtifactDeps.cmake records them in meta.json). Built WITHOUT the engine's -fpack-struct=4:
# OggVorbis_File has ogg_int64_t/double members, 8-aligned on wasm32, and libvorbisfile must lay it out the way
# the C ABI says. The driver includes the headers under #pragma pack(push,8) and Init() compares sizeof() with
# VorbisLayout.c below, compiled the library's way.
# ---------------------------------------------------------------------------------------------------------------
include(FetchContent)
set(HP2_OGG_GIT_REPOSITORY    "https://github.com/xiph/ogg.git"    CACHE STRING "libogg git repo")
set(HP2_OGG_GIT_TAG           "v1.3.5"                             CACHE STRING "libogg release tag")
set(HP2_VORBIS_GIT_REPOSITORY "https://github.com/xiph/vorbis.git" CACHE STRING "libvorbis git repo")
set(HP2_VORBIS_GIT_TAG        "v1.3.7"                             CACHE STRING "libvorbis release tag")
FetchContent_Declare(ogg    GIT_REPOSITORY "${HP2_OGG_GIT_REPOSITORY}"    GIT_TAG "${HP2_OGG_GIT_TAG}"    GIT_SHALLOW TRUE)
FetchContent_Declare(vorbis GIT_REPOSITORY "${HP2_VORBIS_GIT_REPOSITORY}" GIT_TAG "${HP2_VORBIS_GIT_TAG}" GIT_SHALLOW TRUE)
# Their options, as cache entries (both projects predate CMP0077, so a normal variable would be ignored). None of
# these names is one the engine uses.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)
set(INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)   # their old cmake_minimum_required vs CMake 4.x
FetchContent_MakeAvailable(ogg)
# libvorbis looks for Ogg with its FindOgg module; point it at the tree just configured (the Ogg::ogg ALIAS target
# ogg defines is what actually gets linked).
set(OGG_INCLUDE_DIR "${ogg_SOURCE_DIR}/include" CACHE PATH "" FORCE)
set(OGG_LIBRARY ogg CACHE STRING "" FORCE)
FetchContent_MakeAvailable(vorbis)
unset(CMAKE_POLICY_VERSION_MINIMUM)
foreach(_lib ogg vorbis vorbisfile)
  if(NOT TARGET ${_lib})
    message(FATAL_ERROR "NOpenALDrv: expected target '${_lib}' from xiph ogg/vorbis was not created")
  endif()
endforeach()

# The layout probe: libvorbisfile's own sizeof(OggVorbis_File).
add_library(NOpenALDrvVorbisLayout STATIC VorbisLayout.c)
target_link_libraries(NOpenALDrvVorbisLayout PRIVATE vorbisfile)

# Strip the engine's directory-level packing (and its companion warning switch) from every target of both trees,
# subdirectories included (the walk cmake/OpenALSoft.cmake does for openal-soft), and from the probe.
function(hp2_strip_engine_packing)
  foreach(_t ${ARGN})
    get_target_property(_type ${_t} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY" OR _type STREQUAL "UTILITY")
      continue()
    endif()
    get_target_property(_opts ${_t} COMPILE_OPTIONS)
    if(_opts)
      list(REMOVE_ITEM _opts -fpack-struct=4 -Wno-address-of-packed-member)
      set_target_properties(${_t} PROPERTIES COMPILE_OPTIONS "${_opts}")
    endif()
    # Static archives that may end up in a shared NOpenALDrv (a non-BUILD_STATIC native tree).
    set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endforeach()
endfunction()
set(_xiph_dirs "${ogg_SOURCE_DIR}" "${vorbis_SOURCE_DIR}")
set(_xiph_targets NOpenALDrvVorbisLayout)
while(_xiph_dirs)
  list(POP_FRONT _xiph_dirs _dir)
  get_property(_subdirs DIRECTORY "${_dir}" PROPERTY SUBDIRECTORIES)
  list(APPEND _xiph_dirs ${_subdirs})
  get_property(_dir_targets DIRECTORY "${_dir}" PROPERTY BUILDSYSTEM_TARGETS)
  list(APPEND _xiph_targets ${_dir_targets})
endwhile()
hp2_strip_engine_packing(${_xiph_targets})
message(STATUS "NOpenALDrv: xiph ogg ${HP2_OGG_GIT_TAG} + vorbis ${HP2_VORBIS_GIT_TAG} (static, natural packing)")

# ---------------------------------------------------------------------------------------------------------------
# OpenAL (template, unchanged).
# ---------------------------------------------------------------------------------------------------------------
if(EMSCRIPTEN)
  # ufront 2.61: openal-soft, built by the parent (cmake/OpenALSoft.cmake WEB) — was Emscripten's built-in
  # -lopenal. Linking the target also gives this driver openal-soft's <AL/alext.h>/<AL/efx.h>.
  set(OPENAL_LIBRARY OpenAL)
elseif(TARGET OpenAL)
  # ufront 2.70: native Linux — the parent built the same fork SHARED (cmake/OpenALSoft.cmake NATIVE); the
  # launcher copies libopenal.so.1 beside the binary and links with $ORIGIN, so this DT_NEEDED resolves there.
  set(OPENAL_LIBRARY OpenAL)
elseif(MINGW AND CMAKE_CROSSCOMPILING)
  # ufront 2.34 — cross-building for Windows: no system OpenAL, so fetch OpenAL Soft's official prebuilt
  # Windows binaries (same pattern as SDL2Mingw). See cmake/OpenALMingw.cmake for why this one library is
  # loaded rather than linked.
  include(OpenALMingw)
else()
  # Native (Linux) without HP2_BUNDLED_OPENAL: the system OpenAL (openal-soft).
  find_package(OpenAL)
  if(NOT OPENAL_LIBRARY)
    set(OPENAL_LIBRARY "-lopenal")
  endif()
endif()

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
  ${OPENAL_INCLUDE_DIR}
)

if(MSVC)
  # openal-soft release libs are built without safeseh for some reason?
  target_link_options(${PROJECT_NAME} PRIVATE "/SAFESEH:NO")
endif()

target_link_libraries(${PROJECT_NAME} ${OPENAL_LIBRARY} vorbisfile vorbis ogg NOpenALDrvVorbisLayout)

target_link_libraries(${PROJECT_NAME} Engine Render Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOPENALDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

# ---------------------------------------------------------------------------------------------------------------
# The EA-XA decoder's acceptance test (Test/EAXAOracle.cpp, template-drivers.md §11.4). NOT built by default and
# never run by the build; native only (it reads the retail Assets/Sounds, read-only):
#   cmake --build <native-tree> --target eaxa_oracle && <native-tree>/NOpenALDrv/eaxa_oracle Assets/Sounds
# ---------------------------------------------------------------------------------------------------------------
if(NOT EMSCRIPTEN AND NOT CMAKE_CROSSCOMPILING)
  add_executable(eaxa_oracle EXCLUDE_FROM_ALL Test/EAXAOracle.cpp EAXADecoder.cpp)
endif()

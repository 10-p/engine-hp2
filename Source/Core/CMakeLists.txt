project(Core CXX)

# HP2 build 433 Core: Core.dsp's list minus UnPSX2.cpp (PS2), UnVcWin32.cpp (MSVC only, below) and
# UnCoreNative.cpp (its whole body is `#if __STATIC_LINK`, never defined; static builds use UnStaticExports.cpp).
set(SRC_FILES
  "Src/UnProp.cpp"
  "Src/UnObj.cpp"
  "Src/UnLinker.cpp"
  "Src/UnCorSc.cpp"
  "Src/UnName.cpp"
  "Src/UnMem.cpp"
  "Src/UnMisc.cpp"
  "Src/UnMath.cpp"
  "Src/UnCoreNet.cpp"
  "Src/UnClass.cpp"
  "Src/UnCache.cpp"
  "Src/UnBits.cpp"
  "Src/UnAnsi.cpp"
  "Src/Core.cpp"
  "Src/UFactory.cpp"
  "Src/UExporter.cpp"
)

# The platform layer follows the compiler, not the target OS: MSVC gets Epic's UnVcWin32.cpp; every GCC/Clang
# target (Linux, web, MinGW) gets UnUnix.cpp, with the few Win32-only calls in UnMinGW.cpp.
if(MSVC)
  list(APPEND SRC_FILES "Src/UnVcWin32.cpp")
else()
  # Source order is link order: UnGUID/UnMinGW before UnUnix.
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

if(NOT MSVC AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
  # The reference juggling UE does with FFrame::Step() confuses gcc at higher optimisation levels, so the
  # core UnrealScript interpreter is built at -Og.
  set_source_files_properties("Src/UnCorSc.cpp" PROPERTIES COMPILE_FLAGS -Og)
  set_source_files_properties("Src/UnProp.cpp" PROPERTIES COMPILE_FLAGS -Og)
endif()

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE CORE_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

if(USE_SDL)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARY})
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})
endif()

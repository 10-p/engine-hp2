project(Core CXX)

# HP2 build 433 Core. The list follows Core.dsp minus the PS2 layer (UnPSX2.cpp), the Windows core
# (UnVcWin32.cpp, MSVC only) and UnCoreNative.cpp (the Loki/PS2 __STATIC_LINK native lookup table — its
# whole body is `#if __STATIC_LINK`, which this port never defines: the template's UnStaticExports registry
# is the static-link mechanism). UnLinker.cpp is the 433 split of the package linker out of UnObj.cpp.
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

# The platform layer is chosen by COMPILER, not by target OS (the template's ufront 2.33 lesson): MSVC gets
# Epic's Win32 core; every GCC/Clang target keeps the portable UnUnix.cpp that web and Linux exercise, with
# the handful of genuinely Win32 calls in UnMinGW.cpp beside it.
if(MSVC)
  list(APPEND SRC_FILES "Src/UnVcWin32.cpp")
else()
  # ORDER: UnGUID/UnMinGW before UnUnix, matching the template list (source order is link order).
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
  # core UnrealScript interpreter is lowered to -Og (inherited from ut99dc; deliberate, see the template).
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

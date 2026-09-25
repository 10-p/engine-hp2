#=============================================================================
# SoftDrv - HP2's own software render device (CPU rasterizer, the 433 URenderDevice face: DrawTriangles /
# MaxVertices / DrawComplexSurface with PolyFlags+cAlpha). Its x86 __asm rasterization carries portable
# #if !ASM paths, so it builds with -DASM=0 the way the template's SoftDrv does. Pure software (no
# glad/SDL GL), so it coexists with XOpenGLDrv on web. Selected via -SOFTWARERENDERER.
#=============================================================================

project(SoftDrv CXX)

set(SRC_FILES
  "Src/SoftDrv.cpp"
  "Src/DrawPoly.cpp"
  "Src/DrawSurf.cpp"
  "Src/DrawTile.cpp"
  "Src/Line2D.cpp"
  "Src/Hit.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
  PRIVATE
  # Render.h / UnRender.h come from ../Render (HP2 keeps the private render header in Render/Inc).
  ${CMAKE_CURRENT_SOURCE_DIR}/../Render/Inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../Render/Src
)

target_link_libraries(${PROJECT_NAME} Render Engine Core)

# ASM=0: the portable (non-x86) rasterization paths + the portable no-MMX path.
target_compile_definitions(${PROJECT_NAME} PRIVATE SOFTDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME} ASM=0)

# The SizeX/SizeY-indexed scratch tables are STATIC arrays bounded by this (the template's 2.19f/2.71
# finding: the web canvas is the display's DEVICE pixel size, so the wasm preset raises it to 8192).
set(SOFTDRV_MAX_SCREEN "4096" CACHE STRING "Max software-renderer viewport dimension (static table bound)")
target_compile_definitions(${PROJECT_NAME} PRIVATE SOFTDRV_MAX_SCREEN=${SOFTDRV_MAX_SCREEN})

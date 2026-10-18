#=============================================================================
# SoftDrv - the v400 software renderer (CPU rasterizer) with HP2's 433 render face (DrawTriangles /
# MaxVertices / DrawComplexSurface with PolyFlags+cAlpha). Built with -DASM=0: the portable #if !ASM paths
# replace its x86 __asm rasterization. Pure software (no GL), so it links beside the GL devices.
# Selected via -SOFTWARERENDERER.
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

# The SizeX/SizeY-indexed scratch tables are static arrays bounded by this; the wasm preset raises it to
# 8192 because the web canvas is the display's device-pixel size.
set(SOFTDRV_MAX_SCREEN "4096" CACHE STRING "Max software-renderer viewport dimension (static table bound)")
target_compile_definitions(${PROJECT_NAME} PRIVATE SOFTDRV_MAX_SCREEN=${SOFTDRV_MAX_SCREEN})

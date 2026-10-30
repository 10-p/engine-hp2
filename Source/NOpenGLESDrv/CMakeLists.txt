#=============================================================================
# NOpenGLESDrv - the GLES2 render device with HP2's 433 render face (DrawTriangles / MaxVertices /
# DrawComplexSurface with PolyFlags+cAlpha). WebGL 2 on the web, GLES 2 on Android, the desktop GL context
# natively. Selected by -NOPENGLESRENDERER.
#=============================================================================

project(NOpenGLESDrv C CXX)

if(NOT USE_SDL)
  message(FATAL_ERROR "NOpenGLESDrv requires NSDLDrv to function.")
endif()

set(SRC_FILES "NOpenGLESDrv.cpp")
# The vendored glad is native only: on the web Emscripten links the GLES2 entry points directly (-sFULL_ES2).
if(NOT EMSCRIPTEN)
  list(APPEND SRC_FILES "${CMAKE_SOURCE_DIR}/../Thirdparty/glad_es/glad.c")
endif()

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

# FSceneNode / FSurfaceInfo / FTransTexture come from Engine's UnRender.h (433 has no RenderPrivate.h).
target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
  ${CMAKE_SOURCE_DIR}/../Thirdparty/glad_es
  ${CMAKE_SOURCE_DIR}/../Thirdparty
  ${SDL2_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})

target_link_libraries(${PROJECT_NAME} Render Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOPENGLESDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

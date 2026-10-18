project(XOpenGLDrv C CXX)

# XOpenGLDrv — the OldUnreal OpenGL renderer (WebGL 2 / GLES3 on the web), fetched from the 10-p fork. Its UE1
# adaptations are gated on XOGL_UE1_V4XX (the v400-family engines, HP2 433 included) and XOGL_HP2 (433's
# render-device face). It creates no GL context of its own: it adopts the one NSDLViewport makes current, so
# it needs SDL/NSDLDrv.
if(NOT USE_SDL)
  message(FATAL_ERROR "XOpenGLDrv requires NSDLDrv (SDL) to function.")
endif()

# All Src/*.cpp are standalone translation units (incl. the *_GLSL.cpp shader files and the vendored
# desktop-GL glad.cpp loader). Source is fetched (${XOPENGL_SOURCE_DIR}, set in the top-level
# CMakeLists.txt); only this build glue lives in the engine tree.
file(GLOB SRC_FILES "${XOPENGL_SOURCE_DIR}/Src/*.cpp")

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

# Isolated include set: the driver vendors its own desktop-GL `glad.h`/`glext.h` (Inc/) and glm (glm/), from
# the fetched source (${XOPENGL_SOURCE_DIR}). Render.h comes from Render/Inc through the Render target.
target_include_directories(${PROJECT_NAME}
  PRIVATE
  ${XOPENGL_SOURCE_DIR}/Inc
  ${XOPENGL_SOURCE_DIR}/glm
  ${SDL2_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../Render/Src
)

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})
target_link_libraries(${PROJECT_NAME} Render Engine Core)

# UPACKAGE_NAME drives the static-build package-symbol mangling (THIS_PACKAGE / GLoaded<Pkg>); the
# token must stay identical to IMPLEMENT_PACKAGE(XOpenGLDrv), the .so basename, the .int, and the ini
# RenderDevice=XOpenGLDrv.XOpenGLRenderDevice.
#
# SDLBUILD/SDL2BUILD: the fork's XOpenGLDrv.h picks its SDL build flavor from `__LINUX__`/`MACOSX`,
# which this engine does not define — so force SDL2 here (the header's auto-detect block then no-ops).
target_compile_definitions(${PROJECT_NAME} PRIVATE
  XOPENGLDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME}
  SDLBUILD=1 SDL2BUILD=1 SDL3BUILD=0)

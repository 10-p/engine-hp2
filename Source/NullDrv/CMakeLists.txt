#=============================================================================
# NullDrv - null render device (GPU-free). A no-op URenderDevice with the 433 face: no GL context, no
# rasterization, draws nothing. Selected by -NULLRENDERER for a headless run; the NSDL viewport gives it a
# software SDL_Texture surface because the device name lacks "OPENGL".
#=============================================================================

project(NullDrv CXX)

set(SRC_FILES
  "Src/NullDrv.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)
# FSceneNode / FSurfaceInfo / FSpanBuffer come from Engine's UnRender.h (433 has no RenderPrivate.h).

target_link_libraries(${PROJECT_NAME} Render Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE NULLDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

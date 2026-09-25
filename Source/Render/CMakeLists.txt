project(Render CXX)

# HP2 build 433 Render: Render.dsp's list. UnParticleRn.cpp is the 433 particle renderer; UnTest.cpp is the
# stock test harness the dsp always compiled.
set(SRC_FILES
  "Src/Render.cpp"
  "Src/UnLight.cpp"
  "Src/UnMeshRn.cpp"
  "Src/UnParticleRn.cpp"
  "Src/UnRandom.cpp"
  "Src/UnRender.cpp"
  "Src/UnSoftLn.cpp"
  "Src/UnSpan.cpp"
  "Src/UnSprite.cpp"
  "Src/UnTest.cpp"
)

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE RENDER_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})
